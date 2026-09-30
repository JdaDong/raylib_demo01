#!/usr/bin/env bash
# build.sh [debug|release|relwithdebinfo] [--core-only] [--werror] [--asan]
#   --core-only : headless build (rd_core + unit_tests), no window/GL needed (CI)
source "$(dirname "$0")/common.sh"

extra=()
for a in "$@"; do
  case "$a" in
    debug) BUILD_TYPE=Debug ;;
    release) BUILD_TYPE=Release ;;
    relwithdebinfo) BUILD_TYPE=RelWithDebInfo ;;
    --core-only) extra+=(-DRD_BUILD_GAME=OFF); BUILD_DIR="$ROOT/build-core" ;;
    --werror) extra+=(-DRD_WERROR=ON) ;;
    --asan) extra+=(-DRD_SANITIZE=ON); BUILD_TYPE=Debug; BUILD_DIR="$ROOT/build-asan" ;;
    -h|--help) sed -n '2,4p' "$0"; exit 0 ;;
    *) die "unknown argument: $a" ;;
  esac
done

info "configure ($BUILD_TYPE) -> ${BUILD_DIR#$ROOT/}"
configure ${extra[@]+"${extra[@]}"}  # bash 3.2 + set -u safe empty-array expansion
info "build (-j$JOBS)"
start=$(date +%s)
log="$BUILD_DIR/build.log"
# NOTE: don't use `cmd | grep || true` + PIPESTATUS: with pipefail the `|| true` masks real failures
if ! build >"$log" 2>&1; then
  grep -E "error|Error" "$log" | head -30 >&2
  die "build failed (full log: ${log#$ROOT/})"
fi
grep -E "error|warning: [^s]|Built target" "$log" || true
info "done in $(( $(date +%s) - start ))s"
for bin in raylib_demo01 unit_tests; do
  [[ -x "$BUILD_DIR/$bin" ]] && echo "  ${BUILD_DIR#$ROOT/}/$bin"
done
exit 0
