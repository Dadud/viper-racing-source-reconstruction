# DirectDraw startup

`plan_directdraw_startup` models the ordered application path rooted at
`0x44E080`. The outer function clears its initial graphics state before calling
`0x44E460`. That function resets the selected-device globals and calls
`0x44E560`, which enumerates devices only when `DAT_004D9CF0` says enumeration
has not already happened; if the callback has not selected a device, it calls
the fallback selector `0x44E4D0(0)`. That selector builds a DirectDraw object
through `0x458470`, queries its capability structures through `0x458CB0`, and
accepts the object only when bit 0 of the observed capability field is set. It
stores the accepted object globally; otherwise it releases the rejected object
through `0x458540`. `0x458470` releases the base DirectDraw interface after
QueryInterface whether that query succeeds or fails. DirectDraw creation
failure logs its message and exits with code 2; DirectDraw2 QueryInterface
failure logs the HRESULT and exits with code 2. After a successful query, it
allocates a four-byte wrapper and stores the DirectDraw2 pointer in it. If that
allocation fails, the wrapper returns null and the path falls through to the
no-device message/exit 4; no release of the queried DirectDraw2 interface is
visible on that path.

On a selected device, `0x44EEB0` enumerates modes and passes each record to
`0x44EE00`. It initializes a `0x6C`-byte mode description with flags `0x1001`
and surface caps `0x2000`; wrapper `0x458D60` calls the DirectDraw vtable's
mode-enumeration slot with zero flags and logs an HRESULT error if the call
fails. The callback recognizes exactly 320x200, 512x384, 640x480,
800x600, and 1024x768 by width and height, sets a separate global flag for
each, and returns 1 for a recognized mode or 0 for any other mode. The
initializer requires the 640x480 flag; this callback does not compare color
depth. Missing support logs the mode error and exits with code 5. The next
stage `0x458D20` sets exclusive mode using flags `0x11`; failure
logs the DirectDraw error and exits with code 2. On success, `0x44E080` clears
the subsequent render-state globals and sets the initialized flag. Device
enumeration results, mode support, and native DirectDraw results remain inputs;
this portable model does not create COM objects or alter a display.

Evidence:

- `0x44E080`: `ev_78ecef39c0b592f1c5a9a02738547909961221f22c0fd55919b3e317caab24dc`
- `0x44E460`: `ev_dda278a7fada7309419d6d5299c61ea57289fd7f52b10d737f5bfade76e24071`
- `0x44E560`: `ev_34fd6a3fb1cac64e89994682152bc1c56b14789e1004b82e25fa511564743e0f`
- `0x44EEB0`: `ev_5d499e8403490489758ca993366a06bf222bc77aa73e7013a249957c0b259609`
- `0x458D20`: `ev_bdcf08f2b252e0b06d341eae1aa4ecd586288f52f8af29fb6cfae672b8e743ce`
- `0x44E4D0`: `ev_6f504b6a5a6f2e8b651c7f1df0e373f875451d0d55fad333560bece4278dd2c6`
- `0x458CB0`: `ev_63eb342cae8948075dc2e688589c367eff8021e8d8f3730677436519e53344e4`
- `0x458540`: `ev_5f389f2e5e3acc65bfc35a0035931046ad71a39a2145f19054e2a4efa14fb174`
- `0x458C80`: `ev_29bece3388653074c959a6c553448bc6b3d6240962c9ee6f559e8aa616ff4d61`
- `0x44EE00` callback bytes: `ev_83ced40ae522b925b9aa5519d3612ade5290e2c2aebae303af1304cc029a4ba9`
- `0x458470`: `ev_9665ed7bfb7c711d15f2d19e30313017cae5cd70c8d2de5dc7b7b04c2314d542`
- `0x458D60`: `ev_911568a9e79753d182e2e1c1fbd2ef053ac328ddae00b0334aa3da6d00c83bfb`
