# Sacked! Your Last Day — macOS app

A free one-click macOS release of the original Windows game, running through Wine.

## Download

Download the `Sacked-*-arm64.zip` asset below, unzip it, and move `Sacked.app` to Applications.

This build is ad-hoc signed, not notarized. On first launch, right-click the app and choose **Open**. You need **Rosetta 2** on Apple Silicon.

## Included

- Original Sacked! game and English/Hungarian overlays
- Bundled Wine 11.18 runtime
- GDI-only cnc-ddraw rendering
- Compact OSD with Fullscreen, Language, and Quit controls
- Esc returns to the main menu through a safe game-thread DLL hook
- Save data stored outside the app bundle

## Checksums

See `SHA256SUMS.txt` in this release.

## Credits

Sacked! Your Last Day was created by Limbic Entertainment. The game installer is distributed through the Internet Archive item `sacked_202610`.

Hungarian translation by Wicktor89, published on Magyarítások Portál, used under CC BY-NC-ND 4.0.

Wine is LGPL-2.1-or-later. cnc-ddraw is MIT licensed.

Proudly vibecoded.
