#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

GAME_DIR="${1:-}"
[[ -n "$GAME_DIR" ]] || die "Usage: install-translations.sh <game-directory>"
[[ -d "$GAME_DIR" ]] || die "Game directory not found: $GAME_DIR"
[[ -f "$TRANSLATIONS_DIR/en/sacked.exe" ]] || die "Missing translations/en/sacked.exe"
[[ -f "$TRANSLATIONS_DIR/hu/sacked.exe" ]] || die "Missing translations/hu/sacked.exe"

for language in en hu; do
    source_dir="$TRANSLATIONS_DIR/$language"
    target_dir="$GAME_DIR/translations/$language"

    rm -rf "$target_dir"
    mkdir -p "$target_dir/Levels"
    cp "$source_dir/sacked.exe" "$target_dir/sacked.exe"
    cp "$source_dir/Levels"/*.txt "$target_dir/Levels/"
done
