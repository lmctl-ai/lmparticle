# Schema v0

One context, `rad: Rad`. Object types: `Material`, `Geometry`, `Source`, `Sim`, `Result`.
`schema/lmparticle.dsl.json` is generated from the `method(r, ...)` registrations in `cpp/main.cpp` by
`scripts/gen-schema.py`; `test.sh` fails when it is stale and diffs it against `lmscript --lmcode-signatures`.

Conventions: MeV, cm, g/cm3. Builder methods return the receiver (chain them); `Source.energy` replaces the
spectrum, `Source.line` adds a weighted line (sampled per history). `Sim.run` may be called repeatedly (each call
restarts from the seed). Regions and planes are addressed by name (unknown names are errors listing the known
ones). The DSL has no exponent literals (`1e-24` is invalid) and no `exp`/`log`: write
`pow(2.718281828459045, -x)` and decimal digits.

`Result` getters: values per source photon; `_err` variants are the standard error of the mean.
`plane_count` counts forward crossings (positive direction along the axis) of the infinite plane,
`plane_uncollided` those of photons that never interacted. `pulse_fraction(region, lo, hi)` sums the
bins whose lower edge lies in `[lo, hi)`: bin resolution is `emax/bins` set by `spectrum`.
Energies above `emax` are not tallied in spectra or pulse-height histograms (no pile-up in the last bin);
`emax` itself falls in the last bin. Nonfinite numbers (inf from `pow` overflow, NaN) are refused for
densities, mass fractions, source positions, line weights and plane positions (exit 3).

CSV outputs are space-separated with `%.10g`: regions `name deposit err`; spectra
`e_low e_high value err` (gnuplot reads them directly); tracks `history x y z dx dy dz energy weight event`
with a `#` header, `energy` being the energy leaving the vertex (0 where the photon ended) and `event` one of
source, annihilation, compton, rayleigh, photo, pair, escape, cutoff, roulette.

Added after v0.1.0 (schema still v2, additive): `Geometry.sphere(name, m, cx, cy, cz, r)`,
`Geometry.cylinder(name, m, cx, cy, r, z0, z1)`; `Source.cone(dx, dy, dz, half_angle_deg)` (0 = pencil beam),
`Source.disc(radius)`; `Sim.implicit_capture(weight_cutoff)` (0 < w < 1, refused together with `pulse_height`),
`Sim.tracks(n)` (0..100000); `Result.deposited_uncollided(region)` (+`_err`), `Result.track_points()`,
`Result.save_tracks(path)`; `Rad.element_table(symbol, Z, text)` adds or replaces an element from XCOM-style rows
`E coherent incoherent photo pair_nuclear pair_electron` (cm2/g; ascending energies covering 0.001-20 MeV; a
repeated energy is an edge; commas allowed; `#` comments); builtin materials `bone` (ICRU-44 cortical, 1.92)
and `pmma` (1.19).
