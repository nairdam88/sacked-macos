#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

mkdir -p "$BUILD/downloads"

file_ok() {
    local path="$1"
    local hash="$2"
    local size="$3"
    [[ -f "$path" ]] || return 1
    [[ "$(sha256_file "$path")" == "$hash" ]] || return 1
    [[ "$(file_size "$path")" == "$size" ]] || return 1
}

download_verified() {
    local label="$1"
    local url="$2"
    local output="$3"
    local hash="$4"
    local size="$5"
    local partial="${output}.part"

    if file_ok "$output" "$hash" "$size"; then
        log "Using cached $label: $output"
        return
    fi

    rm -f "$partial"
    log "Downloading $label..."
    curl --fail --location --retry 3 --retry-delay 2 --output "$partial" "$url"
    if ! file_ok "$partial" "$hash" "$size"; then
        rm -f "$partial"
        die "$label checksum or size mismatch."
    fi
    mv "$partial" "$output"
    log "Verified $label: $output"
}

GAME_LOCAL=""
if [[ -n "${SACKED_INSTALLER:-}" ]]; then
    GAME_LOCAL="$SACKED_INSTALLER"
else
    for candidate in "$ROOT/Sacked.EXE" "$ROOT/Sacked.exe" "$ROOT/../Sacked.EXE" "$ROOT/../Sacked.exe" "$ROOT/installer/Sacked.EXE"; do
        if [[ -f "$candidate" ]]; then
            GAME_LOCAL="$candidate"
            break
        fi
    done
fi

if [[ -n "$GAME_LOCAL" ]] && file_ok "$GAME_LOCAL" "$SACKED_SHA256" "$SACKED_SIZE"; then
    log "Using local game installer: $GAME_LOCAL"
else
    download_verified "Sacked.EXE" "$SACKED_URL" "$BUILD/downloads/Sacked.EXE" "$SACKED_SHA256" "$SACKED_SIZE"
fi

TRANSLATION_LOCAL=""
for candidate in "$ROOT/sackedhun.exe" "$ROOT/../sackedhun.exe" "$ROOT/build/downloads/sackedhun.exe"; do
    if [[ -f "$candidate" ]]; then
        TRANSLATION_LOCAL="$candidate"
        break
    fi
done

if [[ -n "$TRANSLATION_LOCAL" ]] && file_ok "$TRANSLATION_LOCAL" "$TRANSLATION_SHA256" "$TRANSLATION_SIZE"; then
    log "Using local translation patch: $TRANSLATION_LOCAL"
else
    download_verified "translation patch" "$TRANSLATION_URL" "$BUILD/downloads/sackedhun.exe" "$TRANSLATION_SHA256" "$TRANSLATION_SIZE"
fi
