# Physics and limits

**Photon interactions** (analog; every photon is followed until absorbed, below the cutoff or lost):

- Total and partial cross sections come from the material's attenuation tables: for each material the
  elements' energy grids are merged (edges keep their below/above pair) and log-log interpolated.
- *Photoelectric*: the photon is absorbed; its whole energy is deposited at the interaction point.
  No K/L fluorescence.
- *Compton (incoherent)*: free-electron Klein-Nishina angular and energy distribution, sampled exactly
  (EGS4/Geant4 decomposition, validated against the numerical Klein-Nishina mean to 1e-3 at 0.02-6 MeV);
  the electron's kinetic energy is deposited locally. The *total* incoherent cross section is XCOM's,
  which includes binding (scattering function); the angular distribution does not (no binding or Doppler
  broadening: matters below ~100 keV in high Z).
- *Rayleigh (coherent)*: total from XCOM; angle sampled from Thomson (1+cos2)/2 times a Thomas-Fermi
  screened form factor squared, F=1/(1+(q a)^2), a=0.468 A/Z^(1/3), with the scattering element chosen in
  proportion to its coherent cross section. Approximate; `Sim.no_rayleigh()` removes the channel entirely.
- *Pair production* (nuclear + electron field summed): the kinetic energy E-1.022 MeV is deposited at
  the point; the positron annihilates at rest into two back-to-back isotropic 511 keV photons that are
  transported (marked collided). No annihilation in flight, no triplet kinematics.
- Photons below the cutoff (default and minimum 1 keV: the table limit) are absorbed on the spot. Maximum
  energy 20 MeV.

**Charged particles are not transported**: electrons and positrons deposit locally (kerma). Consequences:
no electron-equilibrium buildup in depth-dose curves (they show kerma), no escape of secondary electrons
from thin or small detectors (pulse-height spectra have no electron-escape or bremsstrahlung tails), no
bremsstrahlung photons. Good for attenuation, shielding, buildup, energy-balance and detector counting
questions with photons from tens of keV to a few MeV in low to medium Z; unsuitable for dose in thin layers
of high Z, charged-particle ranges, or radiotherapy dosimetry.

**Geometry**: a list of regions: axis-aligned boxes (`slab` is a box infinite in x and y), spheres and cylinders
with their axis along z. Earlier regions win where they overlap (the distance to the next boundary includes
entering any higher-priority region). Outside all regions is vacuum: photons travel on, and leave for good when no
region lies ahead. After reaching a surface the photon is placed 1e-8 cm across it: for a planar face the hit
coordinate is snapped to the face and pushed across along that axis, for a curved surface the radius is set to
R ± 1e-8. (A push along the flight direction alone was lost to rounding at grazing angles on an inclusive low face,
and such a history then looped until the 1e8-step guard; `tests/core.cpp` runs 200 grazing exits.) No rotated boxes,
no cylinders along x or y.

**Sources**: point or disc (uniform over a disc perpendicular to the beam axis, or to z when isotropic); direction
isotropic, pencil beam, or uniform in solid angle within a cone of a given half-angle.

**Variance reduction** (`Sim.implicit_capture(w)`): at every collision the photoelectric share mu_photo/mu_total
of the weight is deposited and the photon continues with the rest, choosing among Rayleigh, Compton and pair; below
weight `w` Russian roulette kills half and doubles the other half. Tallies are weighted. Means are unchanged,
errors deep in a shield are smaller; energy is conserved only in expectation; pulse-height tallies are refused
(a weighted per-history deposit is not a pulse height).

**Direction change**: the deflection is applied with the standard rotation about the current direction; the
transverse length q = sqrt(u^2 + v^2) is computed directly and only below 1e-10 is the z axis used instead, so
the shortcut's direction error is at most 1e-10 rad (it used to be taken within 0.26 degrees of the axis).

**Tallies** (all per source photon, with the standard error of the mean from per-history sums, so a history that
contributes several times, e.g. two plane crossings, is handled correctly):
region energy deposition (collision kerma: what electrons and positrons receive at the interaction points) and
its first-collision part (deposited by photons that had not interacted before, whichever process; the ratio
deposited / deposited_uncollided is the kerma buildup factor of the region); infinite-plane forward crossings
(count, uncollided count, energy, spectrum); leakage (count, uncollided count, energy, spectrum); interactions by
process; per-history pulse height in a region; optional track vertices of the first n histories.
"Uncollided" means no interaction of any kind since birth; annihilation photons are always collided.

**Random stream**: the Rayleigh angle and azimuth are drawn in a fixed order (two statements), not as two
arguments of one call, because C++ leaves argument evaluation order to the compiler and GCC and Clang differ.

**Reproducibility**: single thread; xoshiro256** seeded through splitmix64; one stream per run. Same program,
same binary, same platform: identical bits. Across platforms the integer random stream is identical and only
libm differences (log, exp, sin, cos in the last ulp) can change a rare branch, so cross-platform comparisons use
statistical tolerances (the tests assert 4 standard errors).
