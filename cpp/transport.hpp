// Analog Monte Carlo photon transport in a set of axis-aligned boxes (slabs are boxes that are infinite sideways).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "xs.hpp"

namespace lmparticle {

struct Box {
  std::string name;
  std::shared_ptr<const Material> material;
  double lo[3], hi[3];
};
struct Geometry {
  std::vector<Box> boxes;  // earlier boxes take priority where boxes overlap; outside every box is vacuum
};
struct Source {
  double pos[3] = {0, 0, 0};
  bool isotropic = true;
  double dir[3] = {0, 0, 1};
  std::vector<std::pair<double, double>> lines;  // energy MeV, weight
};
struct Plane { std::string name; int axis; double position; };
struct Config {
  std::vector<Plane> planes;
  std::vector<int> pulse_regions;  // boxes whose per-history deposited energy is histogrammed (pulse height)
  int bins = 50;
  double emax = 0;          // spectrum upper edge, MeV; 0 = highest source line
  double cutoff = kMinEnergy;
  bool rayleigh = true;
  uint64_t seed = 1;
};

// One tally quantity with per-history statistics: mean and standard error of the mean per primary history.
struct Stat { double mean = 0, err = 0; };

struct Result {
  int64_t histories = 0;
  double source_energy = 0;  // mean energy per primary actually emitted (sampled lines)
  std::vector<std::string> region_names, plane_names, pulse_names;
  std::vector<Stat> deposited;                       // MeV per primary, per box
  Stat escaped, escaped_uncollided, escaped_energy;  // photons leaving the geometry
  std::vector<Stat> plane_count, plane_uncollided, plane_energy;      // forward crossings per primary
  std::vector<std::vector<Stat>> plane_spectrum;     // [plane][bin], crossings per primary
  std::vector<Stat> leak_spectrum;
  std::vector<std::vector<Stat>> pulse_height;       // [pulse region][bin], histories per primary with that deposit
  Stat interactions[kChannels];                      // per primary
  int bins = 0;
  double emax = 0;
  Stat total_deposited;
};

Result run(const Geometry &, const Source &, const Config &, int64_t histories);

// Sampling pieces exposed for tests.
struct Rng {
  uint64_t s[4];
  explicit Rng(uint64_t seed);
  uint64_t next();
  double uniform();  // [0, 1)
};
double sample_compton(double e, Rng &rng, double *cos_theta);  // returns scattered energy

}  // namespace lmparticle
