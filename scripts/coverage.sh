#!/usr/bin/env bash
# coverage.sh [min_in_scope_percent] - regenerate docs/API_COVERAGE.md; non-zero exit when
# the in-scope raylib API coverage drops below the threshold (default 95).
source "$(dirname "$0")/common.sh"
"$(find_python)" "$ROOT/scripts/api_coverage.py" --min-in-scope "${1:-95}"
