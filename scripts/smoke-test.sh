#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ -x "$WINE_BIN" ]] || die "Wine is not installed."
[[ -f "$GAME_PATH_FILE" ]] || die "Game path is missing. Run install-game.sh first."
[[ -f "$GAME_REL_FILE" ]] || die "Game relative path is missing. Run install-game.sh first."

GAME_WIN="$(cat "$GAME_PATH_FILE")"
GAME_REL="$(cat "$GAME_REL_FILE")"
GAME_UNIX="$PREFIX/$GAME_REL"
GAME_DIR="$(dirname "$GAME_UNIX")"

[[ -f "$GAME_UNIX" ]] || die "Game executable is missing: $GAME_UNIX"
[[ -f "$GAME_DIR/ddraw.dll" ]] || die "cnc-ddraw is missing from the game directory."
[[ -f "$GAME_DIR/ddraw.ini" ]] || die "cnc-ddraw configuration is missing."

cd "$GAME_DIR"
export WINEPREFIX="$PREFIX"
export WINEDEBUG="${SACKED_BUILD_WINEDEBUG:-fixme-all,err+all}"
export WINEDLLOVERRIDES="${SACKED_DLLOVERRIDES:-ddraw=n,b;mscoree,mshtml=}"

"$WINE_BIN" reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1 || true

LOG="$BUILD/smoke.log"
: > "$LOG"

log "Launching smoke test for 25 seconds..."
"$WINE_BIN" "$GAME_WIN" >>"$LOG" 2>&1 &
PID=$!

sleep 25

CRASHED=0
if grep -qE 'Unhandled exception|page fault|Exception frame is not in stack limits|winedbg' "$LOG"; then
    CRASHED=1
fi

GAME_RUNNING=0
if pgrep -f 'sacked\.exe' >/dev/null 2>&1; then
    GAME_RUNNING=1
fi

WINE_ALIVE=0
if kill -0 "$PID" 2>/dev/null; then
    WINE_ALIVE=1
fi

if [[ "$CRASHED" -eq 1 ]] || [[ "$GAME_RUNNING" -eq 0 && "$WINE_ALIVE" -eq 0 ]]; then
    wait "$PID" 2>/dev/null || true
    tail -n 80 "$LOG" >&2 || true
    "$WINESERVER_BIN" -k >/dev/null 2>&1 || true
    if [[ "$CRASHED" -eq 1 ]]; then
        die "The game crashed during the smoke test. See $LOG"
    fi
    die "The game exited during the smoke test. See $LOG"
fi

if [[ "$GAME_RUNNING" -eq 1 ]]; then
    log "Smoke test: sacked.exe is running."
else
    warn "Could not confirm sacked.exe by name, but the Wine process is alive."
fi
screencapture -x "$BUILD/smoke.png" >/dev/null 2>&1 || warn "Could not capture a screenshot."

kill "$PID" 2>/dev/null || true
"$WINESERVER_BIN" -k >/dev/null 2>&1 || true
sleep 1

log "Smoke test passed."
