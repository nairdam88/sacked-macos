#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

SKIP_SMOKE="${SACKED_SKIP_SMOKE:-0}"
SKIP_SIGN="${SACKED_SKIP_SIGN:-0}"
CNC_DEVMODE="${SACKED_CNC_DEVMODE:-0}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-smoke)
            SKIP_SMOKE=1
            shift
            ;;
        --no-sign)
            SKIP_SIGN=1
            shift
            ;;
        --cnc-debug)
            CNC_DEVMODE=1
            shift
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
done

export SACKED_SKIP_SMOKE="$SKIP_SMOKE"
export SACKED_SKIP_SIGN="$SKIP_SIGN"
export SACKED_CNC_DEVMODE="$CNC_DEVMODE"

mkdir -p "$BUILD" "$DIST"

log "Sacked Wine wrapper build"
"$ROOT/scripts/fetch-downloads.sh"
"$ROOT/scripts/preflight.sh"
"$ROOT/scripts/fetch-wine.sh"
"$ROOT/scripts/make-prefix.sh"
"$ROOT/scripts/install-game.sh"

if [[ "$SKIP_SMOKE" == "1" ]]; then
    warn "Skipping runtime smoke test."
else
    "$ROOT/scripts/smoke-test.sh"
fi

"$ROOT/scripts/package-app.sh"
"$ROOT/scripts/verify.sh"

echo
log "Done."
log "Launch with: open \"$APP\""
