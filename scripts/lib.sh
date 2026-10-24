#!/usr/bin/env bash
set -euo pipefail

unset WINEARCH

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
APP="$DIST/Sacked.app"
PREFIX="$BUILD/prefix"
SUPPORT_DIR="$HOME/Library/Application Support/Sacked"
SUPPORT_PREFIX="$SUPPORT_DIR/prefix"
WINE_RUNTIME="$BUILD/runtime/wine"
WINE_BIN="$WINE_RUNTIME/bin/wine"
CNC_DDRAW_DIR="$ROOT/third_party/cnc-ddraw"
CNC_DDRAW_DLL="$CNC_DDRAW_DIR/ddraw.dll"
CNC_DDRAW_SHADERS="$CNC_DDRAW_DIR/Shaders"
HOTKEYS_EXE="$ROOT/tools/sacked-hotkeys.exe"
MENU_DLL="$ROOT/tools/sacked-menu.dll"
TRANSLATIONS_DIR="$ROOT/translations"
WINEBOOT_BIN="$WINE_RUNTIME/bin/wineboot"
WINESERVER_BIN="$WINE_RUNTIME/bin/wineserver"
WINEPATH_BIN="$WINE_RUNTIME/bin/winepath"
INSTALLER_HINT_FILE="$BUILD/installer-path.txt"
GAME_PATH_FILE="$BUILD/game-path.txt"
GAME_REL_FILE="$BUILD/game-rel.txt"

WINE_VERSION="${WINE_VERSION:-11.18}"
WINE_FLAVOR="${WINE_FLAVOR:-devel}"
WINE_ASSET="wine-${WINE_FLAVOR}-${WINE_VERSION}-osx64.tar.xz"
WINE_SHA256="${WINE_SHA256:-aa0ea4c82e636ae7bca2076387cb0a5affa26509ad13f119ecd0d62bd7ba6f82}"
WINE_URL="${WINE_URL:-https://github.com/Gcenx/macOS_Wine_builds/releases/download/${WINE_VERSION}/${WINE_ASSET}}"

export PATH="$WINE_RUNTIME/bin:${PATH:-/usr/bin:/bin:/usr/sbin:/sbin}"

SACKED_URL="${SACKED_URL:-https://archive.org/download/sacked_202610/Sacked.EXE}"
SACKED_SHA256="e77cb450636de1f6d6031856a2e67de5e8adb4178ec9e2bf3c59d6f506070f89"
SACKED_SIZE="75160974"
TRANSLATION_URL="${TRANSLATION_URL:-https://magyaritasok.hu/dl/sacked/sackedhun.exe}"
TRANSLATION_SHA256="c6494b2d49a09103f740c2a9c5eb9c8473b6059fc44cbd35abc26381ce59b873"
TRANSLATION_SIZE="482417"
SACKED_WINDOWS_VERSION="${SACKED_WINDOWS_VERSION:-winxp}"
SACKED_DLLOVERRIDES="${SACKED_DLLOVERRIDES:-ddraw=n,b;mscoree,mshtml=}"
SACKED_CNC_WINDOWED="${SACKED_CNC_WINDOWED:-1}"
SACKED_CNC_DEVMODE="${SACKED_CNC_DEVMODE:-0}"

log() { printf '==> %s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"; }
sha256_file() { shasum -a 256 "$1" | awk '{print $1}'; }
file_size() { stat -f%z "$1" 2>/dev/null || stat -c%s "$1"; }
