# Translations

Extracted overlays used by the launcher. The original Hungarian patch
installer (`sackedhun.exe`) is not required at runtime.

The build downloads the patch and verifies its SHA-256 for provenance. The
extracted files are committed so the game can switch languages without a
second full game copy and without running an old Windows installer.

## Hungarian

- Source: Magyarítások Portál, Sacked / Gefeuert! Dein letzter Tag
- Translator: Wicktor89 (Hun Patch v1.0)
- Page: https://magyaritasok.hu/download/357/sacked
- License: CC BY-NC-ND 4.0
- Contents: translated `sacked.exe` and translated `Levels/*.txt` files.
- The patch installer itself is not redistributed here; the build downloads
  and checksum-verifies it.
- No OGG/OGD assets or fonts were changed by the patch.

`en/` contains the matching original executable and level text files so the
launcher can switch languages without a second 250 MB game copy.

This is fan-made translation content. It is included unmodified and
attributed under its non-commercial license.
