#!/usr/bin/env bash
# ci.sh - everything a CI job needs, in order; stops at the first failure.
#   1 doctor  2 headless core build with -Werror  3 unit tests  4 ASan/UBSan unit tests
#   5 full build  6 API coverage gate  7 GUI smoke (skipped with --no-gui or without a display)
source "$(dirname "$0")/common.sh"

gui=1
[[ "${1:-}" == "--no-gui" ]] && gui=0
[[ "$(uname)" == "Linux" && -z "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && gui=0

step() { echo; info "[$1] $2"; }
step 1 doctor;               "$ROOT/scripts/doctor.sh" | tail -3
step 2 "core build -Werror"; "$ROOT/scripts/build.sh" --core-only --werror >/dev/null
step 3 "unit tests";         (cd "$ROOT/build-core" && ctest -L unit --output-on-failure -j "$JOBS" | tail -3)
step 4 "asan + ubsan";       "$ROOT/scripts/test.sh" --asan >/dev/null && echo "  sanitizers clean"
step 5 "full build";         "$ROOT/scripts/build.sh" --werror >/dev/null
step 6 "api coverage gate";  "$ROOT/scripts/coverage.sh" 95
if (( gui )); then
  step 7 "gui smoke";        "$ROOT/scripts/smoke.sh"
else
  step 7 "gui smoke";        warn "skipped (no display)"
fi
echo; info "CI passed"
