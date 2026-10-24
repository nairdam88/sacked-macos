#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

GAME_DIR="${1:-}"
[[ -n "$GAME_DIR" ]] || die "Usage: install-cnc-ddraw.sh <game-directory>"
[[ -d "$GAME_DIR" ]] || die "Game directory not found: $GAME_DIR"
[[ -f "$CNC_DDRAW_DLL" ]] || die "cnc-ddraw is missing: $CNC_DDRAW_DLL"

case "${SACKED_CNC_WINDOWED:-1}" in
    1|true|TRUE|yes|YES) WINDOWED="true" ;;
    *) WINDOWED="false" ;;
esac

case "${SACKED_CNC_DEVMODE:-0}" in
    1|true|TRUE|yes|YES) DEVMODE="true" ;;
    *) DEVMODE="false" ;;
esac

cp "$CNC_DDRAW_DLL" "$GAME_DIR/ddraw.dll"

cat > "$GAME_DIR/ddraw.ini" <<EOF
[ddraw]
width=0
height=0
fullscreen=false
windowed=$WINDOWED
maintas=true
aspect_ratio=
boxing=false
maxfps=-1
vsync=false
adjmouse=true
shader=
posX=-32000
posY=-32000
renderer=gdi
devmode=$DEVMODE
border=true
savesettings=1
resizable=false
d3d9_filter=2
anti_aliased_fonts_min_size=13
min_font_size=0
center_window=1
inject_resolution=
vhack=false
screenshotdir=.\\Screenshots\\
toggle_borderless=false
toggle_upscaled=false
noactivateapp=false
maxgameticks=0
limiter_type=0
minfps=0
nonexclusive=true
EOF

if [[ -d "$CNC_DDRAW_SHADERS" ]]; then
    rm -rf "$GAME_DIR/Shaders"
    if command -v ditto >/dev/null 2>&1; then
        ditto "$CNC_DDRAW_SHADERS" "$GAME_DIR/Shaders"
    else
        cp -R "$CNC_DDRAW_SHADERS" "$GAME_DIR/Shaders"
    fi
fi
