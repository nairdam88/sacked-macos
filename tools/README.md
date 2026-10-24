# sacked-hotkeys

Win32 helper started with the game. It also draws the in-game OSD.

- `Esc` — return to the main menu through `sacked-menu.dll`
- `Control+Command+F` — toggle fullscreen/windowed
- `Control+Command+Q` — quit the game
- `Control+Command+H` — show/hide the in-game OSD

The helper finds the `sacked.exe` window and sends cnc-ddraw's
`WM_TOGGLE_FULLSCREEN` message directly, so it does not depend on
cnc-ddraw's keyboard hook. After a fullscreen transition it sends
cnc-ddraw's auto-renderer message to refresh the cursor clip.

The OSD can toggle between the default 800x600 window and a 1200x900 window.
That mode is stored in `window-mode` and applied by the launcher at restart.

The main-menu transition is handled by `sacked-menu.dll`. The helper injects
the DLL with `LoadLibraryA`, and the DLL subclasses the game window. The helper
sends a private message; the subclassed window procedure calls
`CGame::SetState(3)` on the game's own thread. There is no `SuspendThread` or
`TerminateThread`, and no state call is made from a remote thread.

It also accepts one-shot CLI actions for external helpers:

```sh
sacked-hotkeys.exe toggle
sacked-hotkeys.exe quit
sacked-hotkeys.exe menu
```

Build on Linux with MinGW:

```sh
./tools/build-hotkeys.sh
./tools/build-menu.sh
```

Prebuilt binary SHA-256:

- `sacked-hotkeys.exe`: `78b35e1feb39ffda664e30ec6a190123abf29a97ab33de3744d68f6137c7ecc6`
- `sacked-menu.dll`: `218faedbf1b7acda01954b027c642c293d62f44ca77a3cbd2f7b052b7305161a`

Both DLL and helper must be 32-bit.
