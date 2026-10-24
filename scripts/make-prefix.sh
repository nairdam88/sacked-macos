#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ -x "$WINE_BIN" ]] || die "Wine is not installed. Run fetch-wine.sh first."

mkdir -p "$BUILD"
export WINEPREFIX="$PREFIX"
export WINEDEBUG="${SACKED_BUILD_WINEDEBUG:-fixme-all,err+all}"
export WINEDLLOVERRIDES="mscoree,mshtml="

if [[ ! -f "$PREFIX/system.reg" ]]; then
    log "Creating Wine prefix..."
    if ! "$WINEBOOT_BIN" -u >>"$BUILD/prefix.log" 2>&1; then
        tail -n 40 "$BUILD/prefix.log" >&2 || true
        die "Wine prefix creation failed."
    fi
    "$WINESERVER_BIN" -w >/dev/null 2>&1 || true
fi

[[ -f "$PREFIX/system.reg" ]] || die "Wine prefix is incomplete."

"$WINE_BIN" reg add 'HKCU\Software\Wine' /v Version /d "$SACKED_WINDOWS_VERSION" /f >/dev/null 2>&1 || true
"$WINE_BIN" reg add 'HKCU\Software\Wine\Drivers' /v Audio /d coreaudio /f >/dev/null 2>&1 || true

log "Wine prefix ready: $PREFIX"
