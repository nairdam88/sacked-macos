#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

GAME_DIR="${1:-}"
[[ -n "$GAME_DIR" ]] || die "Usage: install-hotkeys.sh <game-directory>"
[[ -d "$GAME_DIR" ]] || die "Game directory not found: $GAME_DIR"
[[ -f "$HOTKEYS_EXE" ]] || die "Hotkey helper is missing: $HOTKEYS_EXE"
[[ -f "$MENU_DLL" ]] || die "Menu hook DLL is missing: $MENU_DLL"

cp "$HOTKEYS_EXE" "$GAME_DIR/sacked-hotkeys.exe"
cp "$MENU_DLL" "$GAME_DIR/sacked-menu.dll"
