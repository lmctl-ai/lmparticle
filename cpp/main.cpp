// lmparticle: Monte Carlo photon transport (NIST XCOM cross sections, Klein-Nishina Compton) behind an lmscript `_dsl_`
// schema. Scripting only: the executable has no user-facing commands besides --dsl and the schema/version queries.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "dslvm.hpp"
#include "memory_budget.hpp"
#include "notices_embedded.hpp"
#include "schema_embedded.hpp"
#include "transport.hpp"
#include "xs.hpp"

namespace {
using dslvm::Args;
using dslvm::Value;
using namespace lmparticle;

struct Rad {};
struct MaterialSpec {
  std::string name;
  double density = 0;
  std::vector<std::pair<std::string, double>> fractions;
  std::shared_ptr<const Material> built;
  bool builtin = false;
  const Material &get() {
    if (!built) built = std::make_shared<const Material>(name, density, fractions);
    return *built;
  }
};
struct GeometrySpec { Geometry geo; };
struct SimSpec { Geometry geo; Source src; Config cfg; };

struct Host {
  std::vector<std::unique_ptr<MaterialSpec>> materials;
  std::vector<std::unique_ptr<GeometrySpec>> geometries;
  std::vector<std::unique_ptr<Source>> sources;
  std::vector<std::unique_ptr<SimSpec>> sims;
  std::vector<std::unique_ptr<Result>> results;
  template <class T> Value hold(std::vector<std::unique_ptr<T>> &arena, const char *type, T v) {
    if (arena.size() >= 1000000) throw dslvm::Error(std::string(type) + " object limit exceeded");
    arena.push_back(std::make_unique<T>(std::move(v)));
    return Value::object(type, arena.back().get());
  }
};

template <class F> Value translate(F &&fn) {
  try { return fn(); }
  catch (const dslvm::Error &) { throw; }
  catch (const std::exception &e) { throw dslvm::Error(e.what()); }
}

void method(dslvm::Registry &r, const char *type, const char *name, std::vector<std::string> ps, std::string ret, dslvm::Method fn) {
  const std::string who = std::string(type) + "." + name;
  r.method(type, name, ps, ret, [who, ps, fn](void *self, Args &a) {
    for (size_t i = 0; i < ps.size(); ++i) {
      const auto &p = a[i];
      const bool ok = ps[i] == "float" ? p.kind == Value::FLOAT
                      : ps[i] == "int" ? p.kind == Value::INT
                      : ps[i] == "string" ? p.kind == Value::STR
                      : ps[i] == "bool" ? p.kind == Value::BOOL
                      : p.kind == Value::OBJ && p.obj && p.type == ps[i];
      if (!ok) throw dslvm::Error(who + " argument " + std::to_string(i + 1) + " must be " + ps[i]);
    }
    return translate([&] { return fn(self, a); });
  });
}

template <class T> T &obj(void *p) { return *static_cast<T *>(p); }
template <class T> T &arg(Value &v) { return *static_cast<T *>(v.obj); }
std::string fmt(double x) { char b[48]; std::snprintf(b, sizeof b, "%.17g", x); return b; }
void need(bool ok, const std::string &msg) { if (!ok) throw dslvm::Error(msg); }
double energy_arg(double e, const char *who) {
  need(e >= kMinEnergy && e <= kMaxEnergy, std::string(who) + ": energy must be between 0.001 and 20 MeV (MeV units)");
  return e;
}
size_t find_name(const std::vector<std::string> &names, const std::string &n, const char *what) {
  for (size_t i = 0; i < names.size(); i++) if (names[i] == n) return i;
  std::string known;
  for (auto &k : names) known += (known.empty() ? "" : ", ") + k;
  throw dslvm::Error(std::string("unknown ") + what + " '" + n + "' (known: " + known + ")");
}
std::string g(double x) { char b[48]; std::snprintf(b, sizeof b, "%.10g", x); return b; }
void save(const std::string &path, const std::string &text, const char *who) {
  std::ofstream f(path, std::ios::binary);
  if (!f) throw dslvm::Error(std::string(who) + ": cannot write " + path);
  f << text;
}

void register_host(dslvm::Registry &r, Host &h) {
  r.context("rad", "Rad");
  // ---------------------------------------------------------------- Rad
  method(r, "Rad", "builtin", {"string"}, "Material", [&h](void *, Args &a) {
    auto m = builtin_material(a[0].s);
    MaterialSpec s{m->name(), m->density(), m->fractions(), m, true};
    return h.hold(h.materials, "Material", std::move(s));
  });
  method(r, "Rad", "material", {"string", "float"}, "Material", [&h](void *, Args &a) {
    need(a[1].f > 0 && std::isfinite(a[1].f), "Rad.material: density must be positive and finite (g/cm3)");
    return h.hold(h.materials, "Material", MaterialSpec{a[0].s, a[1].f, {}, nullptr, false});
  });
  method(r, "Rad", "klein_nishina", {"float"}, "float", [](void *, Args &a) { return Value::real(klein_nishina_total(energy_arg(a[0].f, "Rad.klein_nishina"))); });
  method(r, "Rad", "compton_mean_transfer", {"float"}, "float", [](void *, Args &a) { return Value::real(compton_mean_transfer(energy_arg(a[0].f, "Rad.compton_mean_transfer"))); });
  method(r, "Rad", "sample_compton_mean", {"float", "int", "int"}, "float", [](void *, Args &a) {
    energy_arg(a[0].f, "Rad.sample_compton_mean");
    need(a[1].i >= 1 && a[1].i <= 100000000, "Rad.sample_compton_mean: samples must be 1..100000000");
    Rng rng(static_cast<uint64_t>(a[2].i));
    double sum = 0, c;
    for (int64_t i = 0; i < a[1].i; i++) sum += sample_compton(a[0].f, rng, &c) / a[0].f;
    return Value::real(sum / a[1].i);
  });
  method(r, "Rad", "geometry", {}, "Geometry", [&h](void *, Args &) { return h.hold(h.geometries, "Geometry", GeometrySpec{}); });
  method(r, "Rad", "source", {}, "Source", [&h](void *, Args &) { return h.hold(h.sources, "Source", Source{}); });
  method(r, "Rad", "simulation", {"Geometry", "Source"}, "Sim", [&h](void *, Args &a) {
    SimSpec s;
    s.geo = arg<GeometrySpec>(a[0]).geo;
    s.src = arg<Source>(a[1]);
    return h.hold(h.sims, "Sim", std::move(s));
  });
  method(r, "Rad", "assert_close", {"float", "float", "float"}, "void", [](void *, Args &a) {
    if (!std::isfinite(a[0].f) || !std::isfinite(a[1].f) || !std::isfinite(a[2].f) || a[2].f < 0 || std::fabs(a[0].f - a[1].f) > a[2].f)
      throw dslvm::Error("assert_close failed: " + fmt(a[0].f) + " vs " + fmt(a[1].f) + " (tolerance " + fmt(a[2].f) + ")");
    return Value::none();
  });
  method(r, "Rad", "assert_true", {"bool"}, "void", [](void *, Args &a) {
    if (!a[0].b) throw dslvm::Error("assert_true failed");
    return Value::none();
  });
  method(r, "Rad", "save_text", {"string", "string"}, "void", [](void *, Args &a) { save(a[0].s, a[1].s, "Rad.save_text"); return Value::none(); });
  method(r, "Rad", "print", {"string"}, "void", [](void *, Args &a) { std::cout << a[0].s << '\n'; return Value::none(); });
  method(r, "Rad", "version", {}, "string", [](void *, Args &) { return Value::string("lmparticle " LMPARTICLE_VERSION); });

  // ---------------------------------------------------------------- Material
  method(r, "Material", "element", {"string", "float"}, "Material", [](void *p, Args &a) {
    auto &m = obj<MaterialSpec>(p);
    need(!m.builtin, "Material.element: a builtin material cannot be changed");
    element(a[0].s);  // validates the symbol now
    need(a[1].f > 0 && std::isfinite(a[1].f), "Material.element: mass fraction must be positive and finite");
    m.fractions.emplace_back(a[0].s, a[1].f);
    m.built.reset();
    return Value::object("Material", p);
  });
  method(r, "Material", "name", {}, "string", [](void *p, Args &) { return Value::string(obj<MaterialSpec>(p).name); });
  method(r, "Material", "density", {}, "float", [](void *p, Args &) { return Value::real(obj<MaterialSpec>(p).density); });
  method(r, "Material", "mu", {"float"}, "float", [](void *p, Args &a) { return Value::real(obj<MaterialSpec>(p).get().mu_total(energy_arg(a[0].f, "Material.mu"))); });
  method(r, "Material", "mu_mass", {"float"}, "float", [](void *p, Args &a) {
    auto &m = obj<MaterialSpec>(p).get();
    return Value::real(m.mu_total(energy_arg(a[0].f, "Material.mu_mass")) / m.density());
  });
  method(r, "Material", "mu_partial", {"float", "string"}, "float", [](void *p, Args &a) {
    auto &m = obj<MaterialSpec>(p).get();
    Channels c = m.mu(energy_arg(a[0].f, "Material.mu_partial"));
    const std::string &k = a[1].s;
    if (k == "rayleigh" || k == "coherent") return Value::real(c[kRayleigh]);
    if (k == "compton" || k == "incoherent") return Value::real(c[kCompton]);
    if (k == "photo" || k == "photoelectric") return Value::real(c[kPhoto]);
    if (k == "pair") return Value::real(c[kPair]);
    throw dslvm::Error("Material.mu_partial: process must be rayleigh, compton, photo or pair");
  });
  method(r, "Material", "mfp", {"float"}, "float", [](void *p, Args &a) { return Value::real(1.0 / obj<MaterialSpec>(p).get().mu_total(energy_arg(a[0].f, "Material.mfp"))); });
  method(r, "Material", "hvl", {"float"}, "float", [](void *p, Args &a) { return Value::real(std::log(2.0) / obj<MaterialSpec>(p).get().mu_total(energy_arg(a[0].f, "Material.hvl"))); });

  // ---------------------------------------------------------------- Geometry
  auto add_box = [](void *p, const char *who, const std::string &name, Value &mat, double lo[3], double hi[3]) {
    auto &geo = obj<GeometrySpec>(p).geo;
    for (int i = 0; i < 3; i++) need(lo[i] < hi[i], std::string(who) + ": each box needs low < high on every axis");
    for (auto &b : geo.boxes) need(b.name != name, std::string(who) + ": duplicate region name '" + name + "'");
    Box b;
    b.name = name;
    b.material = arg<MaterialSpec>(mat).built ? arg<MaterialSpec>(mat).built : (arg<MaterialSpec>(mat).get(), arg<MaterialSpec>(mat).built);
    for (int i = 0; i < 3; i++) { b.lo[i] = lo[i]; b.hi[i] = hi[i]; }
    geo.boxes.push_back(std::move(b));
  };
  method(r, "Geometry", "box", {"string", "Material", "float", "float", "float", "float", "float", "float"}, "Geometry",
         [add_box](void *p, Args &a) {
    double lo[3] = {a[2].f, a[4].f, a[6].f}, hi[3] = {a[3].f, a[5].f, a[7].f};
    add_box(p, "Geometry.box", a[0].s, a[1], lo, hi);
    return Value::object("Geometry", p);
  });
  method(r, "Geometry", "slab", {"string", "Material", "float", "float"}, "Geometry", [add_box](void *p, Args &a) {
    double inf = std::numeric_limits<double>::infinity();
    double lo[3] = {-inf, -inf, a[2].f}, hi[3] = {inf, inf, a[3].f};
    add_box(p, "Geometry.slab", a[0].s, a[1], lo, hi);
    return Value::object("Geometry", p);
  });
  method(r, "Geometry", "regions", {}, "int", [](void *p, Args &) { return Value::integer((int64_t)obj<GeometrySpec>(p).geo.boxes.size()); });

  // ---------------------------------------------------------------- Source
  method(r, "Source", "point", {"float", "float", "float"}, "Source", [](void *p, Args &a) {
    need(std::isfinite(a[0].f) && std::isfinite(a[1].f) && std::isfinite(a[2].f), "Source.point: coordinates must be finite (cm)");
    for (int i = 0; i < 3; i++) obj<Source>(p).pos[i] = a[i].f;
    return Value::object("Source", p);
  });
  method(r, "Source", "beam", {"float", "float", "float"}, "Source", [](void *p, Args &a) {
    double n = std::sqrt(a[0].f * a[0].f + a[1].f * a[1].f + a[2].f * a[2].f);
    need(n > 0 && std::isfinite(n), "Source.beam: direction must be a nonzero vector");
    auto &s = obj<Source>(p);
    s.isotropic = false;
    for (int i = 0; i < 3; i++) s.dir[i] = a[i].f / n;
    return Value::object("Source", p);
  });
  method(r, "Source", "isotropic", {}, "Source", [](void *p, Args &) { obj<Source>(p).isotropic = true; return Value::object("Source", p); });
  method(r, "Source", "energy", {"float"}, "Source", [](void *p, Args &a) {
    obj<Source>(p).lines.assign(1, {energy_arg(a[0].f, "Source.energy"), 1.0});
    return Value::object("Source", p);
  });
  method(r, "Source", "line", {"float", "float"}, "Source", [](void *p, Args &a) {
    need(a[1].f > 0 && std::isfinite(a[1].f), "Source.line: weight must be positive and finite");
    obj<Source>(p).lines.emplace_back(energy_arg(a[0].f, "Source.line"), a[1].f);
    return Value::object("Source", p);
  });

  // ---------------------------------------------------------------- Sim
  method(r, "Sim", "plane", {"string", "string", "float"}, "Sim", [](void *p, Args &a) {
    auto &c = obj<SimSpec>(p).cfg;
    int axis = a[1].s == "x" ? 0 : a[1].s == "y" ? 1 : a[1].s == "z" ? 2 : -1;
    need(axis >= 0, "Sim.plane: axis must be x, y or z");
    need(std::isfinite(a[2].f), "Sim.plane: position must be finite (cm)");
    for (auto &pl : c.planes) need(pl.name != a[0].s, "Sim.plane: duplicate plane name '" + a[0].s + "'");
    c.planes.push_back({a[0].s, axis, a[2].f});
    return Value::object("Sim", p);
  });
  method(r, "Sim", "spectrum", {"int", "float"}, "Sim", [](void *p, Args &a) {
    need(a[0].i >= 1 && a[0].i <= 100000, "Sim.spectrum: bins must be 1..100000");
    need(a[1].f > 0 && a[1].f <= kMaxEnergy * 2, "Sim.spectrum: upper energy must be positive (MeV)");
    obj<SimSpec>(p).cfg.bins = (int)a[0].i;
    obj<SimSpec>(p).cfg.emax = a[1].f;
    return Value::object("Sim", p);
  });
  method(r, "Sim", "pulse_height", {"string"}, "Sim", [](void *p, Args &a) {
    auto &s = obj<SimSpec>(p);
    int idx = -1;
    for (size_t i = 0; i < s.geo.boxes.size(); i++) if (s.geo.boxes[i].name == a[0].s) idx = (int)i;
    need(idx >= 0, "Sim.pulse_height: unknown region '" + a[0].s + "'");
    for (int k : s.cfg.pulse_regions) need(k != idx, "Sim.pulse_height: region '" + a[0].s + "' is already tallied");
    s.cfg.pulse_regions.push_back(idx);
    return Value::object("Sim", p);
  });
  method(r, "Sim", "cutoff", {"float"}, "Sim", [](void *p, Args &a) {
    need(a[0].f >= kMinEnergy && a[0].f < kMaxEnergy, "Sim.cutoff: photon cutoff must be between 0.001 and 20 MeV");
    obj<SimSpec>(p).cfg.cutoff = a[0].f;
    return Value::object("Sim", p);
  });
  method(r, "Sim", "no_rayleigh", {}, "Sim", [](void *p, Args &) { obj<SimSpec>(p).cfg.rayleigh = false; return Value::object("Sim", p); });
  method(r, "Sim", "seed", {"int"}, "Sim", [](void *p, Args &a) {
    need(a[0].i >= 0, "Sim.seed: seed must not be negative");
    obj<SimSpec>(p).cfg.seed = (uint64_t)a[0].i;
    return Value::object("Sim", p);
  });
  method(r, "Sim", "run", {"int"}, "Result", [&h](void *p, Args &a) {
    need(a[0].i >= 1 && a[0].i <= 1000000000, "Sim.run: histories must be 1..1000000000");
    auto &s = obj<SimSpec>(p);
    // Fail before transport if the tallies (sum, sum2, current value, touched flag and index per
    // cell) and the Result kept in the arena (one Stat per cell) do not fit the memory budget.
    const size_t bins = (size_t)s.cfg.bins, npl = s.cfg.planes.size(), npulse = s.cfg.pulse_regions.size();
    const size_t nbox = s.geo.boxes.size();
    const double cells = double(nbox + 1 + 3 * npl + 7) + double(npulse + npl + 1) * double(bins);
    const double kept = double(nbox + 3 * npl) + double(npl + npulse + 1) * double(bins);
    const double bytes = cells * (4 * sizeof(double) + 1) + kept * sizeof(Stat);
    membudget::require(bytes < 1e18 ? (size_t)bytes : SIZE_MAX);
    return h.hold(h.results, "Result", run(s.geo, s.src, s.cfg, a[0].i));
  });

  // ---------------------------------------------------------------- Result
  auto region = [](void *p, const std::string &n) { auto &x = obj<Result>(p); return find_name(x.region_names, n, "region"); };
  auto plane = [](void *p, const std::string &n) { auto &x = obj<Result>(p); return find_name(x.plane_names, n, "plane"); };
  method(r, "Result", "histories", {}, "int", [](void *p, Args &) { return Value::integer(obj<Result>(p).histories); });
  method(r, "Result", "source_energy", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).source_energy); });
  method(r, "Result", "deposited", {"string"}, "float", [region](void *p, Args &a) { return Value::real(obj<Result>(p).deposited[region(p, a[0].s)].mean); });
  method(r, "Result", "deposited_err", {"string"}, "float", [region](void *p, Args &a) { return Value::real(obj<Result>(p).deposited[region(p, a[0].s)].err); });
  method(r, "Result", "total_deposited", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).total_deposited.mean); });
  method(r, "Result", "total_deposited_err", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).total_deposited.err); });
  method(r, "Result", "escaped", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).escaped.mean); });
  method(r, "Result", "escaped_err", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).escaped.err); });
  method(r, "Result", "escaped_uncollided", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).escaped_uncollided.mean); });
  method(r, "Result", "escaped_uncollided_err", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).escaped_uncollided.err); });
  method(r, "Result", "escaped_energy", {}, "float", [](void *p, Args &) { return Value::real(obj<Result>(p).escaped_energy.mean); });
  method(r, "Result", "plane_count", {"string"}, "float", [plane](void *p, Args &a) { return Value::real(obj<Result>(p).plane_count[plane(p, a[0].s)].mean); });
  method(r, "Result", "plane_count_err", {"string"}, "float", [plane](void *p, Args &a) { return Value::real(obj<Result>(p).plane_count[plane(p, a[0].s)].err); });
  method(r, "Result", "plane_uncollided", {"string"}, "float", [plane](void *p, Args &a) { return Value::real(obj<Result>(p).plane_uncollided[plane(p, a[0].s)].mean); });
  method(r, "Result", "plane_uncollided_err", {"string"}, "float", [plane](void *p, Args &a) { return Value::real(obj<Result>(p).plane_uncollided[plane(p, a[0].s)].err); });
  method(r, "Result", "plane_energy", {"string"}, "float", [plane](void *p, Args &a) { return Value::real(obj<Result>(p).plane_energy[plane(p, a[0].s)].mean); });
  method(r, "Result", "interactions", {"string"}, "float", [](void *p, Args &a) {
    const std::string &k = a[0].s;
    int c = k == "rayleigh" ? kRayleigh : k == "compton" ? kCompton : k == "photo" ? kPhoto : k == "pair" ? kPair : -1;
    need(c >= 0, "Result.interactions: process must be rayleigh, compton, photo or pair");
    return Value::real(obj<Result>(p).interactions[c].mean);
  });
  auto spectrum_text = [](const Result &x, const std::vector<Stat> &s) {
    std::string t;
    for (int b = 0; b < x.bins; b++) t += g(x.emax * b / x.bins) + " " + g(x.emax * (b + 1) / x.bins) + " " + g(s[b].mean) + " " + g(s[b].err) + "\n";
    return t;
  };
  auto pulse = [](void *p, const std::string &n) { auto &x = obj<Result>(p); return find_name(x.pulse_names, n, "pulse height region"); };
  method(r, "Result", "pulse_fraction", {"string", "float", "float"}, "float", [pulse](void *p, Args &a) {
    auto &x = obj<Result>(p);
    auto &hist = x.pulse_height[pulse(p, a[0].s)];
    need(a[1].f < a[2].f, "Result.pulse_fraction: need low < high");
    double sum = 0;  // bins whose lower edge lies in [low, high)
    for (int b = 0; b < x.bins; b++) { double lo = x.emax * b / x.bins; if (lo >= a[1].f && lo < a[2].f) sum += hist[b].mean; }
    return Value::real(sum);
  });
  method(r, "Result", "save_pulse_height", {"string", "string"}, "void", [pulse, spectrum_text](void *p, Args &a) {
    save(a[1].s, spectrum_text(obj<Result>(p), obj<Result>(p).pulse_height[pulse(p, a[0].s)]), "Result.save_pulse_height");
    return Value::none();
  });
  method(r, "Result", "save_regions", {"string"}, "void", [](void *p, Args &a) {
    auto &x = obj<Result>(p);
    std::string t;
    for (size_t i = 0; i < x.region_names.size(); i++) t += x.region_names[i] + " " + g(x.deposited[i].mean) + " " + g(x.deposited[i].err) + "\n";
    save(a[0].s, t, "Result.save_regions");
    return Value::none();
  });
  method(r, "Result", "save_plane_spectrum", {"string", "string"}, "void", [plane, spectrum_text](void *p, Args &a) {
    save(a[1].s, spectrum_text(obj<Result>(p), obj<Result>(p).plane_spectrum[plane(p, a[0].s)]), "Result.save_plane_spectrum");
    return Value::none();
  });
  method(r, "Result", "save_leakage_spectrum", {"string"}, "void", [spectrum_text](void *p, Args &a) {
    save(a[0].s, spectrum_text(obj<Result>(p), obj<Result>(p).leak_spectrum), "Result.save_leakage_spectrum");
    return Value::none();
  });
}

const char *usage = "usage: lmparticle --version | --notices | --dsl-schema [--json] | --dsl-signatures | [--dry-run] --dsl FILE [FILE ...]\n"
                    "environment: LMPARTICLE_MAX_MEMORY_MB  total memory budget in MiB (default 4096); a program that needs more\n"
                    "             stops with a run error (exit 3) instead of allocating\n";
}  // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  Host h;
  dslvm::Registry reg;
  register_host(reg, h);
  std::string a1 = argc > 1 ? argv[1] : "";
  if (argc == 2 && a1 == "--version") { std::cout << "lmparticle " LMPARTICLE_VERSION "\n"; return 0; }
  if (argc == 2 && a1 == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmparticle::embedded_notices), (std::streamsize)lmparticle::embedded_notices_size);
    return 0;
  }
  if (argc >= 2 && a1 == "--dsl-schema") {
    if (argc == 2) { for (const auto &s : reg.signatures()) std::cout << s << '\n'; return 0; }
    if (argc == 3 && std::string(argv[2]) == "--json") {
      std::cout.write(reinterpret_cast<const char *>(lmparticle::embedded_schema), (std::streamsize)lmparticle::embedded_schema_size);
      return 0;
    }
    std::cerr << usage;
    return 2;
  }
  if (argc == 2 && a1 == "--dsl-signatures") { for (const auto &s : reg.signatures()) std::cout << s << '\n'; return 0; }
  bool dry = false;
  int dsl_arg = 1;
  if (argc > 1 && a1 == "--dry-run") { dry = true; ++dsl_arg; }
  if (argc <= dsl_arg || std::string(argv[dsl_arg]) != "--dsl") { std::cerr << usage; return 2; }
  std::vector<std::string> files;
  for (int i = dsl_arg + 1; i < argc; i++) {
    if (std::string(argv[i]) == "--dry-run") {
      if (dry) { std::cerr << usage; return 2; }
      dry = true;
    } else if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) {
      std::cerr << usage;
      return 2;
    } else files.push_back(argv[i]);
  }
  if (files.empty()) { std::cerr << usage; return 2; }
  if (!membudget::configure("lmparticle", "LMPARTICLE_MAX_MEMORY_MB")) { std::cerr << usage; return 2; }
  const std::string schema(reinterpret_cast<const char *>(lmparticle::embedded_schema), lmparticle::embedded_schema_size);
  dslvm::Context ctx;
  Rad rad;
  ctx.set("rad", &rad);
  std::vector<dslvm::Program> programs;
  for (const auto &file : files)
    try { programs.push_back(dslvm::Program::load(dslvm::read_file(file), schema, reg)); }
    catch (const dslvm::Error &e) { return dslvm::report_error("load", e); }
    catch (const std::exception &e) { return dslvm::report_error("load", dslvm::Error(e.what())); }
  if (dry) { dslvm::report_ok(); return 0; }
  for (const auto &p : programs) {
    try { p.run(ctx, reg); }
    catch (const dslvm::Error &e) { return dslvm::report_error("run", e); }
    catch (const std::exception &e) { return dslvm::report_error("run", dslvm::Error(e.what())); }  // budget in the VM
    dslvm::report_ok();
  }
  return 0;
}
