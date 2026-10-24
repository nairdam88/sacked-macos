#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ -x "$WINE_BIN" ]] || die "Wine is not installed."
[[ -f "$INSTALLER_HINT_FILE" ]] || die "Installer path is missing. Run preflight.sh first."

INSTALLER="$(cat "$INSTALLER_HINT_FILE")"
[[ -f "$INSTALLER" ]] || die "Installer not found: $INSTALLER"

export WINEPREFIX="$PREFIX"
export WINEDEBUG="${SACKED_BUILD_WINEDEBUG:-fixme-all,err+all}"
export WINEDLLOVERRIDES="mscoree,mshtml="

find_game() {
    find "$PREFIX/drive_c" -type f -iname 'sacked.exe' 2>/dev/null | grep -v '/windows/' | head -n 1 || true
}

GAME_EXE="$(find_game)"

if [[ -z "$GAME_EXE" ]]; then
    log "Running the Sacked installer..."
    set +e
    "$WINE_BIN" "$INSTALLER" /S /D=C:\\Sacked >>"$BUILD/install.log" 2>&1
    INSTALL_STATUS=$?
    set -e
    "$WINESERVER_BIN" -w >/dev/null 2>&1 || true
    if [[ "$INSTALL_STATUS" -ne 0 ]]; then
        warn "The installer returned status $INSTALL_STATUS."
    fi
    GAME_EXE="$(find_game)"
fi

if [[ -z "$GAME_EXE" ]] && [[ -d "$ROOT/game" ]]; then
    log "Installer did not produce sacked.exe; using game/ fallback."
    mkdir -p "$PREFIX/drive_c/Sacked"
    ditto "$ROOT/game" "$PREFIX/drive_c/Sacked"
    GAME_EXE="$(find_game)"
fi

if [[ -z "$GAME_EXE" ]]; then
    tail -n 40 "$BUILD/install.log" >&2 2>/dev/null || true
    die "Could not find sacked.exe after installation."
fi

GAME_SIZE="$(file_size "$GAME_EXE")"
if (( GAME_SIZE < 100000 )); then
    warn "Installed sacked.exe is unexpectedly small ($GAME_SIZE bytes)."
fi

"$ROOT/scripts/install-cnc-ddraw.sh" "$(dirname "$GAME_EXE")"
"$ROOT/scripts/install-hotkeys.sh" "$(dirname "$GAME_EXE")"
"$ROOT/scripts/install-translations.sh" "$(dirname "$GAME_EXE")"

GAME_WIN="$("$WINEPATH_BIN" -w "$GAME_EXE" | tr -d '\r' | tail -n 1)"
[[ -n "$GAME_WIN" ]] || die "Could not convert the game path to a Windows path."

GAME_REL="${GAME_EXE#"$PREFIX"/}"
[[ "$GAME_REL" != "$GAME_EXE" ]] || die "Installed game is outside the Wine prefix."

printf '%s\n' "$GAME_WIN" > "$GAME_PATH_FILE"
printf '%s\n' "$GAME_REL" > "$GAME_REL_FILE"
"$WINE_BIN" reg add 'HKCU\Software\Limbic Entertainment\Gefeuert' /f >/dev/null 2>&1 || true

log "Installed: $GAME_WIN ($GAME_SIZE bytes)"
