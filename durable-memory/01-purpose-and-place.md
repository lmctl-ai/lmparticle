# Purpose and place

lmparticle is target 6 of the *scripted scientific libraries* family (`../../simulation/ROADMAP.md`,
procedure `../../simulation/PROCEDURE.md`, feasibility `../../simulation/FEASIBILITY.md` section
"lmparticle"). FEASIBILITY ruled Geant4 out for now (data sets of hundreds of MB, 45-90 min CI per
platform, ~100 MB binary) and asked for a reduced own Monte Carlo with embedded public cross sections.
That is this project: no third-party source at all, one ~2.5 MB static executable, 20 s build.

- **Scripting only**: `--dsl [--dry-run]`, `--version` (exactly `lmparticle X.Y.Z`), `--notices`,
  `--dsl-schema [--json]`, `--dsl-signatures`; exit 2 usage, 3 load/run error. Context `rad: Rad`.
- **Layout**: `cpp/xs.*` (XCOM tables, materials, Klein-Nishina), `cpp/transport.*` (RNG, geometry
  tracking, interactions, tallies), `cpp/main.cpp` (DSL registrations), `tests/core.cpp` (native ctest),
  `tests/*.lms` and `examples/*.lms` (DSL programs that assert), `data/xcom.csv` (embedded tables),
  `scripts/fetch-xcom.py` (regenerates the tables; not part of the build).
- **Not Geant4**: no electron transport, no nuclear/hadronic physics, no neutrons. A later lmparticle
  that wraps Geant4 would need an explicit data-download decision.
