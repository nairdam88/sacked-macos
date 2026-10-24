#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ -d "$APP" ]] || die "App bundle not found: $APP"

REQUIRED=(
    "Contents/MacOS/Sacked"
    "Contents/Info.plist"
    "Contents/Resources/Sacked.icns"
    "Contents/Resources/game-path.txt"
    "Contents/Resources/game-rel.txt"
    "Contents/Resources/THIRD_PARTY_NOTICES.txt"
    "Contents/Resources/LICENSE.txt"
    "Contents/Resources/LICENSE.cnc-ddraw.txt"
    "Contents/Resources/force-gdi.sh"
    "Contents/Resources/sacked-hotkeys.exe"
    "Contents/Resources/sacked-menu.dll"
    "Contents/Resources/prefix.tar.xz"
    "Contents/Resources/wine/bin/wine"
)

for relative in "${REQUIRED[@]}"; do
    [[ -e "$APP/$relative" ]] || die "Missing from app: $relative"
done

[[ -x "$APP/Contents/MacOS/Sacked" ]] || die "Launcher is not executable."
[[ -x "$APP/Contents/Resources/force-gdi.sh" ]] || die "GDI helper is not executable."

GAME_WIN="$(cat "$APP/Contents/Resources/game-path.txt")"
[[ "$GAME_WIN" == *[Ss]acked.exe ]] || die "Unexpected game path: $GAME_WIN"

plutil -lint "$APP/Contents/Info.plist" >/dev/null
[[ ! -e "$APP/Contents/Resources/Sacked.EXE" ]] || die "Installer was copied into the app."

tar -tJf "$APP/Contents/Resources/prefix.tar.xz" > "$BUILD/prefix-list.txt"
grep -q 'prefix/system.reg$' "$BUILD/prefix-list.txt" || die "Prefix archive is incomplete."
grep -q 'ddraw.dll$' "$BUILD/prefix-list.txt" || die "cnc-ddraw is missing from the prefix archive."
grep -q 'ddraw.ini$' "$BUILD/prefix-list.txt" || die "cnc-ddraw configuration is missing from the prefix archive."
grep -q 'sacked-hotkeys.exe$' "$BUILD/prefix-list.txt" || die "Hotkey helper is missing from the prefix archive."
grep -q 'sacked-menu.dll$' "$BUILD/prefix-list.txt" || die "Menu hook DLL is missing from the prefix archive."
grep -q 'translations/en/sacked.exe$' "$BUILD/prefix-list.txt" || die "English translation overlay is missing from the prefix archive."
grep -q 'translations/hu/sacked.exe$' "$BUILD/prefix-list.txt" || die "Hungarian translation overlay is missing from the prefix archive."

"$APP/Contents/MacOS/Sacked" --check >/dev/null

if [[ "${SACKED_SKIP_SIGN:-0}" != "1" ]]; then
    codesign --verify --deep "$APP" >/dev/null 2>&1 || warn "Code signature is not fully valid, but the app can still run locally."
fi

if [[ -f "$SUPPORT_PREFIX/system.reg" ]]; then
    SUPPORT_GAME_EXE="$SUPPORT_PREFIX/$(cat "$APP/Contents/Resources/game-rel.txt")"
    SUPPORT_GAME_DIR="$(dirname "$SUPPORT_GAME_EXE")"
    [[ -f "$SUPPORT_GAME_DIR/ddraw.dll" ]] || die "cnc-ddraw is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/ddraw.ini" ]] || die "cnc-ddraw configuration is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/sacked-hotkeys.exe" ]] || die "Hotkey helper is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/sacked-menu.dll" ]] || die "Menu hook DLL is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/translations/en/sacked.exe" ]] || die "English translation overlay is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/translations/hu/sacked.exe" ]] || die "Hungarian translation overlay is missing from the writable game data."
    [[ -f "$SUPPORT_GAME_DIR/translations/hu/Levels/Level_00.txt" ]] || die "Hungarian level text is missing."
    log "Writable game data: $SUPPORT_PREFIX"
else
    warn "Writable game data is not installed yet; it will be extracted on first launch."
fi

SIZE_MB="$(du -sm "$APP" | awk '{print $1}')"

log "Verified: $APP (${SIZE_MB} MB)"
log "Game: $GAME_WIN"
