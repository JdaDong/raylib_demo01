#!/usr/bin/env bash
# format.sh [--check] - clang-format all sources (uses .clang-format); --check = dry run for CI
source "$(dirname "$0")/common.sh"
command -v clang-format >/dev/null || die "clang-format not installed (brew install clang-format)"
cd "$ROOT"
files=()
while IFS= read -r f; do files+=("$f"); done < <(find src tests \( -name '*.cpp' -o -name '*.hpp' \) | sort)
if [[ "${1:-}" == "--check" ]]; then
  clang-format --dry-run --Werror "${files[@]}" && info "format OK (${#files[@]} files)"
else
  clang-format -i "${files[@]}" && info "formatted ${#files[@]} files"
fi
