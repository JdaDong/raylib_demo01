#!/usr/bin/env bash
# clean.sh [--saves] - remove build directories inside this project only.
#   --saves also removes ./saves (settings, highscores, screenshots, replays)
source "$(dirname "$0")/common.sh"
for d in build build-core build-asan; do
  [[ -d "$ROOT/$d" ]] && { rm -rf "${ROOT:?}/$d"; info "removed $d/"; }
done
if [[ "${1:-}" == "--saves" && -d "$ROOT/saves" ]]; then
  rm -rf "${ROOT:?}/saves"; info "removed saves/"
fi
exit 0
