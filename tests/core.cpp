// Native regression tests of the physics core: tables, sampling and bookkeeping without the DSL layer.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "transport.hpp"
#include "xs.hpp"

using namespace lmparticle;
static int failures = 0;
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

  if (failures) { std::fprintf(stderr, "%d failures\n", failures); return 1; }
  std::puts("core tests passed");
  return 0;
}
