# Schema v0

One context, `rad: Rad`. Object types: `Material`, `Geometry`, `Source`, `Sim`, `Result`.
`schema/lmparticle.dsl.json` is generated from the `method(r, ...)` registrations in `cpp/main.cpp` by
`scripts/gen-schema.py`; `test.sh` fails when it is stale and diffs it against `lmscript --dsl-signatures`.

Conventions: MeV, cm, g/cm3. Builder methods return the receiver (chain them); `Source.energy` replaces the
spectrum, `Source.line` adds a weighted line (sampled per history). `Sim.run` may be called repeatedly (each call
restarts from the seed). Regions and planes are addressed by name (unknown names are errors listing the known
ones). The DSL has no exponent literals (`1e-24` is invalid) and no `exp`/`log`: write
`pow(2.718281828459045, -x)` and decimal digits.

`Result` getters: values per source photon; `_err` variants are the standard error of the mean.
`plane_count` counts forward crossings (positive direction along the axis) of the infinite plane,
`plane_uncollided` those of photons that never interacted. `pulse_fraction(region, lo, hi)` sums the
bins whose lower edge lies in `[lo, hi)`: bin resolution is `emax/bins` set by `spectrum`.

CSV outputs are space-separated with `%.10g`: regions `name deposit err`; spectra
`e_low e_high value err` (gnuplot reads them directly).
