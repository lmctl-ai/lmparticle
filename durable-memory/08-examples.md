# Examples

`examples/*.lms` assert their own results and write CSVs under `build/examples/`; `examples/combo/*.lms`
run a simulation then draw with lmplot. Notes from writing them:

- The DSL has no exponent literals, `exp`, `log`, ternary or `mkdir`: strings are built in loops and written with
  `rad.save_text`; `build/examples` is created by `test.sh` (the combo scripts call lmscript's `mkdir`).
- Depth-dose profiles are kerma (no electron transport): the first layer of a Co-60 beam in water gets about
  0.040 MeV per photon and the 30th 0.014, and scatter makes it fall slower than exp(-mu x).
- The NaI spectrum shows the Compton edge at 477 keV and a photopeak holding 93% of pulses of the large crystal;
  without electron/bremsstrahlung escape the peak is sharper than a real detector's (no resolution broadening).
