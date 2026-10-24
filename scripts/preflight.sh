#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

[[ "$(uname -s)" == "Darwin" ]] || die "This builder must run on macOS."

if [[ "$(uname -m)" != "arm64" ]]; then
    warn "This build is intended for Apple Silicon."
    [[ "${SACKED_ALLOW_INTEL:-0}" == "1" ]] || die "Set SACKED_ALLOW_INTEL=1 to continue on Intel."
fi

need curl
need tar
need shasum
need awk
need sed
need grep
need ditto
need plutil
need sips
need iconutil
need codesign
need xattr
need osascript
need pgrep

[[ -f "$HOTKEYS_EXE" ]] || die "Hotkey helper is missing: $HOTKEYS_EXE"
[[ -f "$MENU_DLL" ]] || die "Menu hook DLL is missing: $MENU_DLL"

[[ -x /usr/bin/arch ]] || die "Missing /usr/bin/arch."

if ! /usr/bin/arch -x86_64 /usr/bin/true >/dev/null 2>&1; then
    die "Rosetta 2 is not installed. Run: /usr/sbin/softwareupdate --install-rosetta --agree-to-license"
fi

MACOS_VERSION="$(sw_vers -productVersion)"
MACOS_MAJOR="${MACOS_VERSION%%.*}"
if [[ "$MACOS_MAJOR" =~ ^[0-9]+$ ]] && (( MACOS_MAJOR < 14 )); then
    warn "macOS $MACOS_VERSION is older than the supported range."
fi

FREE_KB="$(df -Pk "$ROOT" | awk 'NR == 2 {print $4}')"
if [[ "$FREE_KB" =~ ^[0-9]+$ ]] && (( FREE_KB < 4000000 )); then
    die "At least 4 GB of free disk space is required."
fi

INSTALLER=""
CANDIDATES=()
if [[ -n "${SACKED_INSTALLER:-}" ]]; then
    CANDIDATES+=("$SACKED_INSTALLER")
fi
CANDIDATES+=(
    "$BUILD/downloads/Sacked.EXE"
    "$ROOT/Sacked.EXE"
    "$ROOT/Sacked.exe"
    "$ROOT/../Sacked.EXE"
    "$ROOT/../Sacked.exe"
    "$ROOT/installer/Sacked.EXE"
    "$ROOT/installer/Sacked.exe"
)

for candidate in "${CANDIDATES[@]}"; do
    if [[ -f "$candidate" ]]; then
        INSTALLER="$(cd "$(dirname "$candidate")" && pwd)/$(basename "$candidate")"
        break
    fi
done

[[ -n "$INSTALLER" ]] || die "Sacked.EXE was not found. Run scripts/fetch-downloads.sh or put it in $ROOT or its parent directory."

INSTALLER_SIZE="$(file_size "$INSTALLER")"
INSTALLER_HASH="$(sha256_file "$INSTALLER")"
if [[ "$INSTALLER_HASH" != "$SACKED_SHA256" ]]; then
    warn "Sacked.EXE checksum differs from the known installer."
    warn "Expected $SACKED_SHA256"
    warn "Got      $INSTALLER_HASH"
fi
if [[ "$INSTALLER_SIZE" != "$SACKED_SIZE" ]]; then
    warn "Sacked.EXE size is $INSTALLER_SIZE bytes, expected $SACKED_SIZE."
fi

mkdir -p "$BUILD"
printf '%s\n' "$INSTALLER" > "$INSTALLER_HINT_FILE"

log "macOS $MACOS_VERSION on $(uname -m)"
log "Installer: $INSTALLER"
log "Rosetta 2: available"
