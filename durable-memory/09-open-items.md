# Open items

- Electron and positron transport (condensed history with CSDA ranges, bremsstrahlung, delta rays): the biggest
  gap; would make depth-dose and thin-layer dose real. Tables: NIST ESTAR/PSTAR.
- Fluorescence (K x-rays after photoabsorption), Doppler broadening, binding-corrected Compton angles, better
  Rayleigh form factors (NIST/Hubbell tables), annihilation in flight, bremsstrahlung.
- Neutrons and nuclear data (ENDF/B-VIII is public domain but large): out of scope for v0.
- More geometry: cylinders, spheres, rotated boxes; finite-area plane tallies; surface tallies by region face.
- Variance reduction (implicit capture, Russian roulette, splitting) for deep shielding; `run` is analog.
- More elements (the table holds 19) and materials (bone, ICRU compounds); a user-supplied table loader.
- Threads for speed (would need per-thread streams, to keep reproducibility).
