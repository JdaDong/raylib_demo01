#!/usr/bin/env bash
# smoke.sh [frames] - GUI integration test: autopilot plays Title -> Gameplay -> GameOver ->
# Showcase (all 8 pages) -> Settings -> Quit, saves screenshots, fails on any LOG_ERROR.
# Output: build/smoke/ (saves + screenshots + smoke.log)
source "$(dirname "$0")/common.sh"

frames="${1:-6000}"
out="$BUILD_DIR/smoke"
configure
build --target raylib_demo01 >/dev/null || die "build failed"

rm -rf "$out"; mkdir -p "$out"
info "running autopilot (max $frames frames, 120s timeout)"
set +e
# `timeout` is GNU coreutils; fall back to perl alarm on stock macOS
if command -v timeout >/dev/null; then
  timeout 120 "$BUILD_DIR/raylib_demo01" --smoke "$frames" --save-dir "$out/saves" >"$out/smoke.log" 2>&1
else
  perl -e 'alarm 120; exec @ARGV' "$BUILD_DIR/raylib_demo01" --smoke "$frames" --save-dir "$out/saves" >"$out/smoke.log" 2>&1
fi
code=$?
set -e

grep -E "SMOKE:|ERROR" "$out/smoke.log" | sed 's/^/  /'
shots=$(ls "$out/saves/screenshots" 2>/dev/null | wc -l | tr -d ' ')
case $code in
  0)   info "smoke OK - $shots screenshots in ${out#$ROOT/}/saves/screenshots" ;;
  124|142) die "smoke TIMEOUT (hung?) - see ${out#$ROOT/}/smoke.log" ;;
  *)   die "smoke FAILED (exit $code) - see ${out#$ROOT/}/smoke.log" ;;
esac
