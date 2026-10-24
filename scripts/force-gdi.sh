#!/usr/bin/env bash
set -euo pipefail

INI="${1:-}"
[[ -n "$INI" ]] || {
    echo "usage: force-gdi.sh <ddraw.ini>" >&2
    exit 1
}
[[ -f "$INI" ]] || {
    echo "missing ddraw.ini: $INI" >&2
    exit 1
}

WINDOW_WIDTH="${SACKED_WINDOW_WIDTH:-0}"
WINDOW_HEIGHT="${SACKED_WINDOW_HEIGHT:-0}"
[[ "$WINDOW_WIDTH" =~ ^[0-9]+$ ]] || WINDOW_WIDTH=0
[[ "$WINDOW_HEIGHT" =~ ^[0-9]+$ ]] || WINDOW_HEIGHT=0

for key in renderer shader width height fullscreen windowed maintas resizable; do
    grep -q "^${key}=" "$INI" || {
        echo "ddraw.ini is missing key: $key" >&2
        exit 1
    }
done

TMP="${INI}.tmp.$$"
trap 'rm -f "$TMP"' EXIT

awk -v window_width="$WINDOW_WIDTH" -v window_height="$WINDOW_HEIGHT" '
    BEGIN { in_ddraw = 0; seen_center_fix = 0 }
    /^\[ddraw\]/ { in_ddraw = 1; print; next }
    /^\[/ {
        if (in_ddraw && !seen_center_fix) {
            print "center_cursor_fix=true"
            seen_center_fix = 1
        }
        in_ddraw = 0
        print
        next
    }
    in_ddraw && /^renderer=/ { print "renderer=gdi"; next }
    in_ddraw && /^shader=/ { print "shader="; next }
    in_ddraw && /^width=/ { print "width=" window_width; next }
    in_ddraw && /^height=/ { print "height=" window_height; next }
    in_ddraw && /^fullscreen=/ { print "fullscreen=false"; next }
    in_ddraw && /^windowed=/ { print "windowed=true"; next }
    in_ddraw && /^maintas=/ { print "maintas=true"; next }
    in_ddraw && /^resizable=/ { print "resizable=false"; next }
    in_ddraw && /^center_cursor_fix=/ {
        print "center_cursor_fix=true"
        seen_center_fix = 1
        next
    }
    { print }
    END {
        if (in_ddraw && !seen_center_fix)
            print "center_cursor_fix=true"
    }
' "$INI" > "$TMP"

mv "$TMP" "$INI"
printf 'renderer: gdi\nwindow: %sx%s\n' "$WINDOW_WIDTH" "$WINDOW_HEIGHT"
