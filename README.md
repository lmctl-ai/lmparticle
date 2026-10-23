# lmparticle

Seeded Monte Carlo photon transport with NIST XCOM cross sections: shielding, detectors, imaging and dosimetry
questions answered by a 2.5 MB executable that an `lmctl autorun` command fetches for you. Every picture below is
made by the example it links to; run the command under it from any empty directory.

### Shielding design

| Tenth-value layers | Source in a lead pot | Streaming through a slit |
| --- | --- | --- |
| ![Tenth-value layers](docs/images/tenth_value_layers.png) | ![Kerma versus distance](docs/images/dose_rate_distance.png) | ![Streaming through a slit](docs/images/streaming_gap.png) |
| [Narrow-beam tables understate the wall: buildup adds a third (lead) to two thirds (concrete)](examples/visual/tenth_value_layers.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/tenth_value_layers.lms` | [Inverse square times exp(-mu t), plus what scatters out of the lead and off the air](examples/visual/dose_rate_distance.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/dose_rate_distance.lms` | [A 4 mm slit in 4 cm of lead passes 50 times the kerma of the wall next to it](examples/visual/streaming_gap.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/streaming_gap.lms` |

| Scatter buildup, 25 thicknesses | Slit geometry (lmdraw) | Layer order |
| --- | --- | --- |
| ![Scatter buildup](docs/images/shield-buildup.gif) | ![Slit layout](docs/images/streaming_layout.png) | ![Layer order energy deposition](docs/images/layer_order.png) |
| [Uncollided photons follow exp(-mu x); all forward crossings do not](examples/visual/shield_buildup.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/shield_buildup.lms` | [The wall, the slit, the broad beam and the 25 detector cells, to scale](examples/visual/streaming_gap.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/streaming_gap.lms` | [Lead-then-aluminium and aluminium-then-lead transmit the same but absorb differently](examples/visual/layer_order.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/layer_order.lms` |

### Detectors and spectra

| NaI(Tl) efficiency | Backscatter peak | How the peaks arise (lmdraw) |
| --- | --- | --- |
| ![NaI efficiency](docs/images/nai_efficiency.png) | ![Backscatter peak](docs/images/backscatter_peak.png) | ![Backscatter geometry](docs/images/backscatter_geometry.png) |
| [3 x 3 inch crystal at 10 cm: total and photopeak efficiency against solid angle x (1 - exp(-mu L))](examples/visual/nai_efficiency.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/nai_efficiency.lms` | [A lead block behind a Cs-137 source adds the 184 keV bump; the Compton edge stays at 477 keV](examples/visual/backscatter_peak.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/backscatter_peak.lms` | [The two 180-degree scatters that make the backscatter peak and the Compton edge](examples/visual/backscatter_peak.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/backscatter_peak.lms` |

### Imaging and dosimetry

| Radiographic contrast | Phantom layout (lmdraw) | X-ray tube filtration |
| --- | --- | --- |
| ![Radiography contrast](docs/images/radiography_contrast.png) | ![Radiography layout](docs/images/radiography_layout.png) | ![Tube filtration](docs/images/tube_filtration.png) |
| [2 cm of bone: 55% contrast at 60 keV, 27% at 120 keV, and scatter fog halves both](examples/visual/radiography_contrast.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/radiography_contrast.lms` | [Tissue block, bone rod, 25 pencil-beam positions and the detector plane](examples/visual/radiography_contrast.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/radiography_contrast.lms` | [2.5 mm Al and 0.2 mm Cu remove the soft photons: mean energy and HVL rise, intensity falls](examples/visual/tube_filtration.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/tube_filtration.lms` |

### Physics of photon interactions

| Photon tracks | Which interaction dominates | Buildup factor |
| --- | --- | --- |
| ![Photon tracks](docs/images/photon_tracks.png) | ![Dominance map](docs/images/dominance_map.png) | ![Buildup factor](docs/images/buildup_factor.png) |
| [60 recorded histories in water and in lead, coloured by energy](examples/visual/photon_tracks.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/photon_tracks.lms` | [Photoelectric, Compton and pair-production crossover energies for all 19 elements of the table](examples/visual/dominance_map.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/dominance_map.lms` | [Point source in water, iron, lead: 2.09 and 3.59 at 1 and 2 mfp against the published 2.08 and 3.62](examples/visual/buildup_factor.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/buildup_factor.lms` |

| Tracks accumulating | Monte Carlo convergence | Water hardening |
| --- | --- | --- |
| ![Tracks animation](docs/images/photon-tracks.gif) | ![Convergence](docs/images/convergence.png) | ![Water hardening](docs/images/water_hardening.png) |
| [662 keV photons entering water, two histories per frame](examples/visual/photon_tracks.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/photon_tracks.lms` | [Error falls as 1/sqrt(N); implicit capture needs fewer histories for the same error](examples/visual/convergence.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/convergence.lms` | [Water removes the 80 keV beam five times faster than the 662 keV beam](examples/visual/water_hardening.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/water_hardening.lms` |

The [complete gallery](docs/gallery.md) has all 25 examples with their pictures and commands, including
material attenuation curves, half-value layers, Compton energy transfer, the NaI pulse-height spectrum, the
pair-production spectrum of 6 MeV photons in lead, the Co-60 depth profile and the finite-shield render (lmcad).

## Install and run

Install [lmctl](https://lmctl.com/tools/lmscript/skill.html) (it brings `lmscript` and `lmctl autorun`). Then copy
any command from the gallery, or run a local file:

```sh
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/visual/backscatter_peak.lms
lmscript examples/visual/backscatter_peak.lms           # from a checkout
LMPARTICLE_TEST_NETWORK=1 examples/render-all.sh        # regenerate every docs/images file
```

The `install=` bindings fetch released hosts (lmparticle, [lmplot](https://github.com/lmctl-ai/lmplot),
[lmdraw](https://github.com/lmctl-ai/lmdraw), [lmcad](https://github.com/lmctl-ai/lmcad)); binaries on `PATH` take
priority. Every example runs from an empty working directory and writes its images under `docs/images/` there.
GitHub rate-limits anonymous release lookups (60 per hour): export a `GITHUB_TOKEN` if `install` reports HTTP 403.

```
# shield.lms: how much of a 662 keV (Cs-137) beam gets through 1 cm of lead without interacting?
p = _dsl_ cli="lmparticle" install="github:lmctl-ai/lmparticle@latest";
let pb = rad.builtin("lead");
let g = rad.geometry();
g.slab("shield", pb, 0.0, 1.0);                       # z from 0 to 1 cm, infinite sideways
let s = rad.source();
s.point(0.0, 0.0, -1.0).beam(0.0, 0.0, 1.0).energy(0.662);
let r = rad.simulation(g, s).plane("exit", "z", 1.0).seed(1).run(200000);
rad.print("uncollided " + str(r.plane_uncollided("exit")) + " +- " + str(r.plane_uncollided_err("exit")));
rad.assert_close(r.plane_uncollided("exit"), pow(2.718281828459045, -pb.mu(0.662)), 0.005);   # exp(-mu x)
end _dsl_
savedsl(p, "shield.dslb");
system ${dsl_cli(p)} --dsl shield.dslb;
```

`lmscript shield.lms` finds `lmparticle` on `PATH` or downloads the release binary for your platform on
first use, compiles against the schema built into it, and runs the program.

## What it does

Energies are in MeV, lengths in cm, densities in g/cm3. Everything is analog Monte Carlo with a seeded
xoshiro256** generator: the same program gives the same numbers, bit for bit, on one platform.

| `rad.` | |
| --- | --- |
| `builtin("water")` `material(name, density)` → `Material` | builtin: water air lead iron aluminum copper tungsten silicon germanium concrete nai polyethylene tissue bone pmma; custom: `.element("Pb", mass_fraction)` for H C N O Na Mg Al Si P S Ar K Ca Fe Cu Ge I W Pb, or any element given with `element_table(symbol, Z, text)` (XCOM rows: `E coherent incoherent photo pair_nuclear pair_electron`) |
| `Material` | `.mu(E)` 1/cm, `.mu_mass(E)` cm2/g, `.mu_partial(E, "rayleigh"\|"compton"\|"photo"\|"pair")`, `.mfp(E)`, `.hvl(E)`, `.density() .name()` |
| `geometry()` → `Geometry` | `.box(name, material, x0, x1, y0, y1, z0, z1)` `.slab(name, material, z0, z1)` `.sphere(name, material, cx, cy, cz, r)` `.cylinder(name, material, cx, cy, r, z0, z1)` (axis along z); earlier regions win where they overlap, outside is vacuum |
| `source()` → `Source` | `.point(x, y, z)` `.isotropic()` `.beam(dx, dy, dz)` `.cone(dx, dy, dz, half_angle_deg)` `.disc(radius)` (start points spread over a disc perpendicular to the beam) `.energy(E)` `.line(E, weight)` |
| `simulation(geometry, source)` → `Sim` | `.plane(name, "x"\|"y"\|"z", pos)` `.spectrum(bins, emax)` `.pulse_height(region)` `.cutoff(E)` `.no_rayleigh()` `.implicit_capture(weight_cutoff)` `.tracks(histories)` `.seed(n)` `.run(histories)` → `Result` |
| `Result` | `.deposited(region)` `.deposited_uncollided(region)` `.total_deposited()` `.escaped()` `.escaped_uncollided()` `.escaped_energy()` `.plane_count(p)` `.plane_uncollided(p)` `.plane_energy(p)` `.interactions(process)` `.pulse_fraction(region, lo, hi)` (each with `_err` where it has a statistical error), `.save_regions .save_plane_spectrum .save_leakage_spectrum .save_pulse_height .save_tracks` |
| physics helpers | `klein_nishina(E)` cm2/electron, `compton_mean_transfer(E)`, `sample_compton_mean(E, n, seed)` |
| checks and output | `assert_close(a, b, tol)` `assert_true(bool)` `print(s)` `save_text(path, text)` `version()` |

`deposited` is the energy handed to electrons and positrons at the photon interaction points, which is
**collision kerma**, not absorbed dose: charged particles are not transported (see Scope).
`deposited_uncollided` is the part of it from photons at their first interaction, so
`deposited / deposited_uncollided` is the kerma buildup factor of a region.
`implicit_capture(w)` replaces photoabsorption by a weight reduction with Russian roulette below weight `w`
(same means, smaller errors deep in a shield; not allowed with `pulse_height`).
`tracks(n)` records the interaction vertices of the first `n` histories for `save_tracks(path)`.

All tallies are per source photon with the standard error of the mean. Physics, models and their limits:
[durable-memory/03-physics-and-limits.md](durable-memory/03-physics-and-limits.md); schema:
[durable-memory/04-schema-v0.md](durable-memory/04-schema-v0.md). The host stops with exit 3 instead of
exhausting memory: `LMPARTICLE_MAX_MEMORY_MB` (default 4096) is the budget.

Combine it with other hosts in one script: several `_dsl_` blocks, one per host. The gallery examples simulate
with lmparticle, then draw with lmplot (curves, spectra, multi-panel figures, GIF frames), lmdraw (layouts and
annotated diagrams) or lmcad (3D renders); lmscript glues them, reading the CSV files back where a figure needs
derived numbers.

## Examples

All 25 examples are self-contained (they bind released hosts, execute their own bytecode and write images under
the current directory) and assert a known answer before drawing. The [gallery](docs/gallery.md) lists each one with
its picture and autorun command. The four original programs under `examples/` also take an output argument for
compile-only use in `test.sh`:

| Program | Shows |
| --- | --- |
| [shielding.lms](examples/shielding.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/shielding.lms` | Cs-137 through 0.25-3.25 cm of lead: the uncollided beam follows exp(-mu x), HVL and TVL |
| [depth_dose.lms](examples/depth_dose.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/depth_dose.lms` | layer-by-layer kerma of a Co-60 beam in a 30 cm water phantom |
| [detector_response.lms](examples/detector_response.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/detector_response.lms` | pulse-height spectrum of a NaI crystal: photopeak, Compton continuum and edge |
| [pair_spectrum.lms](examples/pair_spectrum.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/pair_spectrum.lms` | what leaves lead after 6 MeV photons: pair production and the 511 keV line |
| [combo/shielding_plot.lms](examples/combo/shielding_plot.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/combo/shielding_plot.lms`, [combo/detector_spectrum.lms](examples/combo/detector_spectrum.lms)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmparticle/main/examples/combo/detector_spectrum.lms` | lmparticle simulates, lmplot draws |

`tests/` asserts known answers: XCOM published coefficients, the Klein-Nishina cross section and mean
energy transfer, narrow-beam exp(-mu x) through layered, oblique, spherical and cylindrical geometries, the
solid angle of a detector seen from an isotropic source and from a cone, region priority, energy conservation to
1e-9, the pair threshold and annihilation photons, buildup trends, the Compton edge, implicit capture against
analog transport, and bitwise reproducibility.

## Build

[![CI](https://github.com/lmctl-ai/lmparticle/actions/workflows/ci.yml/badge.svg)](https://github.com/lmctl-ai/lmparticle/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

C++17 compiler, CMake 3.24; nothing else (no third-party sources).

```sh
cmake -S . -B build && cmake --build build     # about 20 seconds
./test.sh                                      # needs lmscript on PATH
```

Linux builds are fully static; macOS links only system libraries; Windows links the MinGW runtime
statically. `lmparticle --notices` prints the XCOM citation and the licenses.

## Scope

Photons only: electrons and positrons deposit their energy where they are produced (collision kerma; no
bremsstrahlung, no fluorescence, no Doppler broadening), and positrons annihilate at rest. That is accurate for
attenuation, shielding, buildup and detector counting questions with photons above a few tens of keV in low and
medium Z; it is not a substitute for Geant4 where charged-particle ranges matter (absorbed dose in thin layers,
build-up regions, electron escape from small detectors). Geometry is axis-aligned boxes, spheres and z-axis
cylinders; the source is a point or a disc, isotropic, a pencil beam or a cone. See
[durable-memory/03-physics-and-limits.md](durable-memory/03-physics-and-limits.md).

## License

MIT ([LICENSE](LICENSE)). The embedded cross sections are NIST XCOM data (public data of the U.S.
Government, cited in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)).
