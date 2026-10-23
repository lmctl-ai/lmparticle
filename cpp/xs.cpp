#include "xs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numeric>
#include <sstream>

#include "xcom_embedded.hpp"

namespace lmparticle {
namespace {

double loglog(double x0, double y0, double x1, double y1, double x) {
  if (y0 > 0 && y1 > 0) return std::exp(std::log(y0) + (std::log(y1) - std::log(y0)) * (std::log(x) - std::log(x0)) / (std::log(x1) - std::log(x0)));
  return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

std::map<std::string, Element> &table() {
  static std::map<std::string, Element> t = [] {
    std::map<std::string, Element> m;
    std::istringstream in(std::string(reinterpret_cast<const char *>(embedded_xcom), embedded_xcom_size));
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::istringstream ls(line);
      int z;
      std::string sym;
      double e, c, i, p, n, el;
      if (!(ls >> z >> sym >> e >> c >> i >> p >> n >> el)) throw std::runtime_error("embedded XCOM table is corrupt: " + line);
      Element &x = m[sym];
      x.z = z;
      x.symbol = sym;
      x.energy.push_back(e);
      x.mass_coeff.push_back({c, i, p, n + el});
    }
    return m;
  }();
  return t;
}

}  // namespace

Channels Element::at(double e) const {
  size_t i = static_cast<size_t>(std::upper_bound(energy.begin(), energy.end(), e) - energy.begin());
  if (i == 0) return mass_coeff.front();
  if (i >= energy.size()) return mass_coeff.back();
  --i;  // energy[i] <= e < energy[i + 1]; at an edge the row above it is used
  Channels out;
  for (int k = 0; k < kChannels; k++) out[k] = loglog(energy[i], mass_coeff[i][k], energy[i + 1], mass_coeff[i + 1][k], e);
  return out;
}

const Element &element(const std::string &symbol) {
  auto it = table().find(symbol);
  if (it == table().end()) {
    std::string known;
    for (const auto &kv : table()) known += (known.empty() ? "" : ", ") + kv.first;
    throw std::runtime_error("unknown element '" + symbol + "' (available: " + known + ")");
  }
  return it->second;
}

std::vector<std::string> element_symbols() {
  std::vector<std::pair<int, std::string>> v;
  for (const auto &kv : table()) v.emplace_back(kv.second.z, kv.first);
  std::sort(v.begin(), v.end());
  std::vector<std::string> out;
  for (auto &p : v) out.push_back(p.second);
  return out;
}

Material::Material(std::string name, double density, std::vector<std::pair<std::string, double>> frac)
    : name_(std::move(name)), density_(density) {
  if (!(density > 0)) throw std::runtime_error("material '" + name_ + "': density must be positive");
  if (frac.empty()) throw std::runtime_error("material '" + name_ + "': add at least one element");
  double sum = 0;
  for (auto &f : frac) {
    if (!(f.second > 0)) throw std::runtime_error("material '" + name_ + "': mass fraction of " + f.first + " must be positive");
    sum += f.second;
  }
  for (auto &f : frac) {
    elements_.push_back(&element(f.first));
    weights_.push_back(f.second / sum);
    fractions_.emplace_back(f.first, f.second / sum);
  }
  // Union grid. Rows are (energy, kind, element, row); an element's "below edge" rows (same energy as the next row)
  // sort first inside a tie so that interpolation just under an edge ends on the below-edge value.
  struct Node { double e; int kind; size_t el, row; };
  std::vector<Node> nodes;
  for (size_t i = 0; i < elements_.size(); i++) {
    const auto &en = elements_[i]->energy;
    for (size_t j = 0; j < en.size(); j++) nodes.push_back({en[j], (j + 1 < en.size() && en[j + 1] == en[j]) ? 0 : 1, i, j});
  }
  std::stable_sort(nodes.begin(), nodes.end(), [](const Node &a, const Node &b) { return a.e != b.e ? a.e < b.e : a.kind < b.kind; });
  for (const Node &n : nodes) {
    Channels sum_c{};
    for (size_t i = 0; i < elements_.size(); i++) {
      Channels c = (i == n.el) ? elements_[i]->mass_coeff[n.row] : elements_[i]->at(n.e);
      for (int k = 0; k < kChannels; k++) sum_c[k] += weights_[i] * c[k] * density_;
    }
    energy_.push_back(n.e);
    mu_.push_back(sum_c);
  }
}

Channels Material::mu(double e) const {
  size_t i = static_cast<size_t>(std::upper_bound(energy_.begin(), energy_.end(), e) - energy_.begin());
  if (i == 0) return mu_.front();
  if (i >= energy_.size()) return mu_.back();
  --i;
  Channels out;
  for (int k = 0; k < kChannels; k++) out[k] = loglog(energy_[i], mu_[i][k], energy_[i + 1], mu_[i + 1][k], e);
  return out;
}
double Material::mu_total(double e) const {
  Channels c = mu(e);
  return c[0] + c[1] + c[2] + c[3];
}
Channels Material::mass_coeff(double e) const {
  Channels c = mu(e);
  for (auto &x : c) x /= density_;
  return c;
}
int Material::pick_rayleigh_element(double e, double u) const {
  if (elements_.size() == 1) return 0;
  std::vector<double> w(elements_.size());
  double sum = 0;
  for (size_t i = 0; i < w.size(); i++) sum += w[i] = weights_[i] * elements_[i]->at(e)[kRayleigh];
  double t = u * sum;
  for (size_t i = 0; i < w.size(); i++) { if (t < w[i]) return static_cast<int>(i); t -= w[i]; }
  return static_cast<int>(w.size()) - 1;
}

std::vector<std::string> builtin_names() {
  return {"water", "air", "lead", "iron", "aluminum", "copper", "tungsten", "silicon", "germanium", "concrete", "nai",
          "polyethylene", "tissue"};
}

std::shared_ptr<const Material> builtin_material(const std::string &n) {
  using F = std::vector<std::pair<std::string, double>>;
  auto mk = [&](double rho, F f) { return std::make_shared<const Material>(n, rho, std::move(f)); };
  if (n == "water") return mk(1.0, {{"H", 0.111894}, {"O", 0.888106}});
  if (n == "air") return mk(0.001205, {{"C", 0.000124}, {"N", 0.755268}, {"O", 0.231781}, {"Ar", 0.012827}});
  if (n == "lead") return mk(11.35, {{"Pb", 1.0}});
  if (n == "iron") return mk(7.874, {{"Fe", 1.0}});
  if (n == "aluminum") return mk(2.699, {{"Al", 1.0}});
  if (n == "copper") return mk(8.96, {{"Cu", 1.0}});
  if (n == "tungsten") return mk(19.3, {{"W", 1.0}});
  if (n == "silicon") return mk(2.33, {{"Si", 1.0}});
  if (n == "germanium") return mk(5.323, {{"Ge", 1.0}});
  if (n == "nai") return mk(3.667, {{"Na", 0.153373}, {"I", 0.846627}});
  if (n == "polyethylene") return mk(0.94, {{"H", 0.143711}, {"C", 0.856289}});
  if (n == "tissue") return mk(1.0, {{"H", 0.101174}, {"C", 0.111000}, {"N", 0.026000}, {"O", 0.761826}});
  if (n == "concrete")
    return mk(2.3, {{"H", 0.022100}, {"C", 0.002484}, {"O", 0.574930}, {"Na", 0.015208}, {"Mg", 0.001266}, {"Al", 0.019953},
                    {"Si", 0.304627}, {"K", 0.010045}, {"Ca", 0.042951}, {"Fe", 0.006435}});
  std::string known;
  for (const auto &b : builtin_names()) known += (known.empty() ? "" : ", ") + b;
  throw std::runtime_error("unknown builtin material '" + n + "' (available: " + known + ")");
}

double klein_nishina_total(double e) {
  const double r_e = 2.8179403262e-13;  // classical electron radius, cm
  const double pi = 3.14159265358979323846;
  double k = e / kElectronMassMeV;
  if (k < 1e-4) return 8.0 * pi / 3.0 * r_e * r_e * (1 - 2 * k + 5.2 * k * k);  // series: avoids cancellation
  double l = std::log(1 + 2 * k);
  double a = (1 + k) / (k * k) * (2 * (1 + k) / (1 + 2 * k) - l / k);
  double b = l / (2 * k);
  double c = (1 + 3 * k) / ((1 + 2 * k) * (1 + 2 * k));
  return 2 * pi * r_e * r_e * (a + b - c);
}

double compton_mean_transfer(double e) {
  // <(E - E')/E> over the Klein-Nishina distribution in x = E'/E in [1/(1+2k), 1], by Simpson's rule
  double k = e / kElectronMassMeV;
  double x0 = 1.0 / (1 + 2 * k);
  auto pdf = [&](double x) {  // dsigma/dx up to a constant
    double t = (1 - x) / (k * x);  // 1 - cos(theta)
    double sin2 = t * (2 - t);
    return x + 1.0 / x - sin2;
  };
  const int n = 4000;
  double h = (1 - x0) / n, num = 0, den = 0;
  for (int i = 0; i <= n; i++) {
    double x = x0 + h * i, w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
    double p = pdf(x);
    den += w * p;
    num += w * p * (1 - x);
  }
  return num / den;
}

}  // namespace lmparticle
