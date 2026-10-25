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
- `0x44E460`: `ev_ea2e9fc8650b6b06f777b97f1acc33976af9f37c2a41c70efabf5b915a3ca920`
- `0x44EEB0`: `ev_bf721d7a40dce61590fb82b63dbdd85f86a1bc19c0a6e2508b1dd45d44903cab`
- `0x44EC00`: `ev_71a52c712c10d46f0d4d9461eafe3dcadcb96a5598d5a6740130cbf77fa3f7e8`

## Controlled original-client failure capture

The private run `../../analysis/reference_captures/original_primary_adapter_20261025T022451Z.json`
uses the recovered `-1` primary-adapter switch, a temporary hash-verified
`Data` junction, and the original ISO mounted as `VIPER`. The launcher passes
its data/CD checks and starts `race.bin`, then presents the generic video-card
error before gameplay. The dialog localizes the failure only to the graphics
startup path (`0x44E080` calling `0x44E460`); it does not reveal whether
DirectDraw creation, DirectDraw2 QI/capability checks, required 640x480 mode
enumeration, or exclusive-mode setup failed. A sibling 408x149 screenshot is
kept outside the repository. Pre/post artifact/data hashes match, and the
temporary links and mount were removed. The prior `-no3dfx` launch argument is
not recognized by the recovered command-line parser and should not be treated
as a valid bypass test. A 32-bit API trace is required to identify the failing
DirectDraw boundary before another workaround is selected.

### Direct-spawn Frida trace

The follow-up private capture
`../../analysis/reference_captures/original_ddraw_frida_20261025T022949Z.json`
attached Frida to `race.bin` as an IA-32 process and installed hooks on both
the raw DirectDraw exports and the `apphelp.dll`-forwarded export addresses
before resuming it. The process remained responsive with the window title
`MGI`, but neither export was called and no graphics dialog appeared. This
direct-spawn path therefore stalled before the observed DirectDraw boundary;
it did not produce an HRESULT and is not equivalent to launching through the
original launcher. The launcher handoff is still untraced. Cleanup was
verified: the process, temporary `Data` junction, staging directory, and ISO
mount were removed, and hashes of the original launcher, engine, and ISO
remained unchanged.

### Launcher child-creation traces

The initial private capture
`../../analysis/reference_captures/launcher_handoff_frida_20261025T023744Z.json`
instrumented the original launcher before resume. `CreateProcessA` succeeded
with command line `Data\race.bin -1` and creation flags zero, but the child had
already exited before the hook could recover `PROCESS_INFORMATION`.

The follow-up private capture
`../../analysis/reference_captures/launcher_suspended_ddraw_trace_20261025T024913Z.json`
patched only the launcher's `CreateProcessA` flags from zero to
`CREATE_SUSPENDED`, recovered child PID/TID 36376/22800, installed Frida hooks
on raw and apphelp-forwarded DirectDraw exports, and then resumed the child.
The real launcher command remained `Data\race.bin -1`. DirectDrawCreate,
QueryInterface for `IID_IDirectDraw2`, GetCaps, EnumDisplayModes,
SetCooperativeLevel(flags `0x11`), and SetDisplayMode(640x480x16) all returned
success; the enumeration callback accepted the 640x480 mode. Both reported
driver and HEL capability flags had bit 0 set. Nevertheless, the child exited
with the same generic video-driver dialog before gameplay. This moves the
failure boundary past those successful API calls, but the dialog alone did
not prove that the engine's own required-mode gate passed. A screenshot is
kept outside the repository. Process, temporary data junction/staging
directory, Frida session, and ISO mount were cleaned up, and launcher, engine,
and ISO hashes remained unchanged. All 31 GameData SHA-256 values also match
the prior baseline.

The next private capture
`../../analysis/reference_captures/launcher_surface_trace_20261025T025327Z.json`
installed hooks before resuming the suspended launcher-created child. The
same DirectDraw calls again returned success, the enumeration callback
reported return value 1 for 640x480, and SetDisplayMode succeeded at
640x480x16. No CreateSurface or surface-vtable call occurred before the child
exited with code 5 and the generic dialog appeared. This capture established
that no surface setup occurs before termination, but did not identify the
termination path.

The callback-level private capture
`../../analysis/reference_captures/callback_flags_20261025T025906Z.json`
hooked the engine callback at `0x44EE00`. It ran 23 times; the first
640x480x16 callback returned 1 and changed byte `0x509C52` from zero to one.
The 640x480 flag remained set after enumeration and the `0x44EEB0` wrapper,
yet the child still exited with code 5. The exit-origin capture
`../../analysis/reference_captures/exit_origin_20261025T030208Z.json` hooked
termination APIs and observed the engine call `ExitProcess(5)` at `0x44EDAD`
(the preceding `push 5` is at `0x44EDAB`; return address `0x44EDB3`);
`0x44E080` had returned success immediately before. Static analysis places
that call in `0x44EC00`, which queries available video memory and maps the
reported amount into a supported memory class at `0x509C2C`.

The private capture
`../../analysis/reference_captures/vidmem_report_20261025T030612Z.json`
records three successful `GetAvailableVidMem` calls through vtable slot 23.
The first call, with caps `0x4000`, returned total `0xFFFF0000` and free
`0xFFEC4000`. The wrapper passed HRESULT success. `0x44EC00` then adds
`0x96000` to the total in 32-bit arithmetic; the value wraps to `0x00086000`,
below the 2 MB cutoff, so the class at `0x509C2C` becomes zero and its default
switch arm calls `ExitProcess(5)`. This establishes the failure as an
unsupported video-memory-class result produced by the driver's near-4 GB
DWORD report and the engine's 32-bit addition, before surface creation.
Later calls reported totals/free values `0xC0000000/0xBFED4000` for caps
`0x10001000` and `0x3FFF0000/0x3FFF0000` for caps `0x20001000`.

A child-only override probes
`../../analysis/reference_captures/vidmem_override_20261025T031500Z.json`
and `../../analysis/reference_captures/vidmem_override_retry_20261025T031150Z.json`
did not reach the memory-query hook. In both, the child instrumentation
detached before resume and the retained handle reported `0xC0000409`. The
later successful direct-Frida run showed the scripts had confused the fixed
module virtual address `0x44EC00` with an RVA and attempted invalid address
`0x84EC00`; the correct RVA is `0x4EC00`. The failed attempts do not establish
a REA provider limitation.

The successful private capture
`../../analysis/reference_captures/vidmem_python_success_20261025T031822Z.json`
used Python 3.12 and Frida 17.24.3 with hooks confirmed on the suspended child
before resume. It changed only the first successful query's total output from
`0xFFFF0000` to `0x00200000`; the free output and later queries were left
alone. The classifier returned class 4 and stored 4 at `0x509C2C`. Both
DirectDraw `CreateSurface` calls returned S_OK, including the 640x480 surface
with descriptor flags `0x47`. A follow-up run with the same bounded override
hooked DirectDraw surface `Blt`, `Flip`, `Lock`, and `Unlock` calls. It observed
12 successful blits, 24 successful locks, 24 unlocks (23 S_OK and one
`0x887601C2`), and no Flip calls during the 30-second observation. The retained
640x480 screenshot
`../../analysis/reference_captures/original_menu_render_20261025.png`
(SHA-256 `DD27A87189FFD5673D1BFB87F4E97EEA76770CF2469062B5398A1AC7DB0667F2`)
shows the rendered Viper Racing title/menu screen. Its event trace is
`../../analysis/reference_captures/original_menu_render_20261025_events.json`.
This confirms main-menu rendering after the process-only memory override; it
does not confirm an in-race frame or general-purpose compatibility. Cleanup is
verified and hashes of the launcher, engine, ISO, and all 31 GameData files
match their baselines.
