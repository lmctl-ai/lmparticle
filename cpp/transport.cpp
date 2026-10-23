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

struct Photon { double x[3], d[3], e, w; bool uncollided, secondary; };

// Where a ray meets a region's surface: distance along the ray and which surface (0-2: the face normal to that
// axis, 3: the curved surface of a sphere or cylinder). kind -1 and t = kInf: never.
struct Hit { double t = kInf; int kind = -1; };

// The parameter interval of a ray inside the slab lo <= x < hi along one axis; false if the ray misses it.
bool slab_interval(double lo, double hi, double x, double d, double &t1, double &t2) {
  if (std::fabs(d) < 1e-300) {
    if (x < lo || x >= hi) return false;
    t1 = -kInf; t2 = kInf;
    return true;
  }
  t1 = (lo - x) / d; t2 = (hi - x) / d;
  if (t1 > t2) std::swap(t1, t2);
  return true;
}
// Radial interval of a ray inside the circle (sphere: n = 3 coordinates, cylinder: n = 2) of radius r about c.
bool radial_interval(const Box &b, const double x[3], const double d[3], int n, double &t1, double &t2) {
  double a = 0, bb = 0, cc = -b.radius * b.radius;
  for (int i = 0; i < n; i++) { double p = x[i] - b.center[i]; a += d[i] * d[i]; bb += p * d[i]; cc += p * p; }
  if (a < 1e-300) {  // parallel to the axis: inside the circle for ever or never
    if (cc >= 0) return false;
    t1 = -kInf; t2 = kInf;
    return true;
  }
  bb /= a; cc /= a;
  double disc = bb * bb - cc;
  if (disc <= 0) return false;
  double s = std::sqrt(disc);
  t1 = -bb - s; t2 = -bb + s;
  return true;
}

// Distance at which the ray enters a region it is currently outside (kind: the surface it enters through).
Hit entry_hit(const Box &b, const double x[3], const double d[3]) {
  double tmin = 0, tmax = kInf, t1, t2;
  int kind = -1;
  auto merge = [&](bool ok, int k) {
    if (!ok) return false;
    if (t1 >= tmin) { tmin = t1; kind = k; }
    tmax = std::min(tmax, t2);
    return tmin < tmax;
  };
  if (b.shape == kSphereShape) {
    if (!merge(radial_interval(b, x, d, 3, t1, t2), 3)) return Hit{};
  } else {
    for (int a = 0; a < 3; a++) {
      if (b.shape == kCylinderShape && a < 2) continue;
      if (!merge(slab_interval(b.lo[a], b.hi[a], x[a], d[a], t1, t2), a)) return Hit{};
    }
    if (b.shape == kCylinderShape && !merge(radial_interval(b, x, d, 2, t1, t2), 3)) return Hit{};
  }
  return Hit{tmin, kind};  // 0 when the ray starts on a face (x == hi counts as outside) and points into the region
}
// Distance to leave the region the ray is inside (kind: the surface it leaves through).
Hit exit_hit(const Box &b, const double x[3], const double d[3]) {
  Hit h;
  double t1, t2;
  auto take = [&](bool ok, int k) { if (ok && t2 < h.t) { h.t = t2; h.kind = k; } };
  if (b.shape == kSphereShape) {
    take(radial_interval(b, x, d, 3, t1, t2), 3);
  } else {
    for (int a = 0; a < 3; a++) {
      if (b.shape == kCylinderShape && a < 2) continue;
      take(slab_interval(b.lo[a], b.hi[a], x[a], d[a], t1, t2), a);
    }
    if (b.shape == kCylinderShape) take(radial_interval(b, x, d, 2, t1, t2), 3);
  }
  return h;
}
// After moving onto the surface named by the hit, place the photon unambiguously on the far side of it: the hit
// coordinate is snapped to the face and pushed kNudge across (a push along the direction alone can be lost to
// rounding when the ray meets the face at a shallow angle, which used to stall the history).
void cross(const Box &b, Photon &ph, const Hit &h, bool leaving) {
  if (h.kind < 0) {
    for (int a = 0; a < 3; a++) ph.x[a] += kNudge * ph.d[a];
    return;
  }
  if (h.kind < 3) {
    int a = h.kind;
    bool forward = ph.d[a] > 0;
    double face = (leaving == forward) ? b.hi[a] : b.lo[a];
    ph.x[a] = face + (forward ? kNudge : -kNudge);
    return;
  }
  int n = b.shape == kSphereShape ? 3 : 2;
  double len = 0;
  for (int i = 0; i < n; i++) len += (ph.x[i] - b.center[i]) * (ph.x[i] - b.center[i]);
  len = std::sqrt(len);
  double want = b.radius + (leaving ? kNudge : -kNudge);
  if (len < 1e-300) { ph.x[0] = b.center[0] + want; return; }
  for (int i = 0; i < n; i++) ph.x[i] = b.center[i] + (ph.x[i] - b.center[i]) * want / len;
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

// A unit vector uniform in the cone of half-angle acos(cone_cos) about axis.
void sample_cone(const double axis[3], double cone_cos, Rng &rng, double d[3]) {
  for (int i = 0; i < 3; i++) d[i] = axis[i];
  if (cone_cos >= 1) return;
  double c = 1 - rng.uniform() * (1 - cone_cos);
  rotate(d, c, 2 * kPi * rng.uniform());
}

}  // namespace

bool inside(const Box &b, const double x[3]) {
  if (b.shape == kSphereShape) {
    double r2 = 0;
    for (int i = 0; i < 3; i++) r2 += (x[i] - b.center[i]) * (x[i] - b.center[i]);
    return r2 < b.radius * b.radius;
  }
  if (b.shape == kCylinderShape) {
    if (x[2] < b.lo[2] || x[2] >= b.hi[2]) return false;
    double r2 = 0;
    for (int i = 0; i < 2; i++) r2 += (x[i] - b.center[i]) * (x[i] - b.center[i]);
    return r2 < b.radius * b.radius;
  }
  for (int a = 0; a < 3; a++) if (x[a] < b.lo[a] || x[a] >= b.hi[a]) return false;
  return true;
}

// Deflect d by the polar angle theta (cos given) and azimuth phi about d. The general formula divides by the
// transverse length q = sqrt(u^2 + v^2); only when q is below 1e-10 (d within 1e-10 rad of the z axis) is the axis
// itself used instead, so the direction error of the shortcut is at most 1e-10 rad.
void rotate(double d[3], double cost, double phi) {
  double sint = std::sqrt(std::max(0.0, 1 - cost * cost)), cp = std::cos(phi), sp = std::sin(phi);
  double u = d[0], v = d[1], w = d[2];
  double q = std::sqrt(u * u + v * v);
  if (q < 1e-10) {
    double sg = w > 0 ? 1.0 : -1.0;
    d[0] = sint * cp;
    d[1] = sg * sint * sp;
    d[2] = sg * cost;
    return;
  }
  d[0] = u * cost + sint * (u * w * cp - v * sp) / q;
  d[1] = v * cost + sint * (v * w * cp + u * sp) / q;
  d[2] = w * cost - sint * q * cp;
  double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (int i = 0; i < 3; i++) d[i] /= n;
}

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
  if (geo.boxes.empty()) throw std::runtime_error("run: the geometry has no regions");
  if (src.lines.empty()) throw std::runtime_error("run: the source has no energy (use energy() or line())");
  if (cfg.weight_cutoff > 0 && !cfg.pulse_regions.empty())
    throw std::runtime_error("run: pulse height spectra need analog transport (no implicit capture)");
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
  const bool implicit = cfg.weight_cutoff > 0;
  Tally dep(nbox), depu(nbox), tot(1), pulse(cfg.pulse_regions.size() * bins), pcount(npl), punc(npl), pen(npl), pspec(npl * bins), lspec(bins), leak(3), proc(kChannels);
  Rng rng(cfg.seed);
  std::vector<Photon> stack;
  Result res;
  // Spectrum bin of an energy; -1 above the upper edge (not tallied; emax itself, up to rounding, is the last bin).
  auto bin_of = [&](double e) { return e > emax * (1 + 1e-9) ? -1 : std::min(bins - 1, static_cast<int>(e / emax * bins)); };
  // Two perpendicular unit vectors to the beam axis, for disc sources.
  double ax1[3] = {0, 0, 0}, ax2[3] = {0, 0, 0};
  if (src.disc_radius > 0) {
    static const double zaxis[3] = {0, 0, 1};
    const double *n = src.isotropic ? zaxis : src.dir;
    int k = std::fabs(n[0]) < 0.6 ? 0 : (std::fabs(n[1]) < 0.6 ? 1 : 2);  // an axis not parallel to n
    double e[3] = {0, 0, 0};
    e[k] = 1;
    ax1[0] = n[1] * e[2] - n[2] * e[1]; ax1[1] = n[2] * e[0] - n[0] * e[2]; ax1[2] = n[0] * e[1] - n[1] * e[0];
    double l = std::sqrt(ax1[0] * ax1[0] + ax1[1] * ax1[1] + ax1[2] * ax1[2]);
    for (double &v : ax1) v /= l;
    ax2[0] = n[1] * ax1[2] - n[2] * ax1[1]; ax2[1] = n[2] * ax1[0] - n[0] * ax1[2]; ax2[2] = n[0] * ax1[1] - n[1] * ax1[0];
  }

  for (int64_t h = 0; h < histories; h++) {
    const bool tracking = h < cfg.track_histories;
    Photon p;
    double u = rng.uniform() * wsum, acc = 0;
    p.e = src.lines.back().first;
    for (auto &l : src.lines) { acc += l.second; if (u < acc) { p.e = l.first; break; } }
    for (int i = 0; i < 3; i++) p.x[i] = src.pos[i];
    if (src.disc_radius > 0) {
      double rr = src.disc_radius * std::sqrt(rng.uniform()), ph = 2 * kPi * rng.uniform();
      for (int i = 0; i < 3; i++) p.x[i] += rr * (std::cos(ph) * ax1[i] + std::sin(ph) * ax2[i]);
    }
    if (src.isotropic) {
      p.d[2] = 2 * rng.uniform() - 1;
      double s = std::sqrt(std::max(0.0, 1 - p.d[2] * p.d[2])), phi = 2 * kPi * rng.uniform();
      p.d[0] = s * std::cos(phi);
      p.d[1] = s * std::sin(phi);
    } else {
      sample_cone(src.dir, src.cone_cos, rng, p.d);
    }
    p.uncollided = true;
    p.secondary = false;
    p.w = 1;
    esrc += p.e;
    stack.assign(1, p);
    int64_t guard = 0;
    while (!stack.empty()) {
      Photon ph = stack.back();
      stack.pop_back();
      bool alive = true;
      auto record = [&](const char *event) {
        if (!tracking) return;
        TrackPoint t;
        t.history = h;
        for (int i = 0; i < 3; i++) { t.x[i] = ph.x[i]; t.d[i] = ph.d[i]; }
        t.e = alive ? ph.e : 0;
        t.weight = ph.w;
        t.event = event;
        res.tracks.push_back(t);
      };
      auto deposit = [&](int r, double x) {
        dep.add(r, x * ph.w);
        tot.add(0, x * ph.w);
        if (ph.uncollided) depu.add(r, x * ph.w);
      };
      auto advance = [&](double dist) {  // move, scoring forward plane crossings on the way
        for (size_t k = 0; k < npl; k++) {
          const Plane &pl = cfg.planes[k];
          double dv = ph.d[pl.axis];
          if (dv <= 0) continue;
          double t = (pl.position - ph.x[pl.axis]) / dv;
          if (t > 0 && t <= dist) {
            pcount.add(k, ph.w);
            if (ph.uncollided) punc.add(k, ph.w);
            pen.add(k, ph.e * ph.w);
            if (bin_of(ph.e) >= 0) pspec.add(k * bins + bin_of(ph.e), ph.w);
          }
        }
        for (int a = 0; a < 3; a++) ph.x[a] += dist * ph.d[a];
      };
      auto escape = [&]() {
        leak.add(0, ph.w);
        if (ph.uncollided) leak.add(1, ph.w);
        leak.add(2, ph.e * ph.w);
        if (bin_of(ph.e) >= 0) lspec.add(bin_of(ph.e), ph.w);
        record("escape");
        alive = false;
      };
      record(ph.secondary ? "annihilation" : "source");
      while (alive) {
        if (++guard > 100000000) throw std::runtime_error("run: a history exceeded 1e8 steps");
        if (ph.e < cfg.cutoff) {  // below the cutoff: absorbed on the spot
          int r = -1;
          for (size_t b = 0; b < nbox; b++) if (inside(geo.boxes[b], ph.x)) { r = static_cast<int>(b); break; }
          if (r >= 0) { deposit(r, ph.e); alive = false; record("cutoff"); }
          else escape();
          break;
        }
        int r = -1;
        for (size_t b = 0; b < nbox; b++) if (inside(geo.boxes[b], ph.x)) { r = static_cast<int>(b); break; }
        if (r < 0) {
          Hit best;
          size_t into = 0;
          for (size_t b = 0; b < nbox; b++) {
            Hit hb = entry_hit(geo.boxes[b], ph.x, ph.d);
            if (hb.t < best.t) { best = hb; into = b; }
          }
          if (best.t == kInf) {  // leaves the geometry for good
            advance(kInf);
            escape();
          } else {
            advance(best.t);
            cross(geo.boxes[into], ph, best, false);
          }
          continue;
        }
        const Box &box = geo.boxes[r];
        Channels mu = box.material->mu(ph.e);
        if (!cfg.rayleigh) mu[kRayleigh] = 0;
        double mt = mu[0] + mu[1] + mu[2] + mu[3];
        double dint = -std::log(1 - rng.uniform()) / mt;
        Hit hb = exit_hit(box, ph.x, ph.d);
        bool leaving = true;
        size_t into = r;
        for (int b = 0; b < r; b++) {
          Hit he = entry_hit(geo.boxes[b], ph.x, ph.d);
          if (he.t < hb.t) { hb = he; leaving = false; into = b; }
        }
        if (dint >= hb.t) {
          advance(hb.t);
          cross(geo.boxes[into], ph, hb, leaving);
          continue;
        }
        advance(dint);
        // Energy handed to electrons here is tallied as deposited; while ph.uncollided it also counts as
        // first-collision (uncollided-photon) deposit, whichever process it comes from.
        int ch;
        if (implicit) {  // the photoelectric share of the collision is absorbed as weight; the photon always scatters
          double pp = mu[kPhoto] / mt;
          deposit(r, ph.e * pp);
          proc.add(kPhoto, ph.w * pp);
          ph.w *= 1 - pp;
          double xi = rng.uniform() * (mt - mu[kPhoto]);
          ch = xi < mu[kRayleigh] ? kRayleigh : (xi < mu[kRayleigh] + mu[kCompton] ? kCompton : kPair);
        } else {
          double xi = rng.uniform() * mt;
          ch = 0;
          while (ch < 3 && xi >= mu[ch]) { xi -= mu[ch]; ch++; }
        }
        proc.add(ch, ph.w);
        if (ch == kPhoto) {
          deposit(r, ph.e);
          ph.uncollided = false;
          alive = false;
          record("photo");
          break;
        }
        if (ch == kCompton) {
          double c;
          double e2 = sample_compton(ph.e, rng, &c);
          deposit(r, ph.e - e2);
          ph.uncollided = false;
          rotate(ph.d, c, 2 * kPi * rng.uniform());
          ph.e = e2;
          record("compton");
        } else if (ch == kRayleigh) {
          int el = box.material->pick_rayleigh_element(ph.e, rng.uniform());
          ph.uncollided = false;
          double c = sample_rayleigh_cos(ph.e, box.material->element_z(el), rng);  // sequenced before phi: the
          double phi = 2 * kPi * rng.uniform();  // random stream must not depend on argument evaluation order
          rotate(ph.d, c, phi);
          record("rayleigh");
        } else {  // pair production: kinetic energy deposited locally, positron annihilates at rest into two photons
          deposit(r, ph.e - 2 * kElectronMassMeV);
          ph.uncollided = false;
          double cz = 2 * rng.uniform() - 1, phi = 2 * kPi * rng.uniform(), s = std::sqrt(std::max(0.0, 1 - cz * cz));
          Photon a = ph;
          a.e = kElectronMassMeV;
          a.d[0] = s * std::cos(phi); a.d[1] = s * std::sin(phi); a.d[2] = cz;
          Photon b = a;
          for (int i = 0; i < 3; i++) b.d[i] = -a.d[i];
          a.secondary = b.secondary = true;
          stack.push_back(a);
          stack.push_back(b);
          alive = false;
          record("pair");
          break;
        }
        if (implicit && ph.w < cfg.weight_cutoff) {  // Russian roulette: half survive with twice the weight
          if (rng.uniform() < 0.5) ph.w *= 2;
          else { alive = false; record("roulette"); }
        }
      }
    }
    for (size_t k = 0; k < cfg.pulse_regions.size(); k++) {
      double x = dep.cur[cfg.pulse_regions[k]];
      if (x > 0 && bin_of(x) >= 0) pulse.add(k * bins + bin_of(x), 1);
    }
    pulse.end_history(); tot.end_history(); dep.end_history(); depu.end_history(); pcount.end_history(); punc.end_history();
    pen.end_history(); pspec.end_history(); lspec.end_history(); leak.end_history(); proc.end_history();
  }

  res.histories = histories;
  res.source_energy = esrc / histories;
  res.bins = bins;
  res.emax = emax;
  for (size_t b = 0; b < nbox; b++) {
    res.region_names.push_back(geo.boxes[b].name);
    res.deposited.push_back(dep.stat(b, histories));
    res.deposited_uncollided.push_back(depu.stat(b, histories));
  }
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
