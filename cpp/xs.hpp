// Photon cross sections: NIST XCOM element tables (embedded, data/xcom.csv) combined into materials
// by the mixture rule, plus the analytic Klein-Nishina formulas used for Compton sampling.
#pragma once
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lmparticle {

constexpr double kElectronMassMeV = 0.51099895;  // electron rest energy
constexpr double kMinEnergy = 0.001;             // MeV: lower limit of the tables
constexpr double kMaxEnergy = 20.0;              // MeV: upper limit of the tables
enum Channel { kRayleigh = 0, kCompton = 1, kPhoto = 2, kPair = 3, kChannels = 4 };

// mass coefficients, cm^2/g, at one energy: coherent, incoherent, photoelectric, pair (nuclear + electron field)
using Channels = std::array<double, kChannels>;

struct Element {
  int z = 0;
  std::string symbol;
  std::vector<double> energy;           // MeV; an absorption edge appears twice (below, then above)
  std::vector<Channels> mass_coeff;     // cm^2/g per row
  Channels at(double e) const;          // log-log interpolation in energy
};
const Element &element(const std::string &symbol);  // throws on an unknown symbol
std::vector<std::string> element_symbols();

// A material with its attenuation tables on the union of its elements' energy grids (linear, 1/cm).
class Material {
 public:
  Material(std::string name, double density, std::vector<std::pair<std::string, double>> mass_fractions);
  const std::string &name() const { return name_; }
  double density() const { return density_; }
  const std::vector<std::pair<std::string, double>> &fractions() const { return fractions_; }  // normalized
  Channels mu(double e) const;                    // linear attenuation by process, 1/cm
  double mu_total(double e) const;
  Channels mass_coeff(double e) const;            // same per gram
  // Element of the material hit by a coherent scattering at energy e, chosen with probability u in [0,1).
  int pick_rayleigh_element(double e, double u) const;
  int element_z(int i) const { return elements_[i]->z; }
 private:
  std::string name_;
  double density_;
  std::vector<std::pair<std::string, double>> fractions_;
  std::vector<const Element *> elements_;
  std::vector<double> weights_;                   // normalized mass fractions
  std::vector<double> energy_;
  std::vector<Channels> mu_;
};

std::shared_ptr<const Material> builtin_material(const std::string &name);
std::vector<std::string> builtin_names();

// Klein-Nishina total cross section per free electron, cm^2.
double klein_nishina_total(double e_mev);
// Mean fraction of the photon energy transferred to the electron by one Compton scatter (Klein-Nishina).
double compton_mean_transfer(double e_mev);

}  // namespace lmparticle
