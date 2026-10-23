# Open items

- Electron and positron transport (condensed history with CSDA ranges, bremsstrahlung, delta rays): the biggest
  gap; would make depth-dose and thin-layer dose real. Tables: NIST ESTAR/PSTAR.
- Fluorescence (K x-rays after photoabsorption), Doppler broadening, binding-corrected Compton angles, better
  Rayleigh form factors (NIST/Hubbell tables), annihilation in flight, bremsstrahlung.
- Neutrons and nuclear data (ENDF/B-VIII is public domain but large): out of scope for v0.
- More geometry: rotated boxes, cylinders along x or y, cones; finite-area plane tallies; surface tallies by
  region face. (Spheres and z cylinders, cone and disc sources: done.)
- More variance reduction: geometry splitting and importances, forced collisions, exponential transform;
  implicit capture with roulette is in. `Sim.run` is single-threaded.
- More builtin materials (ICRU compounds beyond bone and PMMA); the 19-element table can be extended at run time
  with `element_table`, but there is no bundled loader for a whole XCOM download.
- Dose: `deposited` is collision kerma; absorbed dose needs electron transport (first item).
- Threads for speed (would need per-thread streams, to keep reproducibility).
