#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

mkdir -p "$BUILD/downloads" "$BUILD/runtime"

ARCHIVE="$BUILD/downloads/$WINE_ASSET"

if [[ ! -f "$ARCHIVE" ]] || [[ "$(sha256_file "$ARCHIVE")" != "$WINE_SHA256" ]]; then
    rm -f "$ARCHIVE" "$ARCHIVE.part"
    log "Downloading Wine $WINE_VERSION ($WINE_FLAVOR)..."
    curl --fail --location --retry 3 --retry-delay 2 --output "$ARCHIVE.part" "$WINE_URL"
    [[ "$(sha256_file "$ARCHIVE.part")" == "$WINE_SHA256" ]] || die "Wine archive checksum mismatch."
    mv "$ARCHIVE.part" "$ARCHIVE"
else
    log "Using cached Wine archive."
fi

if [[ ! -x "$WINE_BIN" ]]; then
    rm -rf "$BUILD/wine-extract" "$BUILD/runtime"
    mkdir -p "$BUILD/wine-extract" "$BUILD/runtime"

    log "Extracting Wine..."
    tar -xJf "$ARCHIVE" -C "$BUILD/wine-extract"

    WINE_APP=""
    for candidate in "$BUILD/wine-extract"/Wine*.app; do
        if [[ -d "$candidate" ]]; then
            WINE_APP="$candidate"
            break
        fi
    done
    [[ -n "$WINE_APP" ]] || die "The Wine archive did not contain a Wine app bundle."
    [[ -d "$WINE_APP/Contents/Resources/wine" ]] || die "The Wine app bundle is missing its runtime."

    ditto "$WINE_APP/Contents/Resources/wine" "$WINE_RUNTIME"
fi

xattr -dr com.apple.quarantine "$WINE_RUNTIME" >/dev/null 2>&1 || true
[[ -x "$WINE_BIN" ]] || die "Wine runtime is missing: $WINE_BIN"

if ! WINE_VERSION_OUTPUT="$("$WINE_BIN" --version 2>&1)"; then
    printf '%s\n' "$WINE_VERSION_OUTPUT" >&2
    die "Wine could not start. Check Rosetta 2 and try again."
fi

[[ "$WINE_VERSION_OUTPUT" == wine-* ]] || die "Unexpected Wine version output: $WINE_VERSION_OUTPUT"

log "$WINE_VERSION_OUTPUT"
