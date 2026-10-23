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
ctest --test-dir "$BUILD_DIR" --output-on-failure || fail "native physics core tests"
ok "native physics core regression tests"
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
"$HOST" --dsl-schema --json >"$WORK/schema.json" || fail "embedded schema print"
cmp -s "$WORK/schema.json" schema/lmparticle.dsl.json || fail "embedded schema differs from schema/lmparticle.dsl.json"
ok "--dsl-schema --json prints byte-identical embedded schema bytes"
diff -u <("$LMSCRIPT" --dsl-signatures schema/lmparticle.dsl.json) <("$HOST" --dsl-signatures) || fail "host/compiler signatures differ"
ok "host registry matches compiler signatures"
"$HOST" --notices | grep -q "XCOM" || fail "--notices missing third-party text"
ok "--notices prints the third-party license texts"

case "$UNAME_S" in
  Linux) if ldd "$HOST" >/dev/null 2>&1; then fail "executable is dynamically linked"; fi; ok "executable is fully static (no dynamic loader)";;
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
ok "$n programs compile as bytecode v5"
for src in tests/*.lms examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --dsl "build/$name.dslb" >"$WORK/$name.out" 2>"$WORK/$name.err" || { cat "$WORK/$name.out" "$WORK/$name.err" >&2; fail "$name run"; }
  grep -q '"ok": true' "$WORK/$name.out" || { cat "$WORK/$name.out" >&2; fail "$name did not report ok"; }
done
ok "$n programs run and pass their own assertions"

# Determinism: the CSV files the examples write are identical on rerun.
for f in build/*.csv build/examples/*.csv; do echo "$(sha "$f") $f"; done >"$WORK/before.txt"
for src in tests/*.lms examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --dsl "build/$name.dslb" >/dev/null 2>&1 || fail "$name rerun"
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
  set +e; out="$("$HOST" --dsl "$WORK/$1.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] || fail "$1: exit code $code, expected 3 ($out)"
  echo "$out" | grep -q "$3" || fail "$1: message lacks '$3': $out"
}
expect_error unknown_material 'let m = rad.builtin("unobtainium");' "unknown builtin material"
expect_error unknown_element 'let m = rad.material("x", 1.0).element("Xx", 1.0);' "unknown element"
expect_error bad_density 'let m = rad.material("x", 0.0);' "density"
expect_error empty_material 'let m = rad.material("x", 1.0); let v = m.mu(1.0);' "at least one element"
expect_error energy_range 'let m = rad.builtin("water"); let v = m.mu(50.0);' "energy must be between"
expect_error bad_process 'let m = rad.builtin("water"); let v = m.mu_partial(1.0, "fusion");' "process must be"
expect_error empty_geometry 'let s = rad.source(); s.energy(1.0); let r = rad.simulation(rad.geometry(), s).run(10);' "no boxes"
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
ok "errors stop with exit 3 and clear messages (24 cases)"

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
"$HOST" --dry-run --dsl build/shielding.dslb >"$WORK/dry.out" || fail "dry run"
[ ! -e build/examples/shielding.csv ] || fail "dry run created CSV"
"$HOST" --dsl build/shielding.dslb >/dev/null || fail "restore example CSV"
ok "dry run verifies without writing outputs"

for flags in '--unknown' '--version extra' '--dsl-schema --bogus' '--dry-run' '--dsl --bogus'; do
  set +e
  # Deliberate word splitting: each fixed test string is a CLI argument sequence.
  # shellcheck disable=SC2086
  "$HOST" $flags >/dev/null 2>&1
  code=$?
  set -e
  [ "$code" = 2 ] || fail "usage flags '$flags': exit $code"
done
ok "invalid CLI combinations use exit 2"

set +e; "$HOST" --dsl "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

echo "all $pass checks passed"
