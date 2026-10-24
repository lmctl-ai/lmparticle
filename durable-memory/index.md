# lmparticle durable memory

`lmparticle` is a static, dependency-free lmscript `_dsl_` host for radiation transport, NIST XCOM photon/electron attenuation, dosimetry, and shielding simulation.
Repository: https://github.com/lmctl-ai/lmparticle.

## 60-second takeover

```sh
cmake -S . -B build                   # Configure CMake build
cmake --build build --parallel 4      # Build binary at build/lmparticle
./test.sh                             # Run verification test suite (NIST benchmarks, radiation oracles)
```

## Document map

| File | Contents |
| --- | --- |
| [01-purpose-and-place.md](01-purpose-and-place.md) | Scope, radiation transport capabilities, and relation to Geant4 |
| [02-data-and-licenses.md](02-data-and-licenses.md) | NIST XCOM cross-section tables, provenance, and public-domain data |
| [03-physics-and-limits.md](03-physics-and-limits.md) | Physical models, photon/charged particle interactions, and limits |
| [04-schema-v0.md](04-schema-v0.md) | `rad` API schema, material definitions, detector tallies |
| [05-validation.md](05-validation.md) | Validation against textbook benchmarks and analytical tolerances |
| [06-build-run-test.md](06-build-run-test.md) | Build commands, test.sh checks, and binary size benchmarks |
| [07-ci.md](07-ci.md) | GitHub Actions CI matrix (5 platforms) and build workflows |
| [08-examples.md](08-examples.md) | Radiation shielding, Bragg peak, and Compton scattering examples |
| [09-open-items.md](09-open-items.md) | Technical backlog, neutron transport, and roadmap |
| [10-resource-limits.md](10-resource-limits.md) | Memory budget allocation (`LMPARTICLE_MAX_MEMORY_MB`) and particle caps |
| [next-release-version.md](next-release-version.md) | Version alignment protocol (VERSION, CMake, THIRD_PARTY_NOTICES) |

## SDK pin and sync

The vendored SDK header is pinned via `sdk.lock` pointing to `https://github.com/lmctl-ai/lmcode-cpp`:
```sh
scripts/sync-sdk.sh                   # Refetches pinned cpp/dslvm.hpp and verifies sha256
```

## Adding an example or function

### Adding an example
1. Create `examples/<name>.lms` using `_dsl_ cli="lmparticle" install="github:lmctl-ai/lmparticle@latest"`.
2. Run script: `build/lmparticle run examples/<name>.lms` or via `lmscript`.
3. Add the example to `./test.sh` test run list.
4. Save visual output to `docs/images/<name>.png` and update `README.md` demo table with `lmctl autorun` command.

### Adding a host function
1. Declare function/method in `schema/lmparticle.dsl.json`.
2. Register and implement the function in C++ source under `src/` using `dslvm::Registry`.
3. Rebuild and verify schema synchronization with `cmake --build build` and `./test.sh`.

## Releases

Releases are triggered automatically on push of a `v*` tag. The release workflow builds across Linux x64/arm64, macOS x64/arm64, and Windows x64, publishing platform binaries with `SHA256SUMS`, `LICENSE`, and `THIRD_PARTY_NOTICES`. See `next-release-version.md` before tagging.
