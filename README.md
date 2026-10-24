# Sacked! Your Last Day — macOS app

A free one-click macOS app that runs the original 2003 Windows game **Sacked!
Your Last Day** through Wine.

**Download the latest release:**
https://github.com/nairdam88/sacked-macos/releases/latest

Proudly vibecoded.

## Play right away

1. Download `Sacked-*-arm64.zip` from the latest release.
2. Unzip it.
3. Move `Sacked.app` to Applications.
4. Install Rosetta 2 if you do not have it:

   ```sh
   /usr/sbin/softwareupdate --install-rosetta --agree-to-license
   ```

5. Right-click `Sacked.app` and choose **Open**.

The app is ad-hoc signed, not notarized, so the first launch needs
right-click Open. After that it opens normally.

The app is self-contained. On first launch it extracts the game data to:

```text
~/Library/Application Support/Sacked/
```

Saves and settings stay there, outside the app bundle.

## Requirements

- Apple Silicon Mac
- macOS 14.6 or later
- Rosetta 2
- About 2 GB of free disk space

## What this release adds and fixes

- **One-click macOS app** for the original Windows game.
- **Bundled Wine 11.18** runtime; no separate Wine install.
- **cnc-ddraw with the GDI renderer**, avoiding the OpenGL/shader crash path.
- **GDI is forced on every launch**, so stale shader settings cannot return.
- **Safe Esc/main-menu transition** through a 32-bit game-thread DLL hook.
  The helper no longer suspends the game or calls game UI code from a remote
  thread.
- **Compact vertical OSD** with Fullscreen, Language, and Quit rows.
- **Fullscreen/quit/OSD hotkeys** that work even when Alt+Enter does not.
- **English/Hungarian switching** from the OSD, with automatic restart.
- **Original 4:3 presentation** at 800x600.
- **Saves live outside the signed app**, so rebuilding or updating the app does
  not erase progress.

## Hotkeys

| Hotkey | Action |
| --- | --- |
| `Esc` | Return to the main menu |
| `Control+Command+F` | Toggle fullscreen/windowed |
| `Control+Command+Q` | Quit game |
| `Control+Command+H` | Show/hide the OSD |

Wine maps macOS Command to Win32 Alt. macOS may reserve Control+Command+F or
Control+Command+Q; use the OSD buttons if those combinations do not reach the
game.

The OSD appears for 10 seconds at startup and can be recalled with
Control+Command+H. It is positioned at the top-left of the game window.

## Build from source

### Requirements

- Apple Silicon Mac
- macOS 14.6 or later
- Xcode Command Line Tools
- Rosetta 2
- About 4 GB of free disk space
- `curl`, `tar`, `shasum`, `ditto`, `plutil`, `sips`, `iconutil`, `codesign`,
  `xattr`, `osascript`, and `pgrep`

### Build

```sh
git clone https://github.com/nairdam88/sacked-macos.git
cd sacked-macos
./build.command
```

If the executable bit was lost:

```sh
chmod +x build.command scripts/*.sh
./build.command
```

The build downloads the game installer automatically and verifies it:

- Game installer from the Internet Archive item `sacked_202610`

It also tries to download the original Hungarian patch for provenance. The
Magyarítások Portál download can reject automated CI requests, so that check is
optional. The extracted English/Hungarian overlays are included in the
repository and are what the build actually installs.

You can use a local installer instead:

```sh
SACKED_INSTALLER=/path/to/Sacked.EXE ./build.command
```

Useful options:

```sh
./build.command --skip-smoke
./build.command --cnc-debug
./build.command --no-sign
```

The result is:

```text
dist/Sacked.app
```

## How it works

- `scripts/fetch-downloads.sh` downloads and checksum-verifies the game
  installer and translation patch.
- `scripts/preflight.sh` checks the Mac, Rosetta, tools, disk space, and
  installer.
- `scripts/fetch-wine.sh` downloads the pinned Gcenx Wine 11.18 runtime and
  verifies its checksum.
- `scripts/make-prefix.sh` creates an isolated Wine prefix.
- `scripts/install-game.sh` installs the game and records its path.
- `scripts/install-cnc-ddraw.sh` installs cnc-ddraw configured for GDI.
- `scripts/install-hotkeys.sh` installs the helper and menu DLL.
- `scripts/install-translations.sh` installs the English/Hungarian overlays.
- `scripts/package-app.sh` builds `dist/Sacked.app` and installs writable data
  under Application Support.
- `scripts/verify.sh` checks the bundle and prefix.
- `.github/workflows/release.yml` builds and publishes the macOS release.

## Troubleshooting

### Gatekeeper blocks the app

Right-click the app and choose **Open**, or clear quarantine:

```sh
xattr -dr com.apple.quarantine /Applications/Sacked.app
```

### The game exits immediately

Check:

```text
~/Library/Logs/Sacked.log
build/smoke.log
build/install.log
~/Library/Application Support/Sacked/prefix/drive_c/Sacked/ddraw.log
~/Library/Application Support/Sacked/prefix/drive_c/Sacked/sacked-hotkeys.log
~/Library/Application Support/Sacked/prefix/drive_c/Sacked/sacked-menu.log
```

For more Wine diagnostics:

```sh
SACKED_WINEDEBUG=+ddraw,+dinput,+dsound /Applications/Sacked.app/Contents/MacOS/Sacked
```

### Corrupt or incomplete game data

Delete the writable data and relaunch; the app recreates it from the bundled
prefix:

```sh
rm -rf "$HOME/Library/Application Support/Sacked"
open /Applications/Sacked.app
```

### Rebuild from scratch

```sh
rm -rf build dist
./build.command
```

## Credits and licenses

Wrapper and helper source code: MIT. See `LICENSE`.

Sacked! Your Last Day was created by Limbic Entertainment GmbH. The game
installer is distributed through the Internet Archive item `sacked_202610`:

https://archive.org/details/sacked_202610

Hungarian translation by Wicktor89, published on Magyarítások Portál:

https://magyaritasok.hu/download/357/sacked

The translation is used under **CC BY-NC-ND 4.0**. It is included unmodified
and attributed here. This app is a free, non-commercial fan release.

Wine is licensed under LGPL-2.1-or-later. cnc-ddraw is MIT licensed. See
`packaging/THIRD_PARTY_NOTICES.txt` and the notices inside the built app.

Game assets and trademarks remain the property of their respective owners.

## Known limitations

- The app is ad-hoc signed, not notarized.
- Apple Silicon and Rosetta 2 are required.
- The original game renders at 800x600 4:3; there is no true widescreen mode.
- The OSD cannot reliably appear over exclusive-fullscreen DirectDraw; use
  windowed or borderless mode.
