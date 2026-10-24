# cnc-ddraw

Bundled DirectDraw compatibility layer for Sacked under Wine.

- Upstream: https://github.com/FunkyFr3sh/cnc-ddraw
- Version: v7.1.0.0
- Archive: cnc-ddraw.zip
- Archive SHA-256: `0b13ab89a64c9918189b1dadd449ef6ed3cb3b7b19cabd96d8adbd95505bb908`
- `ddraw.dll` SHA-256: `85e0f7d530dfda134793a57cb3e76b0287dcc96892ee57162dd68f47283b03a9`
- License: MIT (see `LICENSE`)

The game directory receives `ddraw.dll`, a generated `ddraw.ini`, and the
optional shader files. Wine is launched with `ddraw=n,b` so this native
DirectDraw implementation is used instead of Wine's wined3d renderer.
