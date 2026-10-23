# Resource limits

Why: host objects live in arenas until exit (no `free`), so a DSL loop that keeps running
simulations keeps every Result, and one run with many planes and spectrum bins allocates large
tallies. On 2026-10-23 a runaway lm* host (lmnum) reached 16 GB RSS and filled the 24 GB WSL
machine; every lm* host now has a total budget.

- **Total memory budget**, default 4096 MiB, set with `LMPARTICLE_MAX_MEMORY_MB` (positive whole
  MiB; anything else exits 2 with the usage text). Shown in the usage message.
- `cpp/memory_budget.{hpp,cpp}` (identical copy in every lm* host) replaces the global
  `operator new`/`delete` and counts block sizes from the C allocator. lmparticle has no C
  library: everything is C++ heap and counted, including the Material/Geometry/Source/Sim/Result
  arenas (1e6 objects each), mixture grids rebuilt by `Material.element`, boxes, planes, source
  lines, the photon stack and the XCOM tables parsed once from the embedded CSV.
- Sizes: the tallies of one run are (boxes + 3 planes + (planes + pulse regions + 1) x bins) cells
  of ~33 bytes (bins <= 1e5, planes unlimited); the Result keeps one 16-byte Stat per cell.
  `Sim.run` `require()`s both before transport, so an oversized run fails before any history.
  Histories (<= 1e9) cost time, not memory: per-history state is the touched-index list and the
  secondary-photon stack.
- Exceeding it throws `membudget::Exceeded` (a `std::bad_alloc`); `translate` turns it into the usual
  run error, the first failure prints `lmparticle: memory budget exceeded: ... (set
  LMPARTICLE_MAX_MEMORY_MB ...)` on stderr, exit 3. `main` also catches `std::exception` around load/run.
- No `setrlimit`: not portable to macOS static builds and would crash instead of reporting.
- Measured (x86_64 Linux, WSL, loaded machine): a loop of `run(1)` on a Sim with 100 planes and
  100000 bins (~155 MiB kept per Result, 472 MiB required per run) stops at 64 MiB at once
  (pre-check) and at the default 4096 MiB after 3.4 s CPU (9.6-22 s wall under load) with 3.10 GB
  peak RSS (3636 MiB tracked; vector capacity counts before pages are touched). `test.sh` runs both;
  the default-budget run only on Linux, under `ulimit -v 6291456`.
