#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

VERSION="${SACKED_VERSION:-1.0.0}"
ZIP="$DIST/Sacked-${VERSION}-arm64.zip"

[[ -d "$APP" ]] || die "App bundle not found: $APP"

rm -f "$ZIP" "$DIST/SHA256SUMS.txt"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"
shasum -a 256 "$ZIP" > "$DIST/SHA256SUMS.txt"
printf 'release asset: %s\n' "$ZIP"
cat "$DIST/SHA256SUMS.txt"
