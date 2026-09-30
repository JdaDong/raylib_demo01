#!/usr/bin/env bash
# test.sh [--suite NAME] [--filter SUBSTR] [--asan] [--smoke] [-v]
#   default : build + all unit suites through CTest (one CTest entry per suite)
#   --suite : run a single suite directly (fast inner loop)
#   --asan  : AddressSanitizer + UBSan build in build-asan/
#   --smoke : additionally run the GUI autopilot smoke test (needs a display)
source "$(dirname "$0")/common.sh"

suite=""; filter=""; smoke=0; verbose=""; asan=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --suite) suite="$2"; shift ;;
    --filter) filter="$2"; shift ;;
    --smoke) smoke=1 ;;
    --asan) asan=1 ;;
    -v) verbose="-v" ;;
    -h|--help) sed -n '2,7p' "$0"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

if (( asan )); then
  BUILD_DIR="$ROOT/build-asan"; BUILD_TYPE=Debug
  # Probe the ASan runtime first: on macOS 27 both Apple clang 15 and Homebrew LLVM 21 ASan
  # abort/hang before main() ("asan_init_is_running"). Degrade to UBSan instead of failing CI.
  probe_dir="$(mktemp -d)"
  echo 'int main(){int*p=new int[2];p[1]=0;int r=p[1];delete[]p;return r;}' > "$probe_dir/p.cpp"
  sanitizers="address,undefined"
  if ! c++ -fsanitize=address "$probe_dir/p.cpp" -o "$probe_dir/p" 2>/dev/null || \
     ! ( perl -e 'alarm 10; exec @ARGV' "$probe_dir/p" ) >/dev/null 2>&1; then
    warn "AddressSanitizer runtime is broken on this toolchain/OS -> running UBSan only"
    sanitizers="undefined"
  fi
  rm -rf "$probe_dir"
  configure -DRD_SANITIZE=ON -DRD_SANITIZERS="$sanitizers" -DRD_BUILD_GAME=OFF
  info "sanitizers: $sanitizers"
else
  configure
fi
build --target unit_tests >/dev/null || die "build failed"

if [[ -n "$suite" || -n "$filter" ]]; then
  args=(); [[ -n "$suite" ]] && args+=(--suite "$suite"); [[ -n "$filter" ]] && args+=(--filter "$filter")
  "$BUILD_DIR/unit_tests" "${args[@]}" $verbose
  exit $?
fi

info "unit tests (ctest -L unit)"
(cd "$BUILD_DIR" && ctest -L unit --output-on-failure -j "$JOBS") || die "unit tests failed"
"$BUILD_DIR/unit_tests" | tail -1

if (( smoke )); then
  "$ROOT/scripts/smoke.sh"
fi
