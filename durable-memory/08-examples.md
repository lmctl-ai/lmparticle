# Examples

`examples/*.lms` assert their own results and write CSVs under `build/examples/`; `examples/combo/*.lms`
run a simulation then draw with lmplot. Notes from writing them:

- The DSL has no exponent literals, `exp`, `log`, ternary or `mkdir`: strings are built in loops and written with
  `rad.save_text`; `build/examples` is created by `test.sh` (the combo scripts call lmscript's `mkdir`).
- Depth-dose profiles are kerma (no electron transport): the first layer of a Co-60 beam in water gets about
  0.040 MeV per photon and the 30th 0.014, and scatter makes it fall slower than exp(-mu x).
- The NaI spectrum shows the Compton edge at 477 keV and a photopeak holding 93% of pulses of the large crystal;
  without electron/bremsstrahlung escape the peak is sharper than a real detector's (no resolution broadening).

## Pictures-first experiments

README opens with six generated visual results; `docs/gallery.md` covers all original examples
and eight new self-contained scripts under `examples/visual/`: material_attenuation,
interaction_crossover, half_value_layers, compton_transfer, shield_buildup, water_hardening,
layer_order, shield_geometry. Captions give raw-main `lmctl autorun` commands. Each script binds
released hosts, executes its bytecode and creates images in its current working directory.
An explicit output argument to the original scripts keeps their compile-only test interface.

Run `LMPARTICLE_TEST_NETWORK=1 examples/render-all.sh` to regenerate every committed image.
The pipeline is lmparticle (XCOM coefficients or seeded transport) → CSV under ignored
`build/visuals/` or `build/examples/` → lmplot measurements / lmcad finite shield geometry.
Shield geometry shows four separate simulations; brighter exit-beam glyphs encode higher
uncollided transmission, not sampled tracks. The buildup GIF reveals the 25 computed thickness
points with fixed axes; ffmpeg is an optional external executable, with an explicit skip when
absent. No new linked libraries or simulation behavior changes were introduced.

`test.sh` validates README and gallery references, companion coverage, PNG/GIF magic and size,
and regenerates images when sibling hosts are available. Missing siblings print an explicit skip.
Seeded results repeat on one platform; fonts/rendered bytes are not promised identical across OSes.
