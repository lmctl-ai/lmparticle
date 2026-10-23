# lmparticle

**Monte Carlo radiation transport for lmscript in one static executable: photons through slabs and
boxes with NIST XCOM cross sections, Klein-Nishina Compton scattering, pair production, and tallies to
CSV.** It is a scripting host, not a command-line tool: write an
[lmscript](https://lmctl.com/tools/lmscript/skill.html) `_dsl_` block, compile it, run it with
`lmparticle --dsl`. No data files, no Geant4, no Python, no shared libraries.

[![CI](https://github.com/lmctl-ai/lmparticle/actions/workflows/ci.yml/badge.svg)](https://github.com/lmctl-ai/lmparticle/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

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
| `builtin("water")` `material(name, density)` → `Material` | builtin: water air lead iron aluminum copper tungsten silicon germanium concrete nai polyethylene tissue; custom: `.element("Pb", mass_fraction)` for H C N O Na Mg Al Si P S Ar K Ca Fe Cu Ge I W Pb |
| `Material` | `.mu(E)` 1/cm, `.mu_mass(E)` cm2/g, `.mu_partial(E, "rayleigh"\|"compton"\|"photo"\|"pair")`, `.mfp(E)`, `.hvl(E)`, `.density() .name()` |
| `geometry()` → `Geometry` | `.box(name, material, x0, x1, y0, y1, z0, z1)` `.slab(name, material, z0, z1)`; earlier regions win where they overlap, outside is vacuum |
| `source()` → `Source` | `.point(x, y, z)` `.isotropic()` `.beam(dx, dy, dz)` `.energy(E)` `.line(E, weight)` |
| `simulation(geometry, source)` → `Sim` | `.plane(name, "x"\|"y"\|"z", pos)` `.spectrum(bins, emax)` `.pulse_height(region)` `.cutoff(E)` `.no_rayleigh()` `.seed(n)` `.run(histories)` → `Result` |
| `Result` | `.deposited(region)` `.total_deposited()` `.escaped()` `.escaped_uncollided()` `.escaped_energy()` `.plane_count(p)` `.plane_uncollided(p)` `.plane_energy(p)` `.interactions(process)` `.pulse_fraction(region, lo, hi)` (each with `_err` where it has a statistical error), `.save_regions .save_plane_spectrum .save_leakage_spectrum .save_pulse_height` |
| physics helpers | `klein_nishina(E)` cm2/electron, `compton_mean_transfer(E)`, `sample_compton_mean(E, n, seed)` |
| checks and output | `assert_close(a, b, tol)` `assert_true(bool)` `print(s)` `save_text(path, text)` `version()` |

All tallies are per source photon with the standard error of the mean. Physics, models and their limits:
[durable-memory/03-physics-and-limits.md](durable-memory/03-physics-and-limits.md); schema:
[durable-memory/04-schema-v0.md](durable-memory/04-schema-v0.md).

Combine it with other hosts in one script: [examples/combo/](examples/combo) runs a simulation with
lmparticle and draws it with [lmplot](https://github.com/lmctl-ai/lmplot) (two `_dsl_` blocks, two hosts).

![Lead shielding](examples/renders/shielding_plot.png)
![NaI pulse-height spectrum](examples/renders/detector_spectrum.png)

## Examples

| Program | Shows |
| --- | --- |
| [shielding.lms](examples/shielding.lms) | Cs-137 through 0.25-3.25 cm of lead: the uncollided beam follows exp(-mu x), HVL and TVL |
| [depth_dose.lms](examples/depth_dose.lms) | layer-by-layer energy deposition of a Co-60 beam in a 30 cm water phantom |
| [detector_response.lms](examples/detector_response.lms) | pulse-height spectrum of a NaI crystal: photopeak, Compton continuum and edge |
| [pair_spectrum.lms](examples/pair_spectrum.lms) | what leaves lead after 6 MeV photons: pair production and the 511 keV line |
| [combo/shielding_plot.lms](examples/combo/shielding_plot.lms), [combo/detector_spectrum.lms](examples/combo/detector_spectrum.lms) | lmparticle simulates, lmplot draws |

`tests/` asserts known answers: XCOM published coefficients, the Klein-Nishina cross section and mean
energy transfer, narrow-beam exp(-mu x) through layered and oblique geometries, the solid angle of a
detector seen from an isotropic source, region priority, energy conservation to 1e-9, the pair threshold
and annihilation photons, buildup trends, the Compton edge, and bitwise reproducibility.

## Build

C++17 compiler, CMake 3.24; nothing else (no third-party sources).

```sh
cmake -S . -B build && cmake --build build     # about 20 seconds
./test.sh                                      # needs lmscript on PATH
```

Linux builds are fully static; macOS links only system libraries; Windows links the MinGW runtime
statically. `lmparticle --notices` prints the XCOM citation and the licenses.

## Scope

Photons only: electrons and positrons deposit their energy where they are produced (a kerma
approximation; no bremsstrahlung, no fluorescence, no Doppler broadening), and positrons annihilate at
rest. That is accurate for attenuation, shielding, buildup and detector counting questions with photons
above a few tens of keV in low and medium Z; it is not a substitute for Geant4 where charged-particle
ranges matter. See [durable-memory/03-physics-and-limits.md](durable-memory/03-physics-and-limits.md).

## License

MIT ([LICENSE](LICENSE)). The embedded cross sections are NIST XCOM data (public data of the U.S.
Government, cited in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)).
