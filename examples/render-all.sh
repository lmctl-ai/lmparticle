#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"
export PATH="$HERE/build:$PATH"
if command -v gh >/dev/null 2>&1 && [ -z "${GITHUB_TOKEN:-}" ]; then
  GITHUB_TOKEN="$(gh auth token 2>/dev/null || true)"
  [ -n "$GITHUB_TOKEN" ] && export GITHUB_TOKEN
fi
LMSCRIPT="$(scripts/select-compiler.sh)"
if { ! command -v lmplot >/dev/null 2>&1 || ! command -v lmcad >/dev/null 2>&1; } && [ "${LMPARTICLE_TEST_NETWORK:-}" != 1 ]; then
  echo "render-all: skipped (lmplot and lmcad unavailable; set LMPARTICLE_TEST_NETWORK=1 to allow install)"
  exit 0
fi
mkdir -p docs/images
if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "render-all: animation skipped (ffmpeg unavailable)"
fi
status=0
for src in examples/*.lms examples/visual/*.lms examples/combo/*.lms; do
  [ -f "$src" ] || continue
  echo "render-all: $src"
  if ! "$LMSCRIPT" "$src"; then
    echo "render-all: failed: $src" >&2
    status=1
  fi
done
exit "$status"
