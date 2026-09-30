#!/usr/bin/env bash
# doctor.sh - read-only environment check (never installs or changes anything)
source "$(dirname "$0")/common.sh"
set +e

ok=0; bad=0
check() {  # check <label> <command...>
  local label="$1"; shift
  local out; out="$("$@" 2>/dev/null | head -1)"
  if [[ -n "$out" ]]; then printf "  %-14s %s\n" "$label" "$out"; ok=$((ok+1));
  else printf "  %-14s ${c_red}missing${c_off}\n" "$label"; bad=$((bad+1)); fi
}

info "toolchain"
check cmake     cmake --version
check c++       c++ --version
check python3   "$(find_python)" --version
check git       git --version

info "raylib (build order: submodule -> installed package -> FetchContent)"
sub="$ROOT/third_party/raylib"
if [[ -f "$sub/src/raylib.h" ]]; then
  printf "  %-14s %s (%s) %s\n" "submodule" "third_party/raylib" \
    "$(grep -m1 'define RAYLIB_VERSION ' "$sub/src/raylib.h" | cut -d'"' -f2)" \
    "$(git -C "$ROOT" submodule status third_party/raylib 2>/dev/null | awk '{print substr($1,1,9)}')"
else
  warn "submodule not initialized: git submodule update --init --depth 1"
fi
check pkg-config pkg-config --modversion raylib
check brew      bash -c 'brew list --versions raylib 2>/dev/null'
hdr=""
for p in /opt/homebrew/include/raylib.h /usr/local/include/raylib.h /usr/include/raylib.h; do [[ -f $p ]] && hdr=$p && break; done
if [[ -n "$hdr" ]]; then
  printf "  %-14s %s (%s)\n" "header" "$hdr" "$(grep -m1 'define RAYLIB_VERSION ' "$hdr" | cut -d'"' -f2)"
else
  warn "raylib.h not found -> CMake will FetchContent raylib 6.0 from GitHub (needs network)"
fi

info "optional"
check clang-format clang-format --version
[[ -f /System/Library/Fonts/Supplemental/Arial\ Unicode.ttf ]] && echo "  font           Arial Unicode (CJK) available" \
  || warn "no CJK TTF found: UI falls back to raylib's default font (ASCII)"

echo
if (( bad > 0 )); then warn "$bad item(s) missing; install hints: brew install cmake raylib"; else info "all good ($ok checks)"; fi
exit 0
