#!/usr/bin/env bash
# run.sh [game args...]  e.g. ./scripts/run.sh --scene showcase --page 3
# builds (incremental) and starts the game with saves/ in the project root
source "$(dirname "$0")/common.sh"
configure
build --target raylib_demo01 >/dev/null || die "build failed"
cd "$ROOT"
exec "$BUILD_DIR/raylib_demo01" --save-dir "$ROOT/saves" "$@"
