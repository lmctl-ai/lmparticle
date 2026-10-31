# Build, run, test

Measured 2026-10-23, x86_64 Linux (WSL2), 4 cores, GCC, Ninja.

- Clean Release build about 20 s (no third-party sources). Stripped Linux binary links `libstdc++`/`libgcc` statically and dynamic glibc (`>= 2.35`, verified via `readelf -d` allowlist).
- `./test.sh` (20 checks, about 70 s plus the default-budget runaway, see 10-resource-limits.md): sdk pin; no-dependencies check; build; native ctest (`tests/core.cpp`:
  tables, RNG, Klein-Nishina sampler, conserving runs, grazing exits, rotation, curved regions, sources, implicit capture); `--version` exactly `lmparticle X.Y.Z`; schema equals
  the registrations; embedded schema byte-identical; host registry equals compiler signatures; notices; runtime
  linkage allowlist; 15 programs (11 tests, 4 examples) compile and run and pass their own asserts; 12 CSV result files
  byte-identical on rerun; 37 error snippets exit 3 with a message naming the problem; the memory-budget runaway exits 3; the two lmparticle+lmplot
  pipelines when `lmplot` is on `PATH` or `LMPARTICLE_TEST_NETWORK=1`; dry run writes nothing; usage exit codes.
- Speed: roughly 1-3 million photon histories per second in simple geometries.
- Run a program by hand: `lmscript examples/shielding.lms` (compiles, binds to `lmparticle` on PATH).

## Native host flag contract

The host accepts only the lmcode option family, with no retired aliases. The full gate checks exact schema/signature output, new loader routing and rejection before file effects in scratch HOME. SDK20325f7 is synchronized through the owner script for actual source-compiler v6 capabilities; package metadata alone is insufficient. Internal DSLB and .dslb names remain.
