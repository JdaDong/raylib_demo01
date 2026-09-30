#!/usr/bin/env bash
# common.sh - shared helpers for all scripts (sourced, not executed)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
JOBS="${JOBS:-$( (command -v sysctl >/dev/null && sysctl -n hw.ncpu) || nproc 2>/dev/null || echo 4)}"

c_green=$'\033[32m'; c_red=$'\033[31m'; c_yellow=$'\033[33m'; c_dim=$'\033[2m'; c_off=$'\033[0m'
info() { echo "${c_green}==>${c_off} $*"; }
warn() { echo "${c_yellow}warn:${c_off} $*" >&2; }
die()  { echo "${c_red}error:${c_off} $*" >&2; exit 1; }

# prefer the WorkBuddy managed python, then any python3
find_python() {
  local cands=("$HOME/.workbuddy/binaries/python/versions/3.13.12/bin/python3" "$(command -v python3 || true)")
  for p in "${cands[@]}"; do [[ -n "$p" && -x "$p" ]] && { echo "$p"; return; }; done
  die "python3 not found"
}

# NOTE: macOS ships bash 3.2, where "${arr[@]}" of an empty array is "unbound" under `set -u`
# -> pass "$@" straight through instead of copying into a local array.
configure() {
  cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "$@" >/dev/null
}

build() {
  cmake --build "$BUILD_DIR" -j "$JOBS" "$@"
}
