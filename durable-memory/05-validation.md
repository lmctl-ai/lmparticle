# Validation

Every test asserts a closed form or a published value, not a number taken from this code.

- **XCOM values** (`tests/cross_sections.lms`): water mu/rho 5.329 (10 keV), 0.1707, 0.07072, 0.02219 cm2/g at
  0.1, 1, 10 MeV; lead 5.549 and 0.07102 at 0.1 and 1 MeV, 0.1102 at 662 keV; iron 0.3717/0.05995; aluminum
  0.1704/0.06146; air 0.06358; NaI 0.05881 at 1 MeV. The mixture rule and the Pb K edge (88.0 keV, jump x4).
  Hydrogen's incoherent coefficient at 1 MeV equals Klein-Nishina per electron within 0.1% (binding).
- **Klein-Nishina** (`tests/klein_nishina.lms`, `tests/core.cpp`): 0.2112 b at 1 MeV, 0.2866 b at 511 keV,
  Thomson limit 0.6652 b; mean energy transfer 0.4400 at 1 MeV; the sampler's mean E'/E equals the numerical
  integral at five energies (2e6 samples, 1e-3) and the sampled angle satisfies the Compton relation exactly.
  The sampled mean cos(theta) equals the Simpson integral of dsigma/dOmega at 10 keV-10 MeV (4e5 samples, 0.004).
- **Narrow beam** (`tests/attenuation.lms`): uncollided transmission = exp(-mu x) within 4 standard errors for
  one slab (lead 662 keV), a half-value layer (0.5), layered media with a vacuum gap, an oblique beam
  (x/cos), a finite box, a source inside the slab, and with Rayleigh switched off.
- **Geometry** (`tests/geometry.lms`): region priority (brick-first vs tank-first), solid angle of a
  20x20 cm detector at 50 cm: 0.0122457 (exact formula) seen from an isotropic source, box == slab.
- **Bookkeeping** (`tests/conservation.lms`): deposited + escaped energy = emitted to 1e-9 in finite and infinite
  media, with pair production, polychromatic sources and a cutoff; a thin film deposits P(interact) x 0.440 MeV.
- **Pair production** (`tests/pair_production.lms`): zero events below 1.022 MeV; Compton/photo and pair/Compton
  first-interaction ratios follow the tables; a line at 511 keV in the leakage spectrum.
- **Buildup** (`tests/buildup.lms`): grows with thickness (3.3 at 1.8 mfp, 7.6 at 4 mfp for 1 MeV in water by
  forward-crossing counts), smaller in lead than water at equal mfp.
- **Pulse height** (`tests/pulse_height.lms`): no counts above the Compton edge 2E2/(m+2E) = 0.4773 MeV for a thin
  target; the photopeak holds 93% of a large NaI crystal's pulses.
- **Reproducibility** (`tests/determinism.lms`, test.sh rerun): identical bits for the same seed, different for another.

Tolerances are 4 standard errors (statistical) or the table precision (4 digits). A failing case prints both
numbers.
