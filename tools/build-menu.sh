#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC="${CC:-i686-w64-mingw32-gcc}"
OUT="$ROOT/tools/sacked-menu.dll"

command -v "$CC" >/dev/null 2>&1 || {
    echo "error: $CC is required" >&2
    exit 1
}

"$CC" -O2 -Wall -Wextra -shared -Wl,--exclude-all-symbols -Wl,--no-insert-timestamp \
    -o "$OUT" \
    "$ROOT/tools/sacked-menu.c" \
    "$ROOT/tools/sacked-menu-call.S" \
    -luser32 -lkernel32

file "$OUT"
