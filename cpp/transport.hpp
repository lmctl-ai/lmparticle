// Analog Monte Carlo photon transport in a set of regions: axis-aligned boxes (slabs are boxes that are infinite
// sideways), spheres and z-axis cylinders.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "xs.hpp"

namespace lmparticle {

enum Shape { kBoxShape = 0, kSphereShape = 1, kCylinderShape = 2 };
struct Box {
  std::string name;
  std::shared_ptr<const Material> material;
  double lo[3], hi[3];           // box bounds; cylinder: lo[2]..hi[2] is the z extent
  Shape shape = kBoxShape;
  double center[3] = {0, 0, 0};  // sphere center; cylinder axis x, y (center[2] unused)
  double radius = 0;             // sphere or cylinder radius
};
struct Geometry {
  std::vector<Box> boxes;  // earlier regions take priority where they overlap; outside every region is vacuum
};
struct Source {
  double pos[3] = {0, 0, 0};
  bool isotropic = true;
  double dir[3] = {0, 0, 1};
  double cone_cos = 1;        // beam: cos of the half-angle (1 = pencil beam), emission uniform in solid angle
  double disc_radius = 0;     // > 0: start points uniform on a disc of this radius, perpendicular to the beam axis
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
  double weight_cutoff = 0;  // > 0: implicit capture (photoabsorption reduces the weight) with Russian roulette below it
  int64_t track_histories = 0;  // > 0: record the interaction vertices of the first n histories
};

// One tally quantity with per-history statistics: mean and standard error of the mean per primary history.
struct Stat { double mean = 0, err = 0; };

// One vertex of a recorded photon track.
struct TrackPoint { int64_t history; double x[3], d[3], e, weight; const char *event; };

struct Result {
  int64_t histories = 0;
  double source_energy = 0;  // mean energy per primary actually emitted (sampled lines)
  std::vector<std::string> region_names, plane_names, pulse_names;
  std::vector<Stat> deposited;                       // MeV per primary, per region
  std::vector<Stat> deposited_uncollided;            // the part deposited by photons at their first interaction
  Stat escaped, escaped_uncollided, escaped_energy;  // photons leaving the geometry
  std::vector<Stat> plane_count, plane_uncollided, plane_energy;      // forward crossings per primary
  std::vector<std::vector<Stat>> plane_spectrum;     // [plane][bin], crossings per primary
  std::vector<Stat> leak_spectrum;
  std::vector<std::vector<Stat>> pulse_height;       // [pulse region][bin], histories per primary with that deposit
  Stat interactions[kChannels];                      // per primary
  int bins = 0;
  double emax = 0;
  Stat total_deposited;
  std::vector<TrackPoint> tracks;
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
void rotate(double d[3], double cos_theta, double phi);         // deflects the unit vector d by theta about itself, azimuth phi
bool inside(const Box &, const double x[3]);

}  // namespace lmparticle
