#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build}"
HOST="$BUILD_DIR/lmparticle"
UNAME_S="$(uname -s)"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lmparticle-test-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
# Native Windows tools (node, the host) cannot address git-bash /tmp or /d/a paths.
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) WORK="$(cd "$WORK" && pwd -W | tr '\134' '/')";; esac
pass=0
ok(){ pass=$((pass+1)); echo "ok $pass - $1"; }
fail(){ echo "FAIL: $*" >&2; exit 1; }
sha(){ if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -c1-64; else shasum -a 256 "$1" | cut -c1-64; fi; }

# lmscript resolves cli="lmparticle" on PATH first, so programs bind to the binary under test.
PATH="$(cd "$BUILD_DIR" 2>/dev/null && pwd || echo "$HERE/$BUILD_DIR"):$PATH"
export PATH
LMSCRIPT="$(scripts/select-compiler.sh)"
scripts/sync-sdk.sh --verify >/dev/null || fail "SDK header pin mismatch"
ok "SDK header matches sdk.lock"
scripts/sync-deps.sh --verify >/dev/null || fail "third_party pin mismatch"
ok "no third-party sources (data/xcom.csv present)"

GEN=""
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then GEN="-G Ninja"; fi
# shellcheck disable=SC2086
cmake -S . -B "$BUILD_DIR" $GEN -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" --parallel "${BUILD_JOBS:-4}" >"$WORK/build.log" 2>&1 || { tail -40 "$WORK/build.log" >&2; fail "build"; }
ok "build"
python3 tests/check-host-flags.py schema/lmparticle.dsl.json "$HOST" || fail "hard host flag cutover"
ctest --test-dir "$BUILD_DIR" --output-on-failure || fail "native physics core tests"
ok "native physics core regression tests"
python3 scripts/check-version.py "$HOST" || fail "package version agreement"
ok "VERSION agrees with CMake, schema, notices, binary and release tag"
want_version="lmparticle $(sed -n 's/^project(lmparticle VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ "$("$HOST" --version)" = "$want_version" ] || fail "--version is not exactly '$want_version'"
ok "--version prints exactly '$want_version'"
python3 scripts/gen-schema.py >/dev/null && git diff --quiet -- schema/lmparticle.dsl.json 2>/dev/null || ! git rev-parse --git-dir >/dev/null 2>&1 || fail "schema/lmparticle.dsl.json is stale: run scripts/gen-schema.py"
ok "schema matches the registrations in cpp/main.cpp"
python3 scripts/check-visuals.py || fail "visual assets"
if { command -v lmplot >/dev/null 2>&1 && command -v lmcad >/dev/null 2>&1; } || [ "${LMPARTICLE_TEST_NETWORK:-}" = 1 ]; then
  examples/render-all.sh || fail "visual examples"
else
  ok "visual examples skipped (lmplot/lmcad unavailable, LMPARTICLE_TEST_NETWORK unset)"
fi
"$HOST" --lmcode-schema --json >"$WORK/schema.json" || fail "embedded schema print"
cmp -s "$WORK/schema.json" schema/lmparticle.dsl.json || fail "embedded schema differs from schema/lmparticle.dsl.json"
ok "--lmcode-schema --json prints byte-identical embedded schema bytes"
diff -u <("$LMSCRIPT" --lmcode-signatures schema/lmparticle.dsl.json) <("$HOST" --lmcode-signatures) || fail "host/compiler signatures differ"
ok "host registry matches compiler signatures"
"$HOST" --notices | grep -q "XCOM" || fail "--notices missing third-party text"
ok "--notices prints the third-party license texts"

case "$UNAME_S" in
  Linux)
    if command -v readelf >/dev/null 2>&1; then
      needed="$(readelf -d "$HOST" | sed -n 's/.*(NEEDED).*Shared library: \[\(.*\)\]/\1/p')"
    else
      needed="$(ldd "$HOST" | awk '/=>/ {print $1} !/=>/ && /ld-linux/ {sub(/^[[:space:]]*/, ""); sub(/[[:space:]].*/, ""); print $0}' | xargs -n1 basename)"
    fi
    [ -n "$needed" ] || fail "expected dynamic glibc linkage on Linux, found no NEEDED entries"
    bad="$(printf '%s\n' "$needed" | grep -v -E '^(libc\.so\.6|libm\.so\.6|libpthread\.so\.0|libdl\.so\.2|librt\.so\.1|ld-linux.*)' || true)"
    [ -z "$bad" ] || fail "disallowed Linux dynamic libraries: $bad"
    ok "Linux binary links only dynamic glibc (${needed//$'\n'/ })";;
  Darwin) bad="$(otool -L "$HOST" | tail -n +2 | grep -v -E '/usr/lib/(libSystem|libc\+\+|libresolv)' || true)"
          [ -z "$bad" ] || fail "non-system dylib dependencies: $bad"; ok "only system libraries linked";;
  MINGW*|MSYS*|CYGWIN*)
    if command -v objdump >/dev/null 2>&1; then
      bad="$(objdump -p "$HOST" | grep 'DLL Name' | grep -i -E 'libstdc|libgcc|winpthread|libgomp' || true)"
      [ -z "$bad" ] || fail "MinGW runtime DLLs are imported: $bad"
      ok "no MinGW runtime DLL imports"
    else ok "static-link check skipped (no objdump)"; fi;;
  *) ok "static-link check skipped on $UNAME_S";;
esac

# Programs: compile every test and example, run them (they assert their own numbers).
mkdir -p build/examples
n=0
for src in tests/*.lms examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$LMSCRIPT" "$src" "build/$name.dslb" >"$WORK/$name.log" 2>&1 || { cat "$WORK/$name.log" >&2; fail "$name compile"; }
  n=$((n+1))
done
ok "$n programs compile as bytecode v6"
for src in tests/*.lms examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --lmcode "build/$name.dslb" >"$WORK/$name.out" 2>"$WORK/$name.err" || { cat "$WORK/$name.out" "$WORK/$name.err" >&2; fail "$name run"; }
  grep -q '"ok": true' "$WORK/$name.out" || { cat "$WORK/$name.out" >&2; fail "$name did not report ok"; }
done
ok "$n programs run and pass their own assertions"

# Determinism: the CSV files the examples write are identical on rerun.
for f in build/*.csv build/examples/*.csv; do echo "$(sha "$f") $f"; done >"$WORK/before.txt"
for src in tests/*.lms examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --lmcode "build/$name.dslb" >/dev/null 2>&1 || fail "$name rerun"
done
for f in build/*.csv build/examples/*.csv; do echo "$(sha "$f") $f"; done >"$WORK/after.txt"
cmp -s "$WORK/before.txt" "$WORK/after.txt" || fail "result files differ on rerun"
ok "all $(wc -l <"$WORK/after.txt" | tr -d ' ') result files are byte-identical on rerun"

# Error paths: each snippet must fail the run with exit 3 and a message naming the problem.
expect_error() { # name, snippet, expected message fragment
  cat >"$WORK/$1.lms" <<LMS
p = _dsl_ cli="lmparticle";
$2
end _dsl_
savedsl(p, "$WORK/$1.dslb");
LMS
  "$LMSCRIPT" "$WORK/$1.lms" >/dev/null 2>"$WORK/$1.cerr" || { cat "$WORK/$1.cerr" >&2; fail "$1 compile"; }
  set +e; out="$("$HOST" --lmcode "$WORK/$1.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] || fail "$1: exit code $code, expected 3 ($out)"
  echo "$out" | grep -q "$3" || fail "$1: message lacks '$3': $out"
}
expect_error unknown_material 'let m = rad.builtin("unobtainium");' "unknown builtin material"
expect_error unknown_element 'let m = rad.material("x", 1.0).element("Xx", 1.0);' "unknown element"
expect_error bad_density 'let m = rad.material("x", 0.0);' "density"
expect_error empty_material 'let m = rad.material("x", 1.0); let v = m.mu(1.0);' "at least one element"
expect_error energy_range 'let m = rad.builtin("water"); let v = m.mu(50.0);' "energy must be between"
expect_error bad_process 'let m = rad.builtin("water"); let v = m.mu_partial(1.0, "fusion");' "process must be"
expect_error empty_geometry 'let s = rad.source(); s.energy(1.0); let r = rad.simulation(rad.geometry(), s).run(10);' "no regions"
expect_error no_energy 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let r = rad.simulation(g, rad.source()).run(10);' "no energy"
expect_error flat_box 'let g = rad.geometry(); g.box("a", rad.builtin("water"), 0.0, 0.0, 0.0, 1.0, 0.0, 1.0);' "low < high"
expect_error duplicate_region 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0).slab("a", rad.builtin("lead"), 1.0, 2.0);' "duplicate region"
expect_error bad_axis 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let q = rad.simulation(g, s).plane("p", "w", 0.0);' "axis"
expect_error zero_beam 'let s = rad.source(); s.beam(0.0, 0.0, 0.0);' "nonzero"
expect_error bad_histories 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let r = rad.simulation(g, s).run(0);' "histories"
expect_error unknown_region 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let r = rad.simulation(g, s).run(10); let d = r.deposited("b");' "unknown region"
expect_error unknown_plane 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let r = rad.simulation(g, s).run(10); let d = r.plane_count("p");' "unknown plane"
expect_error source_energy 'let s = rad.source(); s.energy(0.0001);' "energy must be between"
expect_error unwritable 'rad.save_text("/nonexistent-dir/x.csv", "x");' "cannot write"
expect_error assertfail 'rad.assert_close(1.0, 2.0, 0.1);' "assert_close failed"
expect_error assert_true 'rad.assert_true(false);' "assert_true failed"
# nonfinite numbers (pow overflow gives inf, inf - inf NaN) are refused where they would poison a run
expect_error nan_point 'let i = pow(10.0, 400.0); let s = rad.source(); s.point(0.0, 0.0, i - i);' "coordinates must be finite"
expect_error inf_density 'let m = rad.material("x", pow(10.0, 400.0));' "density must be positive and finite"
expect_error inf_fraction 'let m = rad.material("x", 1.0).element("H", pow(10.0, 400.0));' "mass fraction must be positive and finite"
expect_error inf_weight 'let s = rad.source(); s.line(1.0, pow(10.0, 400.0));' "weight must be positive and finite"
expect_error nan_plane 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let i = pow(10.0, 400.0); let q = rad.simulation(g, s).plane("p", "z", i - i);' "position must be finite"
expect_error builtin_change 'let m = rad.builtin("water").element("Pb", 1.0);' "builtin material cannot be changed"
expect_error sphere_radius 'let g = rad.geometry(); g.sphere("a", rad.builtin("water"), 0.0, 0.0, 0.0, 0.0);' "radius must be positive"
expect_error cylinder_z 'let g = rad.geometry(); g.cylinder("a", rad.builtin("water"), 0.0, 0.0, 1.0, 2.0, 1.0);' "z0 < z1"
expect_error cone_angle 'let s = rad.source(); s.cone(0.0, 0.0, 1.0, 200.0);' "half-angle"
expect_error cone_axis 'let s = rad.source(); s.cone(0.0, 0.0, 0.0, 10.0);' "nonzero"
expect_error disc_radius 'let s = rad.source(); s.disc(-1.0);' "radius"
expect_error weight_cutoff 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let q = rad.simulation(g, s).implicit_capture(1.0);' "weight cutoff"
expect_error pulse_implicit 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let r = rad.simulation(g, s).pulse_height("a").implicit_capture(0.1).run(10);' "analog transport"
expect_error tracks_range 'let g = rad.geometry(); g.slab("a", rad.builtin("water"), 0.0, 1.0); let s = rad.source(); s.energy(1.0); let q = rad.simulation(g, s).tracks(-1);' "0..100000"
expect_error table_rows 'rad.element_table("Xq", 50, "0.001 1 1 1 0 0\n");' "at least two rows"
expect_error table_order 'rad.element_table("Xq", 50, "0.001 1 1 1 0 0\n20 1 1 1 0 0\n5 1 1 1 0 0\n");' "ascending"
expect_error table_range 'rad.element_table("Xq", 50, "0.01 1 1 1 0 0\n20 1 1 1 0 0\n");' "0.001 to 20"
expect_error table_columns 'rad.element_table("Xq", 50, "0.001 1 1 1 0\n20 1 1 1 0 0\n");' "6 numbers"
ok "errors stop with exit 3 and clear messages (37 cases)"

# Memory budget: a runaway loop of runs with large spectrum tallies stops with a run error (exit 3,
# message on stderr and in the JSON line) instead of exhausting the machine. A small budget everywhere;
# on Linux also the default 4096 MiB under a 6 GiB address-space limit (ulimit only here, never in the host).
cat >"$WORK/runaway.lms" <<LMS
p = _dsl_ cli="lmparticle";
let g = rad.geometry();
g.slab("a", rad.builtin("water"), 0.0, 1.0);
let s = rad.source();
s.energy(1.0);
let sim = rad.simulation(g, s).spectrum(100000, 1.0);
let k = 0;
while (k < 100) {
  sim.plane("p" + str(k), "z", 0.5);
  k = k + 1;
}
let i = 0;
while (i < 1000000) {
  let r = sim.run(1);
  i = i + 1;
}
end _dsl_
savedsl(p, "$WORK/runaway.dslb");
LMS
"$LMSCRIPT" "$WORK/runaway.lms" >/dev/null || fail "runaway.lms compile"
set +e; out="$(LMPARTICLE_MAX_MEMORY_MB=64 "$HOST" --lmcode "$WORK/runaway.dslb" 2>"$WORK/runaway.err")"; code=$?; set -e
[ "$code" = 3 ] || fail "runaway loop under LMPARTICLE_MAX_MEMORY_MB=64: exit $code, expected 3 ($out)"
echo "$out" | grep -q "memory budget exceeded" || fail "runaway loop: JSON lacks the budget message: $out"
grep -q "LMPARTICLE_MAX_MEMORY_MB" "$WORK/runaway.err" || fail "runaway loop: no budget message on stderr"
set +e; LMPARTICLE_MAX_MEMORY_MB=-5 "$HOST" --lmcode "$WORK/runaway.dslb" >/dev/null 2>"$WORK/badenv.err"; code=$?; set -e
[ "$code" = 2 ] && grep -q "positive whole number" "$WORK/badenv.err" || fail "bad LMPARTICLE_MAX_MEMORY_MB: exit $code"
if [ "$UNAME_S" = Linux ]; then
  set +e; out="$(ulimit -v 6291456; "$HOST" --lmcode "$WORK/runaway.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] && echo "$out" | grep -q "limit 4096 MiB" || fail "runaway loop with the default budget under ulimit -v 6 GiB: exit $code ($out)"
  ok "memory budget: runaway loop stops at 64 MiB and at the default 4096 MiB (under ulimit -v 6 GiB)"
else
  ok "memory budget: runaway loop stops at LMPARTICLE_MAX_MEMORY_MB=64 (default-budget run is Linux-only)"
fi

# Combined pipelines (lmparticle simulates, lmplot draws). Need lmplot on PATH, or network for install=.
if command -v lmplot >/dev/null 2>&1 || [ "${LMPARTICLE_TEST_NETWORK:-}" = 1 ]; then
  for name in shielding_plot detector_spectrum; do
    rm -f "build/examples/$name.png"
    "$LMSCRIPT" "examples/combo/$name.lms" >"$WORK/combo-$name.log" 2>&1 || { cat "$WORK/combo-$name.log" >&2; fail "combo example $name"; }
    [ -s "build/examples/$name.png" ] || fail "combo example $name wrote no PNG"
  done
  ok "lmparticle + lmplot pipelines draw the shielding curve and the detector spectrum"
else
  ok "combo examples skipped (no lmplot, LMPARTICLE_TEST_NETWORK unset)"
fi

# Dry run loads/verifies bytecode without executing the simulation or writing CSV.
rm -f build/examples/shielding.csv
"$HOST" --dry-run --lmcode build/shielding.dslb >"$WORK/dry.out" || fail "dry run"
[ ! -e build/examples/shielding.csv ] || fail "dry run created CSV"
"$HOST" --lmcode build/shielding.dslb >/dev/null || fail "restore example CSV"
ok "dry run verifies without writing outputs"

for flags in '--unknown' '--version extra' '--lmcode-schema --bogus' '--dry-run' '--lmcode --bogus'; do
  set +e
  # Deliberate word splitting: each fixed test string is a CLI argument sequence.
  # shellcheck disable=SC2086
  "$HOST" $flags >/dev/null 2>&1
  code=$?
  set -e
  [ "$code" = 2 ] || fail "usage flags '$flags': exit $code"
done
ok "invalid CLI combinations use exit 2"

set +e; "$HOST" --lmcode "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

# The published compiler injects std; the host embeds only its own schema bytes.
"$LMSCRIPT" --lmcode-signatures schema/lmparticle.dsl.json --without-std > "$WORK/std-host-only.sig"
"$LMSCRIPT" --lmcode-signatures cpp/std/lmcode-std.schema.json > "$WORK/std-builtins.sig"
"$HOST" --lmcode-signatures > "$WORK/std-registry.sig"
python3 - "$WORK" <<'PYSTD'
import pathlib,sys,json
w=pathlib.Path(sys.argv[1])
read=lambda name:set((w/name).read_text().splitlines())
assert read('std-registry.sig')-read('std-builtins.sig') == read('std-host-only.sig'), 'host-only registry drift'
schema=str(pathlib.Path('schema/lmparticle.dsl.json').resolve()).replace('\\','/')
out=str(w/'std-probe.dslb').replace('\\','/')
(w/'std-probe.lms').write_text('p = _lmcode_ schema='+json.dumps(schema)+';\nlet values = str_list().add("abc");\nif (str_len(values.at(0)) != 3) { let bad = int_list().at(0); }\nend _lmcode_\nsavedsl(p, '+json.dumps(out)+');\n')
PYSTD
"$LMSCRIPT" "$WORK/std-probe.lms" > "$WORK/std-probe.log" 2>&1 || { cat "$WORK/std-probe.log"; fail "implicit std compile"; }
"$HOST" --lmcode "$WORK/std-probe.dslb" > "$WORK/std-run.log" 2>&1 || { cat "$WORK/std-run.log"; fail "implicit std runtime"; }
ok "host-only signatures and implicit std runtime"

echo "all $pass checks passed"
