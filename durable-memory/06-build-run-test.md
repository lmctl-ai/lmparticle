# Build, run, test

Measured 2026-10-23, x86_64 Linux (WSL2), 4 cores, GCC, Ninja.

- Clean Release build about 20 s (no third-party sources). Binary 2.5 MB stripped, `-static`, `ldd`: not dynamic.
- `./test.sh` (18 checks, about 25 s): sdk pin; no-dependencies check; build; native ctest (`tests/core.cpp`:
  tables, RNG, Klein-Nishina sampler, one conserving run); `--version` exactly `lmparticle X.Y.Z`; schema equals
  the registrations; embedded schema byte-identical; host registry equals compiler signatures; notices; static
  linking; 13 programs (9 tests, 4 examples) compile and run and pass their own asserts; 11 CSV result files
  byte-identical on rerun; 18 error snippets exit 3 with a message naming the problem; the two lmparticle+lmplot
  pipelines when `lmplot` is on `PATH` or `LMPARTICLE_TEST_NETWORK=1`; dry run writes nothing; usage exit codes.
- Speed: roughly 1-3 million photon histories per second in simple geometries.
- Run a program by hand: `lmscript examples/shielding.lms` (compiles, binds to `lmparticle` on PATH).
