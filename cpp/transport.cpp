#include "transport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lmparticle {
namespace {
const double kPi = 3.14159265358979323846;
const double kInf = std::numeric_limits<double>::infinity();
const double kNudge = 1e-8;  // cm pushed past a boundary so the next location is unambiguous

uint64_t splitmix(uint64_t &x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

// Per-history accumulation with touched-index tracking: mean and standard error of per-history sums.
struct Tally {
  std::vector<double> cur, sum, sum2;
  std::vector<char> touched_flag;
  std::vector<size_t> touched;
  explicit Tally(size_t n = 0) : cur(n), sum(n), sum2(n), touched_flag(n) {}
  void add(size_t i, double x) {
    if (!touched_flag[i]) { touched_flag[i] = 1; touched.push_back(i); }
    cur[i] += x;
  }
  void end_history() {
    for (size_t i : touched) { sum[i] += cur[i]; sum2[i] += cur[i] * cur[i]; cur[i] = 0; touched_flag[i] = 0; }
    touched.clear();
  }
  Stat stat(size_t i, int64_t n) const {
    Stat s;
    s.mean = sum[i] / n;
    double var = n > 1 ? std::max(0.0, (sum2[i] / n - s.mean * s.mean) / (n - 1)) : 0.0;
    s.err = std::sqrt(var);
    return s;
  }
};

struct Photon { double x[3], d[3], e; bool uncollided; };

void rotate(double d[3], double cost, double phi) {
  double sint = std::sqrt(std::max(0.0, 1 - cost * cost)), cp = std::cos(phi), sp = std::sin(phi);
  double u = d[0], v = d[1], w = d[2];
  if (std::fabs(w) > 0.99999) {
    double sg = w > 0 ? 1.0 : -1.0;
    d[0] = sint * cp;
    d[1] = sg * sint * sp;
    d[2] = sg * cost;
    return;
  }
  double q = std::sqrt(1 - w * w);
  d[0] = u * cost + sint * (u * w * cp - v * sp) / q;
  d[1] = v * cost + sint * (v * w * cp + u * sp) / q;
  d[2] = w * cost - sint * q * cp;
  double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (int i = 0; i < 3; i++) d[i] /= n;
}

// Distance along the ray at which it enters a box it is currently outside; kInf if it never does.
double entry_distance(const Box &b, const double x[3], const double d[3]) {
  double tmin = 0, tmax = kInf;
  for (int a = 0; a < 3; a++) {
    if (std::fabs(d[a]) < 1e-300) {
      if (x[a] < b.lo[a] || x[a] >= b.hi[a]) return kInf;
      continue;
    }
    double t1 = (b.lo[a] - x[a]) / d[a], t2 = (b.hi[a] - x[a]) / d[a];
    if (t1 > t2) std::swap(t1, t2);
    tmin = std::max(tmin, t1);
    tmax = std::min(tmax, t2);
    if (tmin >= tmax) return kInf;
  }
  return tmin;  // 0 when the ray starts on a face (x == hi counts as outside) and points into the box
}
// Distance to leave the box the ray is inside.
double exit_distance(const Box &b, const double x[3], const double d[3]) {
  double t = kInf;
  for (int a = 0; a < 3; a++) {
    if (std::fabs(d[a]) < 1e-300) continue;
    t = std::min(t, d[a] > 0 ? (b.hi[a] - x[a]) / d[a] : (b.lo[a] - x[a]) / d[a]);
  }
  return t;
}
bool inside(const Box &b, const double x[3]) {
  for (int a = 0; a < 3; a++) if (x[a] < b.lo[a] || x[a] >= b.hi[a]) return false;
  return true;
}

// Rayleigh scattering angle: Thomson (1 + cos^2)/2 shape times a Thomas-Fermi screened form factor squared,
// F(q) = 1/(1 + (q a)^2), a = 0.468 A / Z^(1/3), q = 2 k sin(theta/2) in 1/A. Approximate (the tables fix the total
// cross section; only the forward-peaked character of the angle matters). With s = q^2 the screened part has a
// closed-form inverse CDF, and the Thomson factor is applied by rejection.
double sample_rayleigh_cos(double e, int z, Rng &rng) {
  double a2 = std::pow(0.468 / std::cbrt(static_cast<double>(z)), 2);
  double k = e / 0.00197327;  // wave number in 1/A: E / (hbar c) with hbar c = 0.00197327 MeV A
  double smax = 4 * k * k;
  double span = 1.0 - 1.0 / (1.0 + a2 * smax);
  for (int it = 0; it < 100000; it++) {
    double s = (1.0 / (1.0 - rng.uniform() * span) - 1.0) / a2;
    double c = 1 - s / (2 * k * k);
    if (c < -1) c = -1;
    if (rng.uniform() < 0.5 * (1 + c * c)) return c;
  }
  return 1.0;
}

}  // namespace

Rng::Rng(uint64_t seed) {
  uint64_t x = seed;
  for (auto &w : s) w = splitmix(x);
}
uint64_t Rng::next() {
  uint64_t result = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
  s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 45);
  return result;
}
double Rng::uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

// Klein-Nishina sampling of E'/E (decomposition of Nelson, Hirayama and Rogers, EGS4; as in Geant4's G4KleinNishinaCompton).
double sample_compton(double e, Rng &rng, double *cos_theta) {
  double a = e / kElectronMassMeV;
  double eps0 = 1.0 / (1 + 2 * a), eps0sq = eps0 * eps0;
  double alpha1 = -std::log(eps0), alpha2 = alpha1 + 0.5 * (1 - eps0sq);
  double eps, epssq, onecost;
  for (;;) {
    if (alpha1 / alpha2 > rng.uniform()) {
      eps = std::exp(-alpha1 * rng.uniform());
      epssq = eps * eps;
    } else {
      epssq = eps0sq + (1 - eps0sq) * rng.uniform();
      eps = std::sqrt(epssq);
    }
    onecost = (1 - eps) / (eps * a);
    double sint2 = onecost * (2 - onecost);
    double reject = 1 - eps * sint2 / (1 + epssq);
    if (reject >= rng.uniform()) break;
  }
  *cos_theta = std::max(-1.0, 1 - onecost);
  return eps * e;
}

Result run(const Geometry &geo, const Source &src, const Config &cfg, int64_t histories) {
  if (histories < 1) throw std::runtime_error("run: histories must be at least 1");
  if (geo.boxes.empty()) throw std::runtime_error("run: the geometry has no boxes");
  if (src.lines.empty()) throw std::runtime_error("run: the source has no energy (use energy() or line())");
  double wsum = 0, emax_src = 0, esrc = 0;
  for (auto &l : src.lines) {
    if (!(l.first >= std::max(kMinEnergy, cfg.cutoff) && l.first <= kMaxEnergy))
      throw std::runtime_error("run: source energy " + std::to_string(l.first) + " MeV must lie between the cutoff and 20 MeV");
    wsum += l.second;
    emax_src = std::max(emax_src, l.first);
    esrc += l.second * l.first;
  }
  if (!(wsum > 0) || !std::isfinite(wsum)) throw std::runtime_error("run: source line weights must be positive and finite");
  esrc = 0;  // realized mean source energy, accumulated below
  const double emax = cfg.emax > 0 ? cfg.emax : emax_src * 1.0000001;
  const int bins = cfg.bins;
  const size_t nbox = geo.boxes.size(), npl = cfg.planes.size();
  Tally dep(nbox), tot(1), pulse(cfg.pulse_regions.size() * bins), pcount(npl), punc(npl), pen(npl), pspec(npl * bins), lspec(bins), leak(3), proc(kChannels);
  Rng rng(cfg.seed);
  std::vector<Photon> stack;
  auto deposit = [&](int r, double x) { dep.add(r, x); tot.add(0, x); };
  // Spectrum bin of an energy; -1 above the upper edge (not tallied; emax itself, up to rounding, is the last bin).
  auto bin_of = [&](double e) { return e > emax * (1 + 1e-9) ? -1 : std::min(bins - 1, static_cast<int>(e / emax * bins)); };

  for (int64_t h = 0; h < histories; h++) {
    Photon p;
    double u = rng.uniform() * wsum, acc = 0;
    p.e = src.lines.back().first;
    for (auto &l : src.lines) { acc += l.second; if (u < acc) { p.e = l.first; break; } }
    for (int i = 0; i < 3; i++) p.x[i] = src.pos[i];
    if (src.isotropic) {
      p.d[2] = 2 * rng.uniform() - 1;
      double s = std::sqrt(std::max(0.0, 1 - p.d[2] * p.d[2])), phi = 2 * kPi * rng.uniform();
      p.d[0] = s * std::cos(phi);
      p.d[1] = s * std::sin(phi);
    } else {
      for (int i = 0; i < 3; i++) p.d[i] = src.dir[i];
    }
    p.uncollided = true;
    esrc += p.e;
    stack.assign(1, p);
    int64_t guard = 0;
    while (!stack.empty()) {
      Photon ph = stack.back();
      stack.pop_back();
      bool alive = true;
      auto advance = [&](double dist) {  // move, scoring forward plane crossings on the way
        for (size_t k = 0; k < npl; k++) {
          const Plane &pl = cfg.planes[k];
          double dv = ph.d[pl.axis];
          if (dv <= 0) continue;
          double t = (pl.position - ph.x[pl.axis]) / dv;
          if (t > 0 && t <= dist) {
            pcount.add(k, 1);
            if (ph.uncollided) punc.add(k, 1);
            pen.add(k, ph.e);
            if (bin_of(ph.e) >= 0) pspec.add(k * bins + bin_of(ph.e), 1);
          }
        }
        for (int a = 0; a < 3; a++) ph.x[a] += dist * ph.d[a];
      };
      while (alive) {
        if (++guard > 100000000) throw std::runtime_error("run: a history exceeded 1e8 steps");
        if (ph.e < cfg.cutoff) {  // below the cutoff: absorbed on the spot
          int r = -1;
          for (size_t b = 0; b < nbox; b++) if (inside(geo.boxes[b], ph.x)) { r = static_cast<int>(b); break; }
          if (r >= 0) deposit(r, ph.e);
          else { leak.add(0, 1); leak.add(2, ph.e); if (bin_of(ph.e) >= 0) lspec.add(bin_of(ph.e), 1); }
          alive = false;
          break;
        }
        int r = -1;
        for (size_t b = 0; b < nbox; b++) if (inside(geo.boxes[b], ph.x)) { r = static_cast<int>(b); break; }
        if (r < 0) {
          double t = kInf;
          for (size_t b = 0; b < nbox; b++) t = std::min(t, entry_distance(geo.boxes[b], ph.x, ph.d));
          if (t == kInf) {  // leaves the geometry for good
            advance(kInf);
            leak.add(0, 1);
            if (ph.uncollided) leak.add(1, 1);
            leak.add(2, ph.e);
            if (bin_of(ph.e) >= 0) lspec.add(bin_of(ph.e), 1);
            alive = false;
          } else {
            advance(t + kNudge);
          }
          continue;
        }
        const Box &box = geo.boxes[r];
        Channels mu = box.material->mu(ph.e);
        if (!cfg.rayleigh) mu[kRayleigh] = 0;
        double mt = mu[0] + mu[1] + mu[2] + mu[3];
        double dint = -std::log(1 - rng.uniform()) / mt;
        double db = exit_distance(box, ph.x, ph.d);
        for (int b = 0; b < r; b++) db = std::min(db, entry_distance(geo.boxes[b], ph.x, ph.d));
        if (dint >= db) { advance(db + kNudge); continue; }
        advance(dint);
        double xi = rng.uniform() * mt;
        int ch = 0;
        while (ch < 3 && xi >= mu[ch]) { xi -= mu[ch]; ch++; }
        proc.add(ch, 1);
        ph.uncollided = false;
        if (ch == kPhoto) {
          deposit(r, ph.e);
          alive = false;
        } else if (ch == kCompton) {
          double c;
          double e2 = sample_compton(ph.e, rng, &c);
          deposit(r, ph.e - e2);
          rotate(ph.d, c, 2 * kPi * rng.uniform());
          ph.e = e2;
        } else if (ch == kRayleigh) {
          int el = box.material->pick_rayleigh_element(ph.e, rng.uniform());
          rotate(ph.d, sample_rayleigh_cos(ph.e, box.material->element_z(el), rng), 2 * kPi * rng.uniform());
        } else {  // pair production: kinetic energy deposited locally, positron annihilates at rest into two photons
          deposit(r, ph.e - 2 * kElectronMassMeV);
          double cz = 2 * rng.uniform() - 1, phi = 2 * kPi * rng.uniform(), s = std::sqrt(std::max(0.0, 1 - cz * cz));
          Photon a = ph;
          a.e = kElectronMassMeV;
          a.d[0] = s * std::cos(phi); a.d[1] = s * std::sin(phi); a.d[2] = cz;
          Photon b = a;
          for (int i = 0; i < 3; i++) b.d[i] = -a.d[i];
          a.uncollided = b.uncollided = false;
          stack.push_back(a);
          stack.push_back(b);
          alive = false;
        }
      }
    }
    for (size_t k = 0; k < cfg.pulse_regions.size(); k++) {
      double x = dep.cur[cfg.pulse_regions[k]];
      if (x > 0 && bin_of(x) >= 0) pulse.add(k * bins + bin_of(x), 1);
    }
    pulse.end_history(); tot.end_history(); dep.end_history(); pcount.end_history(); punc.end_history(); pen.end_history(); pspec.end_history();
    lspec.end_history(); leak.end_history(); proc.end_history();
  }

  Result res;
  res.histories = histories;
  res.source_energy = esrc / histories;
  res.bins = bins;
  res.emax = emax;
  for (size_t b = 0; b < nbox; b++) { res.region_names.push_back(geo.boxes[b].name); res.deposited.push_back(dep.stat(b, histories)); }
  res.total_deposited = tot.stat(0, histories);
  res.escaped = leak.stat(0, histories);
  res.escaped_uncollided = leak.stat(1, histories);
  res.escaped_energy = leak.stat(2, histories);
  for (size_t k = 0; k < npl; k++) {
    res.plane_names.push_back(cfg.planes[k].name);
    res.plane_count.push_back(pcount.stat(k, histories));
    res.plane_uncollided.push_back(punc.stat(k, histories));
    res.plane_energy.push_back(pen.stat(k, histories));
    std::vector<Stat> sp;
    for (int b = 0; b < bins; b++) sp.push_back(pspec.stat(k * bins + b, histories));
    res.plane_spectrum.push_back(std::move(sp));
  }
  for (size_t k = 0; k < cfg.pulse_regions.size(); k++) {
    res.pulse_names.push_back(geo.boxes[cfg.pulse_regions[k]].name);
    std::vector<Stat> ph;
    for (int b = 0; b < bins; b++) ph.push_back(pulse.stat(k * bins + b, histories));
    res.pulse_height.push_back(std::move(ph));
  }
  for (int b = 0; b < bins; b++) res.leak_spectrum.push_back(lspec.stat(b, histories));
  for (int c = 0; c < kChannels; c++) res.interactions[c] = proc.stat(c, histories);
  return res;
}

}  // namespace lmparticle
