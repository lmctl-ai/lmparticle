// Native regression tests of the physics core: tables, sampling and bookkeeping without the DSL layer.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "transport.hpp"
#include "xs.hpp"

using namespace lmparticle;
static int failures = 0;
static const double kPiT = 3.14159265358979323846;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
  } while (0)
#define NEAR(a, b, tol) CHECK(std::fabs((a) - (b)) <= (tol))

int main() {
  // the table parses and every element is present with sorted energies and an edge pair only where energies repeat
  auto syms = element_symbols();
  CHECK(syms.size() == 19);
  for (auto &s : syms) {
    const Element &e = element(s);
    CHECK(e.energy.size() > 40);
    CHECK(e.energy.front() == 0.001 && e.energy.back() == 20.0);
    for (size_t i = 1; i < e.energy.size(); i++) CHECK(e.energy[i] >= e.energy[i - 1]);
  }
  bool threw = false;
  try { element("Xx"); } catch (const std::exception &) { threw = true; }
  CHECK(threw);

  // XCOM values at grid points are reproduced exactly (hydrogen at 1 MeV: incoherent 0.1263 cm2/g)
  NEAR(element("H").at(1.0)[kCompton], 0.1263, 1e-6);
  NEAR(element("Pb").at(0.1)[kPhoto] + element("Pb").at(0.1)[kCompton] + element("Pb").at(0.1)[kRayleigh], 5.549, 0.002);
  // interpolation is monotone between two grid points of a smooth region
  double a = element("Fe").at(0.3)[kCompton], b = element("Fe").at(0.35)[kCompton], c = element("Fe").at(0.4)[kCompton];
  CHECK(a > b && b > c);

  // water by the mixture rule
  auto w = builtin_material("water");
  NEAR(w->mass_coeff(1.0)[kCompton] + w->mass_coeff(1.0)[kRayleigh] + w->mass_coeff(1.0)[kPhoto], 0.07072, 1e-5);

  // RNG: reproducible, uniform
  Rng r1(42), r2(42), r3(43);
  bool same = true, differ = false;
  for (int i = 0; i < 1000; i++) { uint64_t x = r1.next(), y = r2.next(), z = r3.next(); same &= x == y; differ |= x != z; }
  CHECK(same && differ);
  Rng r4(1);
  double sum = 0, sum2 = 0;
  const int n = 1000000;
  for (int i = 0; i < n; i++) { double u = r4.uniform(); CHECK(u >= 0 && u < 1); sum += u; sum2 += u * u; }
  NEAR(sum / n, 0.5, 0.002);
  NEAR(sum2 / n, 1.0 / 3.0, 0.002);

  // Klein-Nishina sampler: mean of E'/E, and the scattering cosine stays in range and matches the Compton relation
  for (double e : {0.02, 0.511, 1.0, 6.0}) {
    Rng rng(7);
    double m = 0;
    for (int i = 0; i < 400000; i++) {
      double cost;
      double e2 = sample_compton(e, rng, &cost);
      CHECK(cost >= -1 && cost <= 1);
      if (i < 1000) NEAR(e2, e / (1 + e / kElectronMassMeV * (1 - cost)), 1e-9 * e);
      m += e2 / e;
    }
    NEAR(m / 400000, 1.0 - compton_mean_transfer(e), 0.002);
  }

  // one run: energy is conserved exactly and results do not depend on anything but the seed
  Geometry geo;
  geo.boxes.push_back({"slab", builtin_material("lead"), {-INFINITY, -INFINITY, 0}, {INFINITY, INFINITY, 1}});
  Source src;
  src.isotropic = false;
  src.pos[2] = -1;
  src.lines.push_back({3.0, 1.0});
  Config cfg;
  cfg.seed = 5;
  Result a1 = run(geo, src, cfg, 20000), a2 = run(geo, src, cfg, 20000);
  NEAR(a1.total_deposited.mean + a1.escaped_energy.mean, 3.0, 1e-9);
  CHECK(a1.total_deposited.mean == a2.total_deposited.mean);
  CHECK(a1.escaped.mean == a2.escaped.mean);
  CHECK(a1.interactions[kPair].mean > 0);

  // Klein-Nishina angular distribution: the sampled mean cos(theta) equals the integral of dsigma/dOmega,
  // proportional to P^2 (P + 1/P - sin^2) with P = E'/E = 1/(1 + k(1 - cos)), done here by Simpson's rule in cos.
  for (double e : {0.01, 0.1, 0.662, 2.0, 10.0}) {
    double k = e / kElectronMassMeV, num = 0, den = 0;
    const int m = 20000;
    for (int i = 0; i <= m; i++) {
      double c = -1 + 2.0 * i / m, P = 1 / (1 + k * (1 - c)), w = (i == 0 || i == m) ? 1 : (i % 2 ? 4 : 2);
      double f = P * P * (P + 1 / P - (1 - c * c));
      num += w * f * c;
      den += w * f;
    }
    Rng rng(11);
    double sc = 0;
    const int ns = 400000;
    for (int i = 0; i < ns; i++) { double cost; sample_compton(e, rng, &cost); sc += cost; }
    NEAR(sc / ns, num / den, 0.004);  // standard error of the mean is below 0.0009
  }

  // a source on the high face of a slab pointing into it is inside the slab's reach: uncollided = exp(-mu x)
  {
    Geometry g;
    auto water = builtin_material("water");
    g.boxes.push_back({"w", water, {-INFINITY, -INFINITY, 0}, {INFINITY, INFINITY, 10}});
    Source s;
    s.isotropic = false;
    s.dir[2] = -1;
    s.pos[2] = 10;
    s.lines.push_back({1.0, 1.0});
    Config c;
    Result r = run(g, s, c, 100000);
    double want = std::exp(-10 * water->mu_total(1.0));
    CHECK(std::fabs(r.escaped_uncollided.mean - want) < 4 * r.escaped_uncollided.err);
    CHECK(r.total_deposited.mean > 0.1);
  }
  // spectrum entries above the upper edge are not tallied (no fake peak in the last bin); emax itself is the last bin
  {
    Geometry g;
    g.boxes.push_back({"w", builtin_material("water"), {-INFINITY, -INFINITY, 0}, {INFINITY, INFINITY, 1}});
    Source s;
    s.isotropic = false;
    s.pos[2] = -1;
    s.lines.push_back({1.0, 1.0});
    Config c;
    c.bins = 10;
    c.emax = 0.5;
    c.planes.push_back({"exit", 2, 1.0});
    Result r = run(g, s, c, 20000);
    double sum = 0;
    for (auto &b : r.plane_spectrum[0]) sum += b.mean;
    CHECK(sum < r.plane_count[0].mean - r.plane_uncollided[0].mean + 1e-12);  // uncollided 1 MeV photons are above 0.5
    c.emax = 1.0;
    Result r2 = run(g, s, c, 20000);
    CHECK(r2.plane_spectrum[0][9].mean >= r2.plane_uncollided[0].mean);
  }
  // nonfinite material parameters are refused
  threw = false;
  try { Material m("x", INFINITY, {{"H", 1.0}}); } catch (const std::exception &) { threw = true; }
  CHECK(threw);
  threw = false;
  try { Material m("x", 1.0, {{"H", INFINITY}}); } catch (const std::exception &) { threw = true; }
  CHECK(threw);

  // a photon leaving through a region's low face at a grazing angle used to land exactly on the face (the nudge along
  // the direction was lost to rounding) and stay "inside" until the 1e8-step guard: 200 starts, 1e-12 rad to the face
  {
    Geometry g;
    Box b;
    b.name = "thin";
    b.material = std::make_shared<const Material>("thin", 1e-9, std::vector<std::pair<std::string, double>>{{"H", 1.0}});
    b.lo[0] = -INFINITY; b.lo[1] = -INFINITY; b.lo[2] = 2; b.hi[0] = INFINITY; b.hi[1] = INFINITY; b.hi[2] = 3;
    g.boxes.push_back(b);
    int bad = 0;
    for (int i = 0; i < 200; i++) {
      Source s;
      s.isotropic = false;
      s.pos[2] = 2.3 + 0.001 * i;
      s.dir[0] = 1; s.dir[1] = 0; s.dir[2] = -1e-12;
      s.lines.push_back({1.0, 1.0});
      Config c;
      try { Result r = run(g, s, c, 1); if (r.escaped.mean != 1) bad++; }
      catch (const std::exception &) { bad++; }
    }
    CHECK(bad == 0);
  }
  // rotate: the deflection equals theta to 1e-9 rad even within 1e-5 rad of the z axis (the old shortcut was off by
  // up to 0.26 degrees there), and the result is a unit vector
  for (double tilt : {0.0, 1e-9, 1e-6, 4.5e-3, 0.3, 1.5}) {
    for (double sign : {1.0, -1.0}) {
      double d0[3] = {std::sin(tilt), 0, sign * std::cos(tilt)};
      for (double theta : {0.01, 0.7, 2.5}) {
        for (double phi : {0.3, 2.0, 4.4}) {
          double d[3] = {d0[0], d0[1], d0[2]};
          rotate(d, std::cos(theta), phi);
          double dot = d[0] * d0[0] + d[1] * d0[1] + d[2] * d0[2], n = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
          NEAR(std::acos(std::max(-1.0, std::min(1.0, dot))), theta, 1e-9);
          NEAR(n, 1.0, 1e-12);
        }
      }
    }
  }
  // sphere and cylinder regions: chord lengths give exp(-mu * chord) uncollided transmission
  {
    auto pb = builtin_material("lead");
    double mu = pb->mu_total(1.0);
    auto beam = [](double x, double y, double z, double dx, double dy, double dz) {
      Source s;
      s.isotropic = false;
      s.pos[0] = x; s.pos[1] = y; s.pos[2] = z;
      s.dir[0] = dx; s.dir[1] = dy; s.dir[2] = dz;
      s.lines.push_back({1.0, 1.0});
      return s;
    };
    auto within = [](const Stat &s, double want, double extra) { return std::fabs(s.mean - want) < 4 * s.err + extra; };
    Geometry g;
    Box sph;
    sph.name = "ball"; sph.material = pb; sph.shape = kSphereShape; sph.radius = 1.0;
    sph.center[0] = 1; sph.center[1] = -2; sph.center[2] = 3;
    for (int i = 0; i < 3; i++) { sph.lo[i] = sph.center[i] - 1; sph.hi[i] = sph.center[i] + 1; }
    g.boxes.push_back(sph);
    Config c;
    c.seed = 9;
    Result r = run(g, beam(-5, -2, 3, 1, 0, 0), c, 100000);        // through the center: chord 2R
    CHECK(within(r.escaped_uncollided, std::exp(-2 * mu), 0));
    Result r2 = run(g, beam(1, -2, -5, 0, 0.6, 0.8), c, 100000);   // oblique, misses: the ray passes at distance > R
    CHECK(r2.escaped_uncollided.mean == 1.0);
    Result r3 = run(g, beam(-5, -2 + 0.6, 3, 1, 0, 0), c, 100000);  // impact parameter 0.6: chord 2 sqrt(1 - 0.36) = 1.6
    CHECK(within(r3.escaped_uncollided, std::exp(-1.6 * mu), 0));
    // an isotropic source at the center of the ball: every direction sees R of lead
    Source iso;
    iso.pos[0] = 1; iso.pos[1] = -2; iso.pos[2] = 3;
    iso.lines.push_back({1.0, 1.0});
    Result r4 = run(g, iso, c, 100000);
    CHECK(within(r4.escaped_uncollided, std::exp(-mu), 0));
    NEAR(r4.total_deposited.mean + r4.escaped_energy.mean, 1.0, 1e-9);
    // a source on the sphere's surface pointing inwards enters it (t = 0 entry through the curved surface)
    Result r5 = run(g, beam(0, -2, 3, 1, 0, 0), c, 100000);
    CHECK(within(r5.escaped_uncollided, std::exp(-2 * mu), 0));
    Geometry gc;
    Box cyl;
    cyl.name = "rod"; cyl.material = pb; cyl.shape = kCylinderShape; cyl.radius = 0.5;
    cyl.center[0] = 2; cyl.center[1] = 1; cyl.center[2] = 0;
    cyl.lo[0] = 1.5; cyl.hi[0] = 2.5; cyl.lo[1] = 0.5; cyl.hi[1] = 1.5; cyl.lo[2] = -1; cyl.hi[2] = 2;
    gc.boxes.push_back(cyl);
    Result q1 = run(gc, beam(2, 1, -5, 0, 0, 1), c, 100000);        // along the axis: 3 cm
    CHECK(within(q1.escaped_uncollided, std::exp(-3 * mu), 0));
    Result q2 = run(gc, beam(-5, 1, 1, 1, 0, 0), c, 100000);        // across through the axis: 2R = 1 cm
    CHECK(within(q2.escaped_uncollided, std::exp(-mu), 0));
    Result q3 = run(gc, beam(-5, 1.3, 1, 1, 0, 0), c, 100000);      // across at impact 0.3: chord 2 sqrt(0.25 - 0.09) = 0.8
    CHECK(within(q3.escaped_uncollided, std::exp(-0.8 * mu), 0));
    Result q4 = run(gc, beam(-5, 1, 2.5, 1, 0, 0), c, 1000);        // above the top cap: misses
    CHECK(q4.escaped_uncollided.mean == 1.0);
    Result q5 = run(gc, beam(2.2, 1.2, -5, 0, 0, 1), c, 100000);    // parallel to the axis inside the radius: 3 cm
    CHECK(within(q5.escaped_uncollided, std::exp(-3 * mu), 0));
    NEAR(q5.total_deposited.mean + q5.escaped_energy.mean, 1.0, 1e-9);
  }
  // cone and disc sources: the fraction of photons that reach a black target follows solid angle and area
  {
    auto w = builtin_material("tungsten");
    Geometry g;
    Box sph;
    sph.name = "target"; sph.material = w; sph.shape = kSphereShape; sph.radius = 10 * std::sin(10 * kPiT / 180);  // subtends 10 degrees at 10 cm
    sph.center[2] = 10;
    for (int i = 0; i < 3; i++) { sph.lo[i] = sph.center[i] - sph.radius; sph.hi[i] = sph.center[i] + sph.radius; }
    g.boxes.push_back(sph);
    Source s;
    s.isotropic = false;
    s.cone_cos = std::cos(30 * kPiT / 180);
    s.lines.push_back({0.03, 1.0});  // 30 keV: absorbed within 0.01 cm of tungsten
    Config c;
    c.seed = 3;
    Result r = run(g, s, c, 200000);
    double want = (1 - std::cos(10 * kPiT / 180)) / (1 - std::cos(30 * kPiT / 180));
    CHECK(std::fabs(1 - r.escaped_uncollided.mean - want) < 4 * r.escaped_uncollided.err + 0.002);
    Geometry gb;
    gb.boxes.push_back({"plate", w, {-0.5, -0.5, 5}, {0.5, 0.5, 7}});
    Source d;
    d.isotropic = false;
    d.disc_radius = 1.0;  // a 1 x 1 square inside a disc of radius 1 catches 1/pi of the beam
    d.lines.push_back({0.03, 1.0});
    Result rd = run(gb, d, c, 200000);
    CHECK(std::fabs(1 - rd.escaped_uncollided.mean - 1 / kPiT) < 4 * rd.escaped_uncollided.err + 0.002);
    d.disc_radius = 0.4;  // fully inside the plate
    Result rd2 = run(gb, d, c, 20000);
    CHECK(rd2.escaped_uncollided.mean == 0.0);
  }
  // implicit capture with roulette: same means as analog transport within errors, smaller error deep in the shield
  {
    auto pb = builtin_material("lead");
    Geometry g;
    for (int i = 0; i < 4; i++) g.boxes.push_back({"L" + std::to_string(i), pb, {-INFINITY, -INFINITY, 0.5 * i}, {INFINITY, INFINITY, 0.5 * i + 0.5}});
    Source s;
    s.isotropic = false;
    s.pos[2] = -1;
    s.lines.push_back({0.3, 1.0});
    Config a, b;
    a.seed = b.seed = 8;
    a.planes.push_back({"exit", 2, 2.0});
    b.planes.push_back({"exit", 2, 2.0});
    b.weight_cutoff = 0.05;
    Result ra = run(g, s, a, 200000), rb = run(g, s, b, 200000);
    double mu = pb->mu_total(0.3);
    CHECK(std::fabs(rb.plane_uncollided[0].mean - std::exp(-2 * mu)) < 4 * rb.plane_uncollided[0].err);
    for (int i = 0; i < 4; i++) CHECK(std::fabs(ra.deposited[i].mean - rb.deposited[i].mean) < 4 * std::hypot(ra.deposited[i].err, rb.deposited[i].err));
    CHECK(std::fabs(ra.plane_count[0].mean - rb.plane_count[0].mean) < 4 * std::hypot(ra.plane_count[0].err, rb.plane_count[0].err));
    CHECK(rb.deposited[3].err < ra.deposited[3].err);
    CHECK(rb.plane_count[0].err < ra.plane_count[0].err);
    NEAR(ra.total_deposited.mean + ra.escaped_energy.mean, 0.3, 1e-9);
    CHECK(rb.interactions[kPhoto].mean > 0.5 * ra.interactions[kPhoto].mean && rb.interactions[kPhoto].mean < 1.5 * ra.interactions[kPhoto].mean);
    // first-collision deposit: all of the deposit in a thin film, part of it in a thick slab
    Geometry thin;
    thin.boxes.push_back({"film", pb, {-INFINITY, -INFINITY, 0}, {INFINITY, INFINITY, 0.001}});
    Result rt = run(thin, s, a, 100000);
    CHECK(rt.deposited_uncollided[0].mean > 0.9 * rt.deposited[0].mean);
    CHECK(ra.deposited_uncollided[0].mean < 0.95 * ra.deposited[0].mean && ra.deposited_uncollided[0].mean > 0.3 * ra.deposited[0].mean);
    // tracks: every recorded history starts with "source", energies never rise along a primary, ends are terminal events
    a.track_histories = 50;
    Result rk = run(g, s, a, 200);
    CHECK(!rk.tracks.empty());
    int64_t last = -1;
    double e = 0;
    for (const TrackPoint &t : rk.tracks) {
      CHECK(t.history < 50);
      std::string ev = t.event;
      if (t.history != last) { CHECK(ev == "source"); last = t.history; e = 0.3; }
      else if (ev != "annihilation") CHECK(t.e <= e + 1e-12);
      e = t.e;
    }
    std::string lastev = rk.tracks.back().event;
    CHECK(lastev == "escape" || lastev == "photo" || lastev == "cutoff" || lastev == "pair");
  }
  // user element tables: a two-row table interpolates log-log; malformed tables are refused
  {
    add_element("Xq", 50, "# E coh incoh photo pair_n pair_e\n0.001 1.0 0.1 100.0 0 0\n20.0 0.0001 0.01 0.001 0.02 0.001\n");
    const Element &x = element("Xq");
    CHECK(x.z == 50 && x.energy.size() == 2);
    double want = std::exp(std::log(100.0) + (std::log(0.001) - std::log(100.0)) * (std::log(1.0) - std::log(0.001)) / (std::log(20.0) - std::log(0.001)));
    NEAR(x.at(1.0)[kPhoto], want, 1e-9 * want);
    Material m("xq", 2.0, {{"Xq", 1.0}});
    NEAR(m.mu(1.0)[kPhoto], 2 * want, 1e-9 * want);
    for (const char *bad : {"0.001 1 1 1 0 0\n", "0.001 1 1 1 0 0\n0.5 1 1 1 0 0\n", "0.001 1 1 1 0 0\n20 -1 1 1 0 0\n", "0.001 1 1 1 0 0\n20 1 1 1 0 0 7\n", "0.001 1 1 1 0 0\n0.0005 1 1 1 0 0\n20 1 1 1 0 0\n"}) {
      threw = false;
      try { add_element("Xr", 51, bad); } catch (const std::exception &) { threw = true; }
      CHECK(threw);
    }
  }

  if (failures) { std::fprintf(stderr, "%d failures\n", failures); return 1; }
  std::puts("core tests passed");
  return 0;
}
