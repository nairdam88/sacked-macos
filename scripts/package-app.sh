#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ -d "$WINE_RUNTIME" ]] || die "Wine runtime is missing."
[[ -d "$PREFIX" ]] || die "Wine prefix is missing."
[[ -f "$GAME_PATH_FILE" ]] || die "Game path is missing."
[[ -f "$GAME_REL_FILE" ]] || die "Game relative path is missing."

GAME_WIN="$(cat "$GAME_PATH_FILE")"
GAME_REL="$(cat "$GAME_REL_FILE")"
[[ -n "$GAME_WIN" ]] || die "Game path file is empty."
[[ -n "$GAME_REL" ]] || die "Game relative path file is empty."

log "Assembling Sacked.app..."
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

ditto "$WINE_RUNTIME" "$APP/Contents/Resources/wine"

PREFIX_ARCHIVE="$BUILD/prefix.tar.xz"
rm -f "$PREFIX_ARCHIVE"
log "Compressing the Wine prefix..."
tar -cJf "$PREFIX_ARCHIVE" -C "$BUILD" prefix
cp "$PREFIX_ARCHIVE" "$APP/Contents/Resources/prefix.tar.xz"

printf '%s\n' "$GAME_WIN" > "$APP/Contents/Resources/game-path.txt"
printf '%s\n' "$GAME_REL" > "$APP/Contents/Resources/game-rel.txt"

cp "$ROOT/packaging/Info.plist" "$APP/Contents/Info.plist"
cp "$ROOT/packaging/Sacked" "$APP/Contents/MacOS/Sacked"
cp "$ROOT/tools/sacked-hotkeys.exe" "$APP/Contents/Resources/sacked-hotkeys.exe"
cp "$ROOT/tools/sacked-menu.dll" "$APP/Contents/Resources/sacked-menu.dll"
cp "$ROOT/packaging/THIRD_PARTY_NOTICES.txt" "$APP/Contents/Resources/THIRD_PARTY_NOTICES.txt"
cp "$ROOT/LICENSE" "$APP/Contents/Resources/LICENSE.txt"
cp "$ROOT/third_party/cnc-ddraw/LICENSE" "$APP/Contents/Resources/LICENSE.cnc-ddraw.txt"
cp "$ROOT/scripts/force-gdi.sh" "$APP/Contents/Resources/force-gdi.sh"
"$ROOT/scripts/make-icns.sh" "$APP/Contents/Resources/Sacked.icns"
chmod +x "$APP/Contents/MacOS/Sacked" "$APP/Contents/Resources/force-gdi.sh"

xattr -dr com.apple.quarantine "$APP" >/dev/null 2>&1 || true

if [[ "${SACKED_SKIP_SIGN:-0}" != "1" ]]; then
    log "Ad-hoc signing..."
    if ! codesign --force --deep --sign - "$APP" >>"$BUILD/codesign.log" 2>&1; then
        tail -n 40 "$BUILD/codesign.log" >&2 || true
        warn "Code signing failed; the app will need a right-click Open."
    fi
    codesign --verify --deep --strict "$APP" >>"$BUILD/codesign.log" 2>&1 || warn "Code signature verification reported issues."
fi

if [[ "${SACKED_SKIP_SUPPORT_INSTALL:-0}" != "1" ]]; then
    mkdir -p "$SUPPORT_DIR"
    if [[ ! -f "$SUPPORT_PREFIX/system.reg" ]]; then
        log "Installing writable game data to $SUPPORT_DIR..."
        ditto "$PREFIX" "$SUPPORT_PREFIX"
    fi
    SUPPORT_GAME_EXE="$SUPPORT_PREFIX/$GAME_REL"
    if [[ -f "$SUPPORT_GAME_EXE" ]]; then
        "$ROOT/scripts/install-cnc-ddraw.sh" "$(dirname "$SUPPORT_GAME_EXE")"
        "$ROOT/scripts/install-hotkeys.sh" "$(dirname "$SUPPORT_GAME_EXE")"
        "$ROOT/scripts/install-translations.sh" "$(dirname "$SUPPORT_GAME_EXE")"
    fi
fi

log "Built: $APP"
