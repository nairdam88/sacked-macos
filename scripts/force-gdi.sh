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

for key in renderer shader width height fullscreen windowed maintas resizable; do
    grep -q "^${key}=" "$INI" || {
        echo "ddraw.ini is missing key: $key" >&2
        exit 1
    }
done

TMP="${INI}.tmp.$$"
trap 'rm -f "$TMP"' EXIT

awk '
    BEGIN { in_ddraw = 0 }
    /^\[ddraw\]/ { in_ddraw = 1; print; next }
    /^\[/ { in_ddraw = 0; print; next }
    in_ddraw && /^renderer=/ { print "renderer=gdi"; next }
    in_ddraw && /^shader=/ { print "shader="; next }
    in_ddraw && /^width=/ { print "width=0"; next }
    in_ddraw && /^height=/ { print "height=0"; next }
    in_ddraw && /^fullscreen=/ { print "fullscreen=false"; next }
    in_ddraw && /^windowed=/ { print "windowed=true"; next }
    in_ddraw && /^maintas=/ { print "maintas=true"; next }
    in_ddraw && /^resizable=/ { print "resizable=false"; next }
    { print }
' "$INI" > "$TMP"

mv "$TMP" "$INI"
printf 'renderer: gdi\n'
