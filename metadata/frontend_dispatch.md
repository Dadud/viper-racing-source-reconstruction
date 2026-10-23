# Frontend selector dispatch

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA.

## Verified behavior

`0x401E10` calls the main-menu interaction routine at `0x4805C0`, then
dispatches its returned integer:

| Selector | Observed target / behavior |
|---:|---|
| 0 | `0x40A410` |
| 1 | `0x40A570` |
| 2 | `0x4AA460` |
| 3 | `0x47B4E0` |
| 4 | `0x406950` (called with a null argument) |
| 7 | `0x41C870` |
| 8 | Return from the state-dispatch routine |
| Other | Calls `0x411000` and returns its result with the low byte cleared |

`0x411000` is a shared panic reporter, reconstructed in
`src/engine_panic_reconstruction.c`. It prefixes the variadic message with
`Panic : ` in a 255-byte local buffer with an adjacent gate byte, then uses
unbounded `_vsprintf`. If the gate byte remains zero, it sends the message
through the module logger when both logger globals are nonzero. Otherwise it
calls the observed no-op at `0x4111C0` four times when `0x4D1810` is zero,
or shows a `Last-Resort Log` warning box when that flag is nonzero. It then
sets `0x4D2978` and attempts a write through address zero. The store and
faulting instruction are observed; process-level handling of that fault is
still unknown.

`0x4805C0` constructs menu text entries named `SingleRace`, `Career`,
`Multiplayer`, `Replay`, `Options`, and `Exit`; it also creates player-name
controls and uses `t_main.stp` and `player.stp`. Its final interaction helper
is `0x472B30`.

The selection-value path is verified. `0x472B30` returns the value from
`0x479DA0`; that routine returns global `DAT_005608DC`. The generic setter
`0x479AA0(value)` stores its argument in that global and signals completion.
Control callbacks `0x4751A0`, `0x475250`, `0x478460`, and `0x478500` call the
setter with the activated control's configured DWORD at object offset
`+0x23C` (or `+0x230` for the alternate control layout). In the descriptor
loader `0x472130`, type-2 records pass their second DWORD to the control
constructor `0x475B40`, which stores it at `+0x23C`.

The `0x4805C0` descriptors and their labels establish these selector values:

| Visible entry | Descriptor value | Result returned to `0x401E10` | Handler |
|---|---:|---:|---|
| Single Race | 0 | 0 | `0x40A410` |
| Multiplayer | 1 | 1 | `0x40A570` |
| Career | 2 | 2 | `0x4AA460` |
| Options | 3 | 3 | `0x47B4E0` |
| Replay | 4 | 4 | `0x406950` |
| Exit | -1 | 8 | Return from state dispatch |

The Exit result is normalized in `0x4805C0`: values greater than `-3` and less
than zero return `8`. Handler behavior independently supports the labels:
`0x40A230`, used by the `0x40A570` loop, references `MULTI`; `0x4AC520`, called
by `0x4AA460`, references `Career:Create`, `Career:Load`, and `Career:Delete`;
`0x47B4E0` references the options UI; and `0x406950` references replay buttons.
The type-2 menu descriptors in `0x4805C0` carry IDs `0`, `1`, `2`, `4`, `3`,
and `-1`; none carries selector `7`. The player-name text control is a type-9
descriptor and its constructor does not install a selection result ID. The
selector-7 branch targets `0x41C870`, whose entire body returns zero, and its
only verified caller is the `0x401E10` dispatch switch. Selector 7 therefore
has no verified visible main-menu source and may be a vestigial or externally
fed route; that last interpretation is an inference, not a recovered fact.
`MainMenuItem`, `main_menu_control_value`, and `normalize_main_menu_result`
model the visible selection path.

### Replay screen (`0x406950`)

`plan_replay_screen` projects the replay handler's record-presence branches,
initial resource calls, six raw `0x408F50` control-constructor argument blocks,
the `0x335`-DWORD copy to `0x4EB828`, and repeated
`0x472B30` interaction. Enter (`0x0D`) calls `0x408160` only when a replay
record argument is present. `0x408160` builds the replay-analysis page and
returns zero only when its nested menu returns `0x42`; otherwise it returns one
and the outer menu continues. Without a record, Enter simply repeats the menu.
A finite observation that does not reach an exit is marked incomplete. On
exit, the plan records the post-action `0x426560` call for an
existing replay and the observed teardown/resource order. The six descriptors
are preserved at EBP offsets `-0x2A8`, `-0x270`, `-0x238`, `-0x200`, `-0x158`,
and `-0xB0`; runtime values from `0x4EC554` and `0x4EC804` remain observations.
The model also projects five one-time button registrations from mask
`0x4EC704` and three cache-driven `0x418CC0` refreshes when their observed
conditions are supplied. Resource-manager effects, replay file parsing, and
callback internals remain outside this portable flow model. Shipped-image
evidence for `0x406950` is
`ev_f78ea88766e340634ae8e120f2a324caff257e1184cf11c4d474944136cb6bab`.
The nested analysis screen writes `0x40A00000` to `0x4EB810`, `0x3F800000`
to `0x4EB80C`, and 2 to `0x4EC50C`. All 25 `0x408F50` descriptors retain
their EBP-relative locations and 14 raw arguments; the eight DWORD values
read from globals remain supplied observations. Two model-resource lookups,
the menu call, and teardown are projected. One-time localized-label
registration is now projected from mask `0x4EC788`, with all eight object,
string-key, atexit, and flag-bit records preserved. The eight `0x418CC0` cache
refresh checks are also ordered around their corresponding control constructors;
their pointer-comparison outcomes remain observations. The nested menu result
determines the callback return byte. Evidence for `0x408160`:
`ev_d950a3a9d0ef73510a5640de9d23356bdaa5168a35503818982b830b04f30df2`.

The Load button's `0x407760` callback is projected by `plan_replay_load`. It
preserves the modal guard, optional file-dialog route, `0x411430` file/parser
open, fixed `0xCD4`-byte record read, `0x45C590` validation, and
`0x426790` replay-start branches. It also records the existing-replay cleanup,
invalid-file dialogs, start-failure teardown, handle close, and renderer
restore. Dialog, parser/read, validation, and replay-start results remain
observations; string-resource refreshes and the file/parser internals are not
emulated. Shipped-image evidence: `0x407760`,
`ev_e82713137b5dcc99f51a5506e98764583d2bfcff55a87bcd891019721e74f29a`.

The handler at `0x40A410` zeroes a `0xCD4`-byte setup record, then calls
`0x40A180` to read setup data. `0x40A180` calls `0x40A850` to populate
participant rows and `0x40A5D0` to run three follow-on helpers in order:
`0x40A7B0`, `0x40A710`, and `0x40A620`. The outer handler then loops over
the `0x40EED0` screen, calls `0x401EA0` for outcomes 1 and 2, and calls
`0x40A220` between option passes.

`0x480C80` writes the selected car string to global `0x4EC9C8` and the
selected track string to the caller's temporary buffer when the setup screen
returns `-2`. When the track selection is the literal `random`, the helper
chooses an indexed track using the random source and track-count helper.
`0x40A180` copies the resolved track string to record `+0x08` and passes
the selected car string to `0x40A850`.

`0x40A850` sets participant count at record `+0xCA8` from the configured
`car_count` value. It searches the driver-order values at `0x503D40` for
`car_count - 1`; the matching index receives the human row. The configured
`opponent_strength` value at `+0xCCC` is copied to global `0x4F0B30`.
Rows begin at `+0x28` and advance by `0xC8` bytes. The helper writes a
zero marker for the human row and one for AI rows, a name at row `+0x04`,
a car string at row `+0x11`, a DWORD at row `+0xC0`, and a byte at `+0xC4`.
The human car string comes from its setup input; its name is read from
`GLOBAL/player_name`; its `+0xC0` DWORD is `-1 - GLOBAL/player_paintjob`,
and its `+0xC4` byte is one only when globals `0x4D10FC` and `0x4ECCC0`
are both nonzero. AI rows use the literal car stem `viper`, get their display
name from the selected driver object's string at `+0x210`, store
`driver_order[i] % 7` at `+0xC0`, and zero `+0xC4`. The player-name INI
lookup copies at most 12 characters when the key exists; the missing-key
fallback copies at most 63 characters into the same row field. The copy helper
zero-fills the 13-byte or 64-byte destination. The raw fields are modeled by
value; their wider engine semantics remain open.
Evidence: row builder `ev_e54fa22bb11fb18061986c36b87bdd42131208f2bb6dcd1aa84d850bef29cdb0`;
name helper `ev_f2d8bc04d351fa0e2a8164739a333819f94e99fb0c89309b48b13e94e5065eed`;
missing-key copy `ev_9d0227840abca22637fd2424b9e8443693a4a04701b8a2462d85690511827773`;
bounded-copy helper `ev_62d5fb44d570e3c6520a0bd58bc770e6a87d42beaffdccea6198964ddec2bfd8`.

The human initializer `0x45EFD0` calls `0x45EE40` to load its setup record.
That loader constructs a `<0x4EF0C0>\setups\<param_3>.csu` path, opens it via
`0x411430`, and accepts only a `0xD4`-byte file. It reads `0xD4` bytes, then
warns if the DWORD at record offset `0x8C` is not 1; the read result and version
warning do not change its success return. Open failure or wrong size calls
`0x45F150` to fill the record from fallback setup data; the wrong-size path
closes the file handle, while open failure has no handle to close. The wrapper
then copies only the first `0x8C` bytes into the active vehicle record.
The fallback tries caller parameter 3 plus `.ccs`, parameter 2 plus `.ccs`,
then `default.ccs` through `0x45EDA0`; each resource must have type `0x43435330`,
version 2, and `0x8C` payload bytes. Failed lookups reset the 35-DWORD numeric
record through `0x45F2F0`. If all three fail, `0x45F2A0` writes the `Default
Setup` label and applies the numeric defaults. `plan_human_car_setup_load`
models this fallback order and the exact default field writes as well as the
file-open/size/read/version gates.

Evidence: wrapper `ev_e3b85ae60acf0d3d7550e6c5838c6a1497049437921aa89f5b10e114f9527b54`;
file loader `ev_e2f3f12bea397632c5f8bf556279da9fd6b1ebe334a2cde6437d33ed319453e2`;
fallback `ev_4cd5733e86260555c90cdaceae5a424f4a334b99e1704780172e210d6f863eef`;
resource fallback helper `ev_707cef981fcf8329d53ce81847d2c9749014f40b391b1c8bebd83800e0d7b101`;
numeric defaults `ev_25777c5b2ef1e675ef2da98ccc808c85fe3824367840f034716bd9de7a89bb99`;
generic default label `ev_f044b9de761c0c3596239bb830a124f270f3ceaad184e7fb597bb53c75d2e516`;
path helper `ev_37798836f8b70a1bc03d4b0f7e1fe22c4d6928357c7274bc392fd8faa3f0e3f4`;
path bytes `ev_e25fab28245945bde2b0776149bb60ee145fbc3c6cfad8af5d47d8b98f3d92f6` and `.ccs`/`default.ccs` bytes `ev_e5ec961f3315166cd227aaf969d9352bc20b54431b19008c460acf5b757688b8`.

`0x40A620` confirms marker 0 as the human-car initialization branch and
marker 1 as the AI branch. Human initialization calls `0x45EFD0` only when
global `0x4E322C` is zero; AI initialization calls `0x41A000` with the
entrant index and whole setup record. Marker 2 is skipped; marker 3 and other
values call the panic helper. `0x40A710` loads a difficulty resource selected
from the eight-entry table returned by `0x40A9D0`, then loads each entrant's
car resource through `0x40A6D0`. `0x40A7B0` shows the loading screen, unloads
`ui.res`, loads `race.res`, then loads the selected `.trk` resource.

`build_single_race_entrants` projects the row writes, including the missing
driver-order lookup's panic/continuation gate. The follow-on resource and
initialization calls are resolved, but the lower-level per-car effects and
the outcomes of `0x401EA0` remain unresolved.

`plan_single_race_setup_post_read` records `0x40A5D0`'s setup call order,
resource operations, and per-row initializer branch. Evidence for this path
is retained in `single_race_entrant_plan.evidence.json`, including dossiers
for `0x40A180`
(`ev_8ceec55de83e322f4e21469c854462e3fe60c75a89b7049b9ca1f85048550ac6`),
`0x40A850`
(`ev_e54fa22bb11fb18061986c36b87bdd42131208f2bb6dcd1aa84d850bef29cdb0`),
`0x40A620`
(`ev_975a3cf2e208f3080f652baa79972d58265218201781eed5a8f8690b445e8630`),
`0x40A710`
(`ev_b2b92c295f08f2c2a37f4efc4dfb5f38faf6b2be772bfe0cc6afc3e73d770309`),
`0x40A7B0`
(`ev_c1e68aaa8aeddfafc57ef681fb7cb8b59ffda682e805929aef2f0b9e6def98cb`),
`0x41C9B0`
(`ev_9c3398b9c86673bc939ca99eac671ca6e1dcbb4e18f437343ca37bb6188fedcd`),
`0x41C9F0`
(`ev_e31f0c0cf8cfb67febfa0fe1eba7700606346f6aba2123d4d1518d4e083de3b9`),
and `0x41A000`
(`ev_3632db95593d0d492ae6f6f7bca9e8a845fbb9ea7e98b60a97572300d7fab3ca`).

The wrapper-level disassembly adds these observations. `0x45EFD0` allocates a
temporary buffer, calls `0x45EE40`, and copies 35 DWORDs (`0x8C` bytes) from
the helper result into its destination. `0x45EE40` builds a resource name
from the base path and supplied car name, reads it, and only takes the copy
branch when the reported resource size is exactly `0xD4` bytes. `0x41A000`
first tests the supplied AI record against the
descriptor at `0x4D3E88`; on failure it calls `0x45F2A0` and copies the
resulting `0x8C` bytes. It then resolves the selected AI configuration through
`0x41CA40`, looks up the wheel/configuration table through `0x406640`, calls
`0x41A0C0` for vehicle initialization, and writes/scales vehicle fields
`+0x04`, `+0x4C`, and `+0x48` in that order. The lookup contents, floating
point operands, and lower-level initialization effects are not inferred from
the wrapper alone. `plan_vehicle_initializer_wrapper` records this operation
order without fabricating those values.

The nested `0x41A0C0` path builds and loads a per-car configuration resource,
queries its record, and unloads the resource after either branch. A missing
record writes seven observed binary32 defaults (`0x402A3D71`, `0x3FE3D70A`,
`0x3FA66666`, `0x3F800000`, `0x3F3D70A4`, `0x3F000000`, `0`). A found record
uses a count-controlled coefficient loop; count 5 selects a different source
table. Global `0x4E9754` selects x87 `FDIV` or the compatibility path: the
base ratio calls `0x4BC437`, and each coefficient uses `__adj_fdiv_m32` at
`0x4BC8FC`. The coefficients depend on the
loaded record and caller stack values. The parser at `0x45E2F0` extracts the
resource chunk selected by tag constant `0x43415246`, accepts record version
6, compares the reported size with `0x228` through the empty helper
`0x45E470`, and then copies `0x8A` DWORDs (`0x228` bytes) into the destination
regardless of the comparison result. The `0x41A0C0` caller passes zero for
the parser's optional upgrade-set argument, so the upgrade overlay branch is
skipped here. In the shared nonzero-argument path, `0x45ED10` loads the
`0x55504753` upgrade chunk and calls the panic helper if its version is not 1;
the observed code then continues through pointer fixup and returns the set.
`0x45E480` applies each selected set's counted `(record_offset, value)` DWORD
pairs to the copied car record. The single-race call does not take that path.
Ghidra names the relevant locals by negative stack
displacement; relative to the `local_228` copy destination, the coefficient
count is record `+0x108`, and the formula inputs are record `+0x1B4`,
`+0x1B0`, `+0x1AC`, `+0xCC`, `+0xC8`, and `+0x4C`. These record offsets are
derived by subtracting each observed stack displacement from the destination's
`-0x228` displacement. The remaining two operands come from `param_1[0]` and
`param_3` at `0x41A0C0`.
The decompiler recovers the successful
branch's numerator as
`(local_74 * 0.0254 + local_78 * local_7c * 1.0000001e-5) /
((local_15c - local_160) * param_1[0] + local_160)`, then multiplies by
`local_1dc * 0.1295748`. Each coefficient divides that stored scale by
`table[i] * param_3`, except index zero uses `39.0`. The model consumes the
`CarFilePayload` returned by `parse_car_file_payload`, reads these fields from
its raw DWORD array, embeds the two observed coefficient tables, and calculates
binary32 outputs. The count-5 table at `0x4D3DE0` is
`[0.25, 0.4, 0.7, 0.85, 1.0]`; the other table at `0x4C7508` is
`[0.21, 0.33, 0.5, 0.66, 0.83, 1.0]` (stored with their original binary32
rounding).
The model routes both divisions through the existing common compatibility
approximation when enabled; exact helper operation IDs and x87 rounding are
not reproduced, so calculated values remain approximate.
`plan_ai_vehicle_configuration`
projects the branch, formula, loop count, defaults, resource order, and
division mode. The function dossiers and bytes are in
`single_race_ai_config.evidence.json`: `0x41A0C0`
(`ev_bc1be482fb7c70974185642fd02b11b9d9e53618f36272203c00cac8168f05e7`),
`0x45E2F0` (`ev_2f6428108dd7e2e6f2fd24d85fb0d633948cb45410819c44729265d7cd275b78`),
`0x45ED10` (`ev_bd53fdb90138dd00a7d2a870d4f301fdc59196586f8b37dd18da1cbe48aaed23`),
`0x45E480` (`ev_882e49398a89796f5d7820c15549c050bf0c0f92e1ca46804d58dd780406efac`),
and table bytes `0x4D3DE0` (`ev_724eb955b06f65773847b9aa98ba3e600b62f8e2c2150cc9dc8027cd0641594f`)
and `0x4C7508` (`ev_2f839587818299d434df7ab55df2f5db1722f6ac4902e9d2156471327d9f9602`).

`0x480C80` copies the selected car output before resolving the selected
track. A case-insensitive six-character prefix match for `random` calls
`0x413BD0` for a platform timer value, obtains the track collection count
through `0x406590`, computes `timer_value % (track_count - 2)`, and obtains
the chosen track through `0x4065A0`. The last two collection slots are
excluded by the observed subtraction; the underlying timer backend and track
collection contents remain observations. A zero divisor or signed
`INT_MIN / -1` case faults before the track output is copied, while the car
output has already been written.

Whenever the selection branch reaches settings writeback, `0x480C80` writes
the observed settings in this order: `field`, `realism`, `damage_on`,
`is_reversed`, `time_of_day`, `weather`, `race_type`,
`opponent_strength`, and `car_count`. The DWORD values use `0x46AF50`;
the two boolean keys use byte writes through `0x46AFE0`. If settings DWORD
`+0x18` is nonzero, the DWORD at `+0x1C` is cleared before writeback.

`plan_single_race_setup_screen` records selection output reachability,
random-track modulo/fault behavior, the field mutation, and ordered typed
settings writes. Evidence is in `single_race_setup_screen.evidence.json`;
function dossiers cover `0x480C80`
(`ev_7fd891cc4150f75b085d33381022c38a825b646c2c09c810a8e944da9d707944`),
`0x413BD0`
(`ev_aa2fb78acdcfbfdd3069b1fb8b3c187845daafad5432f537b5ced5e850553858`),
`0x406590`
(`ev_a5fb697c2e44998f7e9953e1e7f65d3d1621f805b810ad89026b775f2a030257`),
`0x4065A0`
(`ev_99aa6b6da7dddae608cc43f7967efe511056f478fbe3d53169d9418b8b79628f`),
`0x418FA0`
(`ev_399dc5c78add45da6df764d34b962d1b760ce581a8fc768679cdec53036068ed`),
`0x418FB0`
(`ev_07c2749cb8028bb3c96f2287cfd4005acd8973b43d28d24c93bea2808e57c01f`),
`0x46AF50`
(`ev_e388d95c39dede7fbe220ad0b03a9cac0a0a0ba3113cef3cb8a0c5e5b55fa42d`),
and `0x46AFE0`
(`ev_909a26f639f5d0527f9a70afc3c279b5f98586cec2f4182d238e9c150d9ddbae`).

`0x40EED0` constructs a pre-race interface with observed labels for Starting
Grid, Track Info, Garage, Race, Qualify, Best Times, Race Info, and Damage,
along with race options for laps, race type, realism, and reversed tracks. It
loads `postrace.res`, `grid.stp`, `trackmap.stp`, and `main_t.stp`. Its return
classification is directly visible: a final UI result of `-2` returns `1`,
`0x29A` returns `2`, and all other values return `0`. These classifications are
represented in `classify_pre_race_screen_result`; their user-facing semantics
remain unverified.

The caller `0x40A410` handles those returned values as follows:

| `0x40EED0` return | Observed caller behavior in `0x40A410` |
|---:|---|
| 0 | Leave the inner interaction loop, refresh through `0x40A220`, and read pre-race data again with `0x40A180` |
| 1 | Call `0x401EA0` with the current pre-race data |
| 2 | Copy the selected entrant/configuration data into the active entry, then call `0x401EA0` |
| Other | Call fallback handler `0x411000` |

`pre_race_route_for_screen_result` records this caller-side branch. The
selected-entry copy uses records with a 200-byte stride in the decompilation;
field names and the user action that produces outcome 2 are not established.
The routine at `0x401EA0` is a large initialization/update sequence and its
subsystems are still being classified, so this route model does not imply that
the playable race loop has been reconstructed.

## Multiplayer state loop at `0x40A570`

The multiplayer state handler calls `0x45C5B0` to zero `0x335` dwords
(`0xCD4` bytes), then writes record type 3 at offset 0 and the record size at
offset 4. It polls `0x40A230`; while that routine returns nonzero, it calls
`0x401EA0(record, nullptr, nullptr)`, calls the post-step wrapper `0x40A400`,
and polls `0x40A230` again. `0x40A400` delegates to `0x40A600`, which calls
`0x40A760` then `0x40A800`. `plan_multiplayer_frontend` preserves the loop
skeleton and accepts poll results as observations; connection and lobby effects
remain open.

The `0x40A600` post-step resource sequence is modeled by
`plan_multiplayer_post_step`. `0x40A760` unloads one `<entrant>.car` resource
for each record at `+0x39` with a 200-byte stride, then unloads a difficulty
resource selected from `easy.res`, `medium.res`, `hard.res`, and `career1.res`
through `career5.res` using the index at `+0xCCC`. `0x40A800` formats
`<track>.trk` from the bytes at `+0x08`, unloads it and `race.res`, then loads
`ui.res`. Car/difficulty/track/race unloads use `0x4178C0`; that helper logs an
absent set, decrements a shared reference count when it exceeds one, or frees
the set when the count reaches one. `ui.res` uses `0x417740`; a failed load
reaches the fatal error handler. The portable plan preserves operation order
and reports missing participant strings or an out-of-range difficulty index
as incomplete observations; resource-manager outcomes remain external inputs.
`plan_multiplayer_frontend_detailed` composes one post-step observation with
each active poll iteration in order. It retains a null slot and marks the
composition incomplete when an active iteration has no corresponding inputs;
the next unobserved poll remains represented by the base loop plan.

The first `0x40A230` poll with no connection object calls `0x49AEA0`. Its
`plan_multiplayer_telephony_initialization` projection preserves the 260
`CreateEventA` attempts, the early return on the first failed event, and the
subsequent TAPI `lineInitialize`/`lineNegotiateAPIVersion` gates. The callback
is `0x49AFB0`; both negotiated API bounds are `0x00010004`. A successful
`lineInitialize` sets `0x5632CC` before negotiating each reported device. A
negotiation failure resets that flag and logs the version error; a line-init
failure logs its own error and returns the existing flag. The caller returns
inactive when this initialization reports false. API results and device count
are observations; the preparation helper `0x49B000` is called first, but its
full synchronization/thread effects are not part of this projection.

Evidence: `0x49AEA0` (`ev_c7dc222bd8177dd4b9b5448aa9734d77a8a67a39e6d9951c93742f0f1ad29ad2`),
`0x49B000` (`ev_014a51d60f374725f06e45a15c1437ab0664c2aaec65a761fbd700f2712d2054`),
`0x489470` (`ev_8751a433f3e34b59b1d7d87651a7dc8938e6a545ca148cf1363bdc4e2ed2481e`),
`0x488B10` (`ev_61952abebcf16eef17c3eb3245c0072ca0fd5b4f13d75a216b41824024468f46`),
`0x49C620` (`ev_2b3f3dd2b3498d71673d0fec2582c92f7d8aaf2844a0af9ae1889e73b2e8d83b`),
and `0x485940` (`ev_f8f90f79a0825a43547cdc62100074c02d8c4fa7a73cf00a3d63162e3d4f1897`).

After telephony initialization succeeds, `0x40A230` requests a `0x3C`-byte
connection object from `0x49C620`. Its constructor `0x49C670` registers
`LiveMultiInfo`, publishes the object in `0x56434C`, starts a worker named
`Multiplayer` with callback `0x49BD40`, and marks its thread-table entry enabled
through `0x414D90`. The constructor copies the thread-table ID into object
fields `+0x30` and `+0x34`. `0x49C950` accepts the object exactly when `+0x30`
is nonzero. A zero ID routes through `0x49C6D0`, which clears the object fields,
stops any nonzero worker, releases `LiveMultiInfo`, clears `0x56434C`, and then
`0x49C620` frees the allocation. `plan_multiplayer_connection_object` models
the allocation and worker-ID gates; actual thread creation and safe-module
registration remain external observations.

Evidence: `0x49C620` (`ev_2b3f3dd2b3498d71673d0fec2582c92f7d8aaf2844a0af9ae1889e73b2e8d83b`),
`0x49C670` (`ev_80fe7b75fe60b53d71f154595a5f38a9fde86ce12b061611efc7475588b31ffa`),
`0x49C950` (`ev_3a17c0c8c4fcc574ff62829fba4f439711bb0f6a3c42b5ceaa2f5cbbeb26bb4f`),
`0x49C6D0` (`ev_9fcdf3d75cbd8a0221d2f60326c2a13f72caff3258e423c1a9751284d997a343`),
`0x414A70` (`ev_84976f529528a776c5c5be88519f3be823c41389f2063ef8338da73617bc3052`),
`0x414D90` (`ev_ce407fdd50c9a19aec577d15ae1b86d1c19ca32d3b72474cf5d8bda185beb44f`),
and `0x4150C0` (`ev_0b14f12122d4b002313b409ba3e5744723b355b465bbee9767a4740b8eee92fb`).

`plan_multiplayer_communication_setup` models the next `0x485940` branch.
When the incoming status byte is nonzero, the binary calls the empty helper
`0x485E30`, then displays the multiplayer screen through `0x485A10`. With a
zero status byte it creates a `SessionMgr::data_pool` session through
`0x4A1100`, then unconditionally calls `0x4A1000` to copy the configured
address into a 16-byte session field. Raw mode byte `+0x04` selects line-open
arguments `(1, 1, 0)` when zero and `(9, 1, 1)` when nonzero. The zero route
uses `0x485E70`, which selects transport helper `0x4A3CD0` or `0x4A3D20` from
raw byte `+0x05`. The nonzero route uses `0x485E40`, a virtual `+0x30` query,
and `0x4A3D70` to derive the line transport object. The status byte is then set
from whether connection field `+0x08` is non-null; only an active connection
reaches `0x485A10`. A false setup/UI result makes `0x40A230` run connection
destruction `0x49C6D0`, free the object, clear the connection global, and enter
TAPI cleanup `0x49B080`; the loop then retries the connection setup. If
`0x4A1100` returns null, the following
`0x4A1000` dereferences its `this` pointer without a visible guard, so the
portable plan marks that case as a possible terminal invalid access rather
than inventing a graceful failure result. Transport and UI outcomes remain
observations. `plan_multiplayer_communication_setup` preserves the distinct
direct/line helper order and reports the caller cleanup, global clear, and
retry gate without claiming the later `0x40A230` object-state poll succeeds.

Evidence: `0x485940` (`ev_f8f90f79a0825a43547cdc62100074c02d8c4fa7a73cf00a3d63162e3d4f1897`),
`0x4A1100` (`ev_0d56d43cfb5f3320aeb3e33f17fb38e5ff10e85a0cf0d3fc31e16b98748d2ff0`),
`0x4A1000` (`ev_f0e31b400e54a1bfb8e8ff1100f81253179e05f093552355ebf161a06bc764c7`),
`0x4A3D70` (`ev_c8a351202797203cfe1b3c2b2076587a36f5d7e9ac958725d528c1779a024005`),
`0x485E70` (`ev_645f1dcfcad9d1ba15fce052a9676687c283309cfde1e4981622403657e879d4`),
`0x485E40` (`ev_e82b897d46b8f46500ba250e1cd1a0aaf122b75c278d83a518dd35544c2a100a`),
`0x485E30` (`ev_1af46f15d2cf77dbbaddff8d83913c232a72abd80812304a3664b0639a7da5bd`),
and `0x485A10` (`ev_b00a762cb05e1b15dcf5c2a4521b6da20345ad2f1362d98cdc5e6277880b5b5e`).

The retry cleanup helper `0x49B080` first calls `0x49AD40`. That pass sets
the serial-I/O counter to `0x10`, decrements it for every active primary slot,
and calls `WaitForSingleObject(completionEvent, 0)` for those slots. A timeout
(`0x102`) leaves the primary slot active; other observed results clear it after
the success/abandoned/failure/panic handling. `0x49B080` then calls
`lineShutdown`, clears the TAPI app handle and initialized flag, and walks 260
slots: still-active primary values produce a pending-data log and `CloseHandle`,
while every non-null completion-event handle is closed. The portable
`plan_multiplayer_telephony_shutdown` takes the slot values and wait results as
observations and projects the active counter, warning/error cases, and close
counts; OS wait and handle behavior are not executed.

Evidence: caller `0x40A230` (`ev_ec39ba29e173b6c2e594f410a91adbae739d869c363353e5580e2dc085cc60ad`),
`0x49AD40` (`ev_155b8134075b88ec660855bc7a15597e6dc97aeb942fec8c6fff80cad6412528`),
`0x49B080` (`ev_d5d1113e390b01af25932e612b20ee174c10be220e2fe32c4da637dd7e73899a`),
and panic path `0x411000`/`0x411020` (`ev_33e3e536e1f2727e9ac6c24e63969e9720ddc0d6590e3ad31c2ce3978cc79c58`, `ev_aaf9730aacfda36e266417948c8961a94c16ed928594b9ab352fa6090a6db793`).

Evidence: `0x40A400` (`ev_09affeb2565a142cd78eb02afaf1705ccb8bde92b8b0dc1297d0e720c6a6bab6`),
`0x40A600` (`ev_ae61fc92fd9c796f2f21f823793bd77cd6a926d83f06108385415e4c636a150d`),
`0x40A760` (`ev_83a4cad992172ec8fc23e2442cbc7ad218736c91ed462622f561ad8e2e2130ec`),
`0x40A800` (`ev_0bab31399e7f6964d14b81b9e130386d9a8cce5758522f9cef44e5847e7274f9`),
`0x40A9A0` (`ev_1d7082e3195a97021f8b06ad699c824c5b2f381dbd4726bf17c79450ea50e029`),
`0x40A9D0` (`ev_c5b35548f4b77672216bc0a463960a8e1ac4de2df6f81ff4360b6162ef4e4321`),
`0x4178C0` (`ev_4d212af486e3ef67edce91ae905998a52dc0fd1572f769a460c1e27da5c5d767`),
and `0x417740` (`ev_33f06f0c2ab2a0355eacac31501a31573e46e7ef343e762e807edae09049075e`).

Evidence: `0x40A570` (`ev_060649b0e778d336fe09f48e6f78071b79ed1c1c0f694b9506a7fba4a0e40719`),
`0x45C5B0` (`ev_15531112733067b60f69b3a835ef59251ac48c32a64e37b182e2d5c0e21a7293`),
`0x40A230` (`ev_4bf8391a8bc9fdb269103e434a8b6d8ccdaa2ab399384e6a345c4a20cb32d151`),
`0x40A400` (`ev_995f5d4f56d868ded0140f7a0b82f71fa3c6415fc9b10f4945592c163fa918cd`),
and `0x40A600` (`ev_5d92c78fad91d09f9b23be4259baf7a436ea889f53cef0646a540ee7aedd600f`).

## Career state loop at `0x4AA460`

The career handler calls `0x40D0E0`, then repeatedly calls menu routine
`0x4AC520`. A negative menu result exits the loop; every nonnegative result is
passed to `0x4AA490` before another menu poll. It finishes through `0x40D100`.
The entry helper saves `0x4D10FC` to `0x4D10F8` and clears `0x4D10FC`; the exit
helper restores `0x4D10FC` from the saved global. `0x4AC520` contains the
Career:Create, Career:Load, and Career:Delete controls. For each nonnegative
menu selection, `0x4AA490` clears two `0x19D`-DWORD buffers, calls the career
file loader `0x4ABE20`, and enters its action UI only when loading succeeds.
The successful-load path copies the loaded buffer, performs setup calls, polls
the event/action helpers, and then releases the event buffer and UI state. The
progression event and ranking effects in that UI loop remain unresolved.
The separate slot chooser's Create, Load, and Delete actions are traced below.

`0x4AB4A0` formats the save path as `%scareer\\career%d.dat` using the
configuration directory returned by `0x412D50` and the selected slot. The
loader `0x4ABE20` opens that path read-only, requests a 16-byte header, and
requires words `0x43494E46`, `2`, and `0x674` in its first three DWORDs. It
then reads exactly `0x674` payload bytes, XORs every byte with `0xA4`, sums the
decoded bytes, and compares that sum with header DWORD 3. A read failure reaches
the panic helper; a failed header check logs a corrupt-file error, and a
checksum mismatch logs its own error. Opened handles are closed on the loader's
completed branches. `career_slot_path` and `load_career_file` implement this
observed path and validation behavior; they do not recreate the UI or file
manager.

The first career screen built by `0x4AB4F0` exposes Events, Ranking, Testing,
Upgrades, Save, and Back controls on `career.stp`. It sets the output flag when
the interaction helper returns `0x21`; `0x4AA490` routes that flag to
`0x4AEBE0`. The ordinary route repeatedly shows the Career Summary Season/Week
screen (`calendar.stp`) through `0x4ADE10`; a successful `-2` interaction result
reaches the career progression routine `0x4AA5E0`. That routine contains a
substantial simulation/results loop, but its internal event and ranking data
effects are not yet modeled.

Within that loop, `0x4AA410(index)` selects an eight-DWORD row at
`0x564E78 + index * 0x80`. It writes the supplied integer to the first zero
slot; when all eight slots are nonzero, it shifts the row left by one and
stores the value in slot 7. The portable `CareerRecentResults` projection keeps
those eight raw values without assigning numeric meanings.

For each completed event, `0x4AA5E0` also consumes eight numeric entries from
`0x564A18` and eight 12-byte result rows. It finds the position whose entry is
7, reads that result row's dword at `+4`, and, for placements 1 through 8,
adds the payout-table dword at `+0x14 + placement*4` to the funds global. For
each position it also maps the result through the corresponding numeric entry,
adds `points_by_placement[placement-1]` to that standings row's dword `+4`, and
stores the raw placement in its event column beginning at dword `+2`.
`apply_career_event_ranking` models those payout and table effects from observed
order, result, point, payout, and event-column inputs. The binary scans the
complete order array and retains the last position containing competitor 7;
duplicate valid indices therefore remain observable in the projection. The
portable model rejects an out-of-range index and a missing competitor 7, since
the corresponding binary accesses would leave the modeled arrays.
`make_career_event_ranking_observation_from_race_setup` composes the season
event with the eight result rows projected from `0x401EA0`. It refuses to
compose incomplete or non-eight-row results. The event column is the current
event cursor: caller `0x4AA490` pushes `0x5649F0` into `0x4AA5E0`, and that value
is written into each standings row. At entry, `0x4AA5E0` advances this cursor
only when both upstream predicates succeed and `cursor + 1 < season_event_count`;
it also increments the event-loop counter and refreshes the aggregate ranking.
`advance_career_event_from_input` models this guarded branch. The pre-race
estimate loop is projected by
`estimate_career_entrant_order`: it combines seven AI profile-time values with
the race-setup baseline and one observed random sample per entry, uses the
player race time for competitor 7, falls back to 300 seconds for nonpositive AI
times, and sorts estimated seconds ascending. The random-generator state and
selected profile fields remain runtime inputs. Evidence: `0x4AA5E0`
(`ev_d306de34923170ef0eb22248da3f0dcdf2e921f857330c74a56e38c66a8e3a3`),
`0x4AB0B0` (`ev_e8d526d4774e8b206df3b2a18a4a7f0a584473331505a83a1654e2265c2c0c1e`),
and random helper `0x4193F0` (`ev_ac4ff3b289c237c75c0e1fd826f40009860951ca0f75f62d7779fda1dab6ee56`). Cursor argument and branch evidence: caller `0x4AA490` (`ev_e1ccadbdca1e319b8454239e64b0a02c992e984de76c32f3aff8cb4149e493de`) and `0x4AA5E0` (`ev_2dd12de899fae8f45fcd758bdfefe7e4aefaa963ec97ad3fb5db51df3954e048`).

After those writes, `0x4AC020` sorts the eight row indices using the accumulated
score at row dword `+1` and writes each sorted rank into the row's first dword.
`rank_career_progression_rows` models this as a separate operation. The helper
uses the Visual Studio 1998 shared sort routine, which takes its shortsort path
for eight entries. Shortsort selects the first strictly greater comparator
result, then unconditionally swaps it with the current endpoint. Equal scores
are therefore unstable; eight equal rows end in index order `1, 2, 3, 4, 5, 6,
7, 0`. The comparator orders lower accumulated scores first. Evidence:
`0x4AC020` (`ev_17a47a86e087ed27516acc2885a3a995a89d10eb6e3da2db39eebb50e5a41dc5`),
sort helper `0x4BD250` (`ev_d6315a72a692f03329cc3f4c8dd0c339bc4e65dcf482a03b20d1c920c5c9b896`),
and matched `_shortsort` (`ev_6ed515ada46f50c0ab91c06bb826ee4499c20159a7b0984c2df766e0981c032e`).

The completion suffix of `0x4AA5E0` is modeled by `finish_career_stage`, with
the upstream completion signal and reward supplied as observations. Without a
completion signal it clears the advancement flag and makes no other state
change. When more events remain it reports the post-event results screen and
leaves progression state untouched. At stage
completion it appends result code 1, 2, 3, or 4 for raw stage selectors 0, 1, 2,
or other values; adds the supplied reward to the raw funds dword; clears the
`0x110`-DWORD table; and resets the event cursor and loop counter. For selector
0, if the history row index is below 3, it advances the row, records marker 5
in that new row, reaches the row-loading helper, seeds the first dword of each
of the eight `0x88`-byte rows with its index, and sets the observed advancement
flag (the action resets that flag to zero on entry). This preserves numeric
effects without naming the progression phases.

The career Save callback `0x4ABB90` calls `0x4ABF10` with the selected slot and
the decoded `0x674`-byte career buffer. The writer creates the `career`
directory, attempts to open the slot path for output, builds the same four-word
header used by the loader, computes the byte sum before encoding, XORs a copy of
the payload with `0xA4`, writes the header and payload, and closes the file. An
open failure logs “Can't create career file”. `make_career_file_save_image`
generates the exact `0x684` bytes supplied to the write calls; it intentionally
does not model directory creation or file-system outcomes.

The Create button callback begins at `0x4ACC50` (Ghidra does not recognize it
as a procedure boundary). Its `NewCareer` form contains PlayerName, Realism,
Damage, and Create controls. When the UI returns `-2` and the name is nonempty,
the callback clears the selected `0x674`-byte slot payload, copies the
NUL-terminated name at offset 0, stores raw settings at offsets `+0x10` and
`+0x14`, sets byte `+0x1C` to 1, seeds eight dwords at `+0x170 + n*0x88` with
`n` for `0..7`, and writes the new slot through `0x4AB3C0`/`0x4ABF10`.
`make_new_career_payload` models those initialized bytes; setting meanings are
not assigned.

The Delete button callback at `0x4ACC30` opens the `AbandonCareerDialog`. On
confirmation, `0x4AB3E0` passes the selected slot path to the file-delete helper
`0x411850`; the callback then refreshes the chooser. The Load control returns
the selected slot when the chooser interaction returns `-2`; `0x4AA490` then
uses the validated load path described above.

`0x4AEBE0` repeatedly obtains a testing selection from `0x4AEDB0`, then calls
`0x4AEC30` with the selected track name and a byte-sized option. The selector
populates eight track choices. On a successful menu return it copies the
selected track string to its first output and returns the current Reversed
option byte through its second output; a non-selection returns the menu's
non-success status. `0x4AEC30` builds a race context from the active career
buffer, including dwords at offsets `0x10` and `0x124` and a byte at `0x14`,
then repeatedly calls `0x40EED0`. Menu result 1 invokes `0x401EA0` with mode 0;
result 2 invokes it with mode 1; any other result exits the loop. The selected
track and Reversed byte are supplied to that race setup path.

`plan_career_testing_race` models that runner's exact loop and context reads.
It reads DWORD `+0x10`, byte `+0x14`, and DWORD `+0x124` from the active
career payload; the last value also feeds the observed `+3` and `+4` context
values. Their higher-level meanings are unresolved.
It records the selected track's NUL-inclusive copy size against the 36-byte
local, preserves the Reversed byte, routes each menu result to the observed
race setup mode, and includes context cleanup plus season reload only after a
non-1/non-2 result. An unterminated result sequence remains incomplete.
Evidence: `0x4AEC30`
(`ev_b9898af2025c9ce90e8c35090b58dd4b32173ab1800a17f32c500f4d6c2a8e`).

The runner then calls the context cleanup helper `0x40A600` and reloads career
season data with `0x4AC1A0(DAT_005649EC)`. `0x4AC1C0` formats the resource name
`season<index>.ssn`, loads its `SEAS` payload through `0x4ADB00`, resolves the
career class label through `0x4AB0F0`, and copies that label to a global buffer.
The loader checks eight header dwords through `0x4ADBB0`; for each declared
record, it also resolves eight dwords beginning at record offset `0x0F` in a
`0x16` dword record stride. The loader/parser call and most event-field
meanings are now projected; DWORDs 2–5, the live race-result inputs, and the
career-record update remain open.

`plan_career_season_parse` projects the `0x4ADB00` label pass from a bounded
DWORD view: event count at word 0, eight header money values at words 1–8, and
eight per-event payout values at `0x0F + event * 0x16`. Each value is formatted
as `Career:Money:<value>` and looked up through `0x418C70`; a missing label
reaches
the panic reporter at `0x411000`, after which the binary continues the loop if
that helper returns. Short or unavailable resource/lookup input is reported as
incomplete. The lookup projection itself does not validate the resource file
format; the separate decoder preserves the observed record layout.
`decode_career_season_data` separately preserves complete raw event records
with the exact 9-word header/22-word stride confirmed by all four shipped
season chunks. The first eight bytes are the event track name; event DWORDs
6–13 are placement payouts and DWORDs 14–21 are placement points, matching the
reads in `0x4AC290` and `0x4AA5E0`. `make_career_event_ranking_observation`
now composes these static payout/point tables with runtime entrant order and
result rows. `0x4AA5E0` also copies the record's low byte at offset `0x10` and
DWORD at `0x14` into its race context; the decoder exposes both raw values
without assigning semantics. DWORDs 2–3 remain uninterpreted, as do runtime
race-result inputs. Observed sizes and payout rows are catalogued in
`metadata/career_season_samples.md`. Evidence: `0x4AA5E0`
(`ev_3a3ac7ccfd379e1ae77b3aca9288354dd126a9ae4e079333599a2cde88a0e810`) and
`0x4AC290` (`ev_60278f919e948e0ba8d746117e43129695455b5d3d3fbdfdebd83f263cd895c9`).

Evidence: `0x4ADB00` (`ev_d1f41041f67f84283636a2cbc6679a2c31b97a19836bbf137e229b0e6077a98b`),
`0x4ADBB0` (`ev_316ee342e579586f29c2fd293c25c92642baa4e110baa8ea1ed5ff10ab595196`),
and lookup helper `0x418C70`/`0x418C00`
(`ev_8942fa8133ef359df42b664e77922f3b6263fa8d74ddf32aa7395bec3865cff8`,
`ev_4f485bab5cd2ef42e7f256fa825739e17a1019b71614aa878aa83edd2e5a0c51`).

Evidence: `0x4AA460` (`ev_c8e744047a0339e5f0550ac4260cd79a7020e1b0691973b06ff527c9fbae523c`),
`0x4AC520` (`ev_8d79df967773ff8e13ba0347f1a74f71e0a6146a2da2deea17126b22281c357f`),
`0x40D0E0` (`ev_46e0d8dd62ed3fbf9c11a23c6b3a5c3f9e5ceadf319c3edef47be1434124692f`),
and `0x40D100` (`ev_76d5a864f6717557213a2aa5bc25419201f3a2ced35b595e1a50d22e0dbaeb9e`).
Action-handler evidence: `0x4AA490` (`ev_94db32f1bc5277f325b28c87f52875b257090e4186c0f10a3774431e231e0e5a`),
`0x4ABE20` (`ev_03eef3b1e0967583c2fb4bfaf0c67471ea57678ea924ef007bcfeb1e86bb9c0a`),
career screen `0x4AB4F0` (`ev_70641dcaf176485b1690404adeb034eb291e7602a1b996ab211b9d331944c9b0`),
career Save callback `0x4ABB90` (`ev_0b992d4614bf1a2bb64a4d5bd5b9f7af6733edd96e2c69b348a5f43c88181e25`),
career file writer `0x4ABF10` (`ev_76416ca3903760f5e28b0cd75800185dd036b541bdd98b5b02cc758ac0321966`),
new-career strings (`ev_105bb2ccbc350d7998d5a06a3779b4e85d6d9c86170dbc9e610acd1be80e6734`,
`ev_6498d2d49c309b2ff43e178d478d922207ff0575d0ae5f782e5555d9a9f90d93`),
new-career form callback instruction at `0x4AD312` (`ev_54e32407d7953f4a5db89b51e3b1161e00572d987e438126c01d3e4b71f3fc0a`),
payload initialization at `0x4AD32E` (`ev_2be44a5fdd9252632045c9af90a32c47de478d3ca331366db504c57e7c7c43dc`),
name copy at `0x4AD34A` (`ev_598e1e3dd7cf3fadc1be4149bc118d664b598d1b5493c93f770d38aa6d737e36`),
raw field writes at `0x4AD353` (`ev_80c20f78601383d8d620313bcfb6fef16b15ebca299cd7dbceacba50d78768d4`),
`0x4AD35B` (`ev_9f95c5191c4d284433e0c436f5b3e7ed91c608a5381bd28e6b328a3fc03c729d`),
`0x4AD362` (`ev_32c737ac6ee500b28cd852b3752304584c23101e2afd136d38d344fef6f7516b`),
seed loop store `0x4AD36B` (`ev_f1be7d72014174a3dea17dcc496c6da4b18e520f803e2af16d1d3226a4d0f25f`),
save call `0x4AD37F` (`ev_553ad7344a22164d19f271e319e50ea50a7ea0fb8aec61101d0a6fe9259464aa`),
delete callback call `0x4ACC36` (`ev_9b97ccc285dba8300b82243aa421279a9573645116313043b970d63d18c87a60`),
and chooser refresh call `0x4ACC3E` (`ev_17c108b2690a493b96cc9e1e5659465039cbedc1b5293cd83b1acfc01043c2c5`),
Create file-write wrapper `0x4AB3C0` (`ev_4613ffd9bf66fb3f1f79b53de23f763bf0c5107d65f023f95e53398cfcf1919a`),
abandon confirmation `0x4AB3E0` (`ev_2b887d899cbba0395f57e5b4b16652ccbfe30fa3f1face6e612486e3f47e7719`),
and `DeleteFileA` wrapper `0x411850` (`ev_43c79c831f8016d6ea0370116649a2501f993d7113683f8d359c0ccf7ff26373`),
summary screen `0x4ADE10` (`ev_8f4a07e6298ed3ec9f9cd6cd3c15a4c40c46639f22d8a8a87df0bafafd0a290e`),
progression routine `0x4AA5E0` (`ev_f28b4512e61dc50b98c258a7412e780fbadf0ba8f925d282f62fba44433a69bc`),
result-history helper `0x4AA410` (`ev_1182d699e444510e87443c388d39163f153ac31b4fa35b368a4881073df156b3`),
career rank-sort helper `0x4AC020` (`ev_17a47a86e087ed27516acc2885a3a995a89d10eb6e3da2db39eebb50e5a41dc5`),
sort helper `0x4BD250` (`ev_d6315a72a692f03329cc3f4c8dd0c339bc4e65dcf482a03b20d1c920c5c9b896`),
matched `_shortsort` (`ev_6ed515ada46f50c0ab91c06bb826ee4499c20159a7b0984c2df766e0981c032e`),
player-placement payout helper `0x4AC290` (`ev_20eb1989323a0b95f3b044cd0118130f4abef38eb859e1b9e1dfb0dc738423af`),
rank comparator score read `0x4AC090` (`ev_1f67794e43cda45f996e452b6b0167ef90a2d56158df57cec452ceb075fa73fd`),
comparison `0x4AC0B3` (`ev_b1a96bf9f08adaea4f75aefb4570821b45622324267ecd859ff033447aff8ae8`),
and difference `0x4AC0B7` (`ev_cac139245631adc9d813bb73efd3fff353bcdd06c62a2dedc8d6487f06b59fef`),
testing wrapper `0x4AEBE0` (`ev_c248fdbe20bdfb6753d5e28f3958417d74fbe01127c3d9f63f4c81f975a5e968`),
testing selector `0x4AEDB0` (`ev_827d652534612d7fbd095042a7e9188796bff9d2e0fba373b879a0fd779d7215`),
and track runner `0x4AEC30` (`ev_e4d9048a0a04230d5f1cb93aaa852c229fa1b2fa624fdb18c4c04b955f0963b1`),
season file loader `0x4ADB00` (`ev_3531a5c426bc80507cf3318621a2767c67e7ad68f7054214d4b69924080a8312`),
and season selection/reload `0x4AC1C0` (`ev_73e49062bae58ab034e88ce95e800c85e4a6af34f5dc4df033727bef7fdff869`),
XOR decoder `0x4AC270` (`ev_f2db3ce8908ef736e58c82121dab5e76b7752c04a4a9f026651b5132143d9afa`),
checksum helper `0x4AC240` (`ev_9cf44d2806fbf19314f61b438819ab11d4ca3ad2f8a420d9187c1d41caad4238`),
path formatter `0x4AB4A0` (`ev_ff0e8787f8dd9a1f6c8107609a24d6c15395e85e2c4508014d0507f072c42072`),
and config-path accessor `0x412D50` (`ev_6b233c6939b82a4577e7ba1f90137539fc2f7e87f26c3c1681c40593b10e1ddc`).

## In-race loop and escape menu

`0x401EA0` enters `0x4020E0` when the mode flag returned by `0x41B020` is zero;
otherwise it calls the separate path at `0x41AB00`. The mode accessor at
`0x41B020` returns byte `DAT_004D3DA8`; observed callers include `0x401EA0`,
`0x4289B0`, `0x429440`, and `0x429E90`. The caller list is supported by
`ev_7dbc4089b5c6cb3b65db3867725ece9bb47df954ab13944df57ae9d91bb24416`; the
global reference observation is `ev_1bbd62d1de3eb7ff9cf7dbe9cd04d9fee1e6de1e3f23213452431c31a088ba68`.
The former repeatedly
polls `0x40A160`, processes update/render-related calls, and invokes
`0x4022F0` when its loop counter expires. This is evidence of a repeated
in-race loop, but does not identify all work performed by each helper or
establish the simulation tick rate.

The outer `0x401EA0` call sequence is represented by `trace_race_setup` in
`include/viper/race_setup.hpp` and `src/race_setup.cpp`. It records the optional
`CONTROL/race_camera` lookup, initialization calls through `0x425530`, the
mode-selected loop entry, cleanup calls through `0x413810`, the second mode
check and resulting default or caller-supplied completion callback, optional
result-copy block, and finalization calls. The two `0x41B020` results are
separate inputs because the binary queries the mode before and after cleanup.
The trace preserves outer call order and branch points; callees' subsystem
effects remain unmodeled. When `param_2` is present, `0x401EA0` zeroes 49
DWORDs, stores the result count in word 0, then writes one three-DWORD row per
result. Row words 1 and 2 copy source offsets `+0x0C` and `+0x04`. Row word 0
is copied from `0x45DFB0(index) + 0x50` only when source `+0x0C` is positive;
otherwise it remains zero. `project_race_setup_results` models these populated
row values without assigning field names. `0x4243C0(index)` locates the source
record by scanning `DAT_005086A0` records at `DAT_0050869C` with a `0x48`-byte
stride and matching the selector byte at `+0x41`; the selected record supplies
the `+0x0C` and `+0x04` words. `find_race_setup_entry` models that keyed search.
`0x45DFB0(index)` returns a separate `0x170`-byte-stride record from
`DAT_0053B1E8` (`0x5C` DWORDs); the associated result word is at `+0x50`. The helper also
calls `0x45DFA0` twice for nonnegative indices (it returns
`DAT_004DC718`) and the no-op `0x45DF90` before returning the record.
The Ghidra evidence IDs are `ev_f4a6f0492821a466d56a330a119d15e90e885058b667220f4aece533f78ecd2e`
(`0x401EA0`), `ev_01f93b3b5dbb61bb664de8689d48bf1b4aec13aedc3efb495260e1596a97726a`
(`0x4243C0`), `ev_cb07271a162f3e082fba45cdbd660a21b95265614694da6ecba4d4b2a80db3ed`
(`0x45DFB0`), `ev_c627c0f54ed9b618b89ea7c4ca96b5252f8b92f6221f25c89a6d2b3478b5ea6a`
(`0x45DFA0`), and `ev_870f30afa7ebfbb3af2afa3834ed5c17c8c66f003893b64daaa375bd1831a542`
(`0x45DF90`).

`project_race_setup_result_buffer` models the complete result-buffer shape:
zero-fill 49 DWORDs, store the signed result count in word 0, and write three
DWORDs per positive-count row. It preserves the byte-cast selector passed to
`0x4243C0`, the conditional `0x45DFB0` lookup when source `+0x0C` is positive,
and reports when the unbounded loop writes beyond the 49-DWORD zeroed region.
Missing lookup outputs mark the plan incomplete and record the possible null
dereference instead of inventing row contents. This call-site expansion is
supported by the `0x401EA0` dossier
`ev_4775651abe34ed567a78a4030c64dbdba73fb905c070b04426e91ce688cd65b3`.

`plan_race_setup` composes these observations. Unless `DAT_004CF5B0 == 4`, it
starts the local `race_camera` setting at 5, applies the observed INI read, maps
value 12 to 4, and records the resulting writes to `DAT_004CF5B4` and
`DAT_004EB184` only on that branch. If the lookup is skipped, the existing
`DAT_004CF5B4` value is passed to race initialization. It keeps the pre-loop
and post-cleanup `0x41B020` bytes independent, then composes the selected loop,
caller/default callback, optional
result buffer, and final `0x46AF50` setting write. Mutations to the saved mode
by callees remain an explicit final-value observation.

The 16-entry table is zeroed by `0x45DE40` over `0x5C0` DWORDs and its
exclusive count is maintained by `0x45DEB0`. Each entry stores the raw mode at
`+0x00`, the enabled byte at `+0x04`, C strings at `+0x05` and `+0x12`, and a
raw DWORD at `+0x134`. The setter calls the `0x49C5C0` predicate for mode 2.
That gate checks the global interface and the nested object's signed `+0x04`
state (`>4`), then calls its vtable slot `+0x84`. For the observed `Race's
ChatLine` vtable, that slot targets `0x4A1B70`, which returns the byte at
object offset `0x1C8 + index*0xD9`; `scene_chatline_virtual_row_flag` reads
that byte from caller-supplied bounded object memory. Its state gate can derive
the nested `+0x04` value from the `0x49FA00` lookup: `0x4A1E80` stores 6 for a
result other than `-1`, otherwise 3. The callback also requests a status label
from `0x4A1680`; its one-time string registration is not projected, and
`0x4A1AF0` is observed as a no-op. The setter then uses unbounded C-string
copies. The portable model truncates at the
first NUL and reports when either copy overlaps the second string, the raw
DWORD, or the `0x170`-byte entry boundary; it does not emulate adjacent-memory
corruption. Evidence: `ev_ae806d03b0a1787ee7f706f00fef2dc1140ca44244c66dd576cdbea7101167fe`
(`0x45DE40`), `ev_2beccbc86e1da7445e8477ecd45541401f2f536d6bc279cba0810b511dc38dde`
(`0x45DEB0`), `ev_1e0adab7ec5ca037bf5bb90731365283909c0564e9373f4923873f4175da5657`
(`0x45DFB0`), and `ev_7b99fc992fca931becf14bcca16c7f43e988cdac207df999ad1374f723917b2e`
(`0x45DF90`). Full Evidence is in `metadata/car_string_table.evidence.json`.

`0x4022F0` initializes visible escape-menu labels `EscapeMenu:Resume`,
`EscapeMenu:Restart`, `EscapeMenu:EndRace`, and
`EscapeMenuMulti:Disconnect`. Its event switch maps `0x170` through `0x17A` to
internal state values 0 through 10. Event `0x17B` reaches the branch tail but
does not store a value; event `0x37B` sets value 11. The displayed labels and
numeric outcomes have not yet been joined by verified evidence. The portable
helper `escape_menu_selection_for_event` now preserves the exact event/value
split. Evidence: `ev_5cb8a4dd5e1e48794c0eac1b0fae15d2acc17dabc82fc34eb963a4bc7dd3faf6`
(`0x4022F0` function dossier).

The menu bootstrap is also modeled by `plan_escape_menu_initialization`. It
tracks the eight one-time bits in `DAT_004EB21C`, emits the four label
registration/`atexit` pairs, retains the saved pre-menu page at
`DAT_004EB184` when the current page is not 12, and follows the four button
group guards and their conditional `0x418CC0` localization refreshes. It also
projects all 13 DWORDs written in each button descriptor at `0x4CF640`,
`0x4CF678`, `0x4CF6B0`, and `0x4CF6E8`; localized text handles remain caller
inputs. The current
outer-loop model includes this bootstrap plan only when it reaches the
`0x4022F0` timeout-menu call. Its detailed dossier is included in
`race_loop_outer_iteration.evidence.json`.

`plan_escape_menu_event` now models the event gate sequence: input availability,
the `0x40C2F0` render-cancel route, the state-indexed callback, the global
`0x4018F0` key handler, then the built-in event switch. The handler consumes
only low-word events `0x50` and `0x70`. It applies the known page writes,
key-`0x31` callback-state toggle and `0x4035D0` startup-byte effect, the
`0x121`/`0x122`/`0x124` player operations, and the Escape routes for
single-player and multiplayer modes. Function-pointer callback results, the
selected player's virtual callback, active-player count, and platform-mode
results are inputs because their runtime outcomes are not available from this
static callsite dossier. Direct analysis of `0x402960` and its callees shows
the key-`0x31` setter calls `0x4035D0` and resource refresh `0x402060`; these do
not set `0x4EB20C`. Escape action `1` through `0x402A40` sets the exit latch.
The outer loop includes this plan only on its timeout-menu branch. Helper
evidence is preserved in `escape_menu_callback_helpers.evidence.json`.

The alternate path at `0x41AB00` prepares a render context, draws a text and lap
list using the current entry and lap records, pumps messages, and polls input.
When `0x40A160` returns 3 it calls `0x4257D0` and increments a local counter;
the pre-increment counter is tested against 30, so the 32nd observation exits
before another render or input poll. Other mode values wait 500 ms and do not
reset that counter. If a render context is acquired, the callback draws a
frame; message pumping and input polling continue even when acquisition fails.
An available key event `0x1B` exits; event `0x31` also sets `DAT_00507C70` to 1.
The loop then reads the optional `test_prompt` setting and may display a
separate prompt that waits for another key. The portable
`advance_alternate_race_loop` models the first loop's wait, render availability,
message/input polling, counter, key-exit effects, and the conditional control
state update called after input handling. It does not assign a screen name to
this path.

When its render-context lookup succeeds, `0x41AB00` draws six centered text
rows using the current-entry record at `0x45DFB0(0)`: offsets `+5`, `+0x12`,
the constant at `0x4D3F24`, and a formatted value from entry offset `+8` plus
the reversed-track label selected by the byte at `+0xCD1`; the remaining rows
include the `is taking notes in the` string and a time/text helper result.
The exact user-visible meaning of those raw entry fields is not established.
The lap count comes from virtual slot `+0x48` on `DAT_00508A04`. It starts at
lap index 1 when the count is at most 20, otherwise at `count - 20`, and queries
indices strictly below the count. A null result from `0x424450(0, lap)` ends
the list early. `0x424450` scans `DAT_005086AC` records with a `0x48`-byte
stride and count `DAT_005086B0`, matching selector byte `+0x41` against zero
and lap-index byte `+0x43` against the requested index. Each available row
formats `Lap %2d: %s` using the record's DWORD at `+4` through `0x425A40`, then
draws at x=400, starting y=100 and advancing by the same row spacing as the
centered text. The formatter converts supplied time values into a
minute/second/separator/fraction string. Its helper first truncates total seconds
toward zero, then truncates the fractional centiseconds; it uses
`%01d:%02d%c%02d` for hundredths and `%01d:%02d%c%01d` for tenths. At 60
minutes or above it emits `99:99%c99` or `99:99%c9`, respectively. The portable
`format_scene_dashboard_time` models these conversions and strings; x87 invalid
conversion and extended-precision edge behavior are approximated for portable
floating-point types.
Thus the list requests at most the last 20 indices for counts above 20, while
a smaller count starts at 1; this describes displayed indices, not a proven
one-based lap-number convention. `alternate_visible_lap_indices` captures
that window and early termination from missing records. `plan_alternate_race_text`
projects the six centered calls and visible lap rows, including line spacing,
source fields, lookup/format helpers, and raw color-global sources;
`advance_alternate_race_frame` emits those calls only after the loop obtains a
render context. Color values remain runtime observations.
Evidence: `ev_e2da7f0df45bbfcf84b7a666b6a9a83151f32b53a216f1de92e27604c0af1e0f`
(`0x41AB00`), `ev_38f45932105352585443ade677f75e3981c62f8cbdcc6ce27e0ff01d8be4ee0b`
(`0x424450`), and `ev_a2a4cf1295f65e27ae3ca9638140757484973671e2cf078420bba611a47cef5b`
(`0x425A40`); `ev_dd5f2ae4d1351c7a65df6332bae0de6f5b3698d8bdc266df30984b66198b5720`
records the fallback format literal at `0x4D4E9C`.
Ghidra evidence: `ev_e2da7f0df45bbfcf84b7a666b6a9a83151f32b53a216f1de92e27604c0af1e0f`
(`0x41AB00`) and `ev_bbd5cf4176570d11658f69c1b9dd4042fdce32056d765b07918b204bb1a57eba`
(`0x41B020`).

After the loop, `0x46AF90` reads `test_prompt` from the settings group at
`0x4D3E08`. If its first byte is nonzero, the callback calls `0x413CE0`, then
retries `0x447C90` until a render context is acquired, pumping messages with
`0x412C80` between failed attempts. It draws three lines at `(320,430)`,
`(320,440)`, and `(320,450)`; the middle line is the observed
`**            Press ESC            **` string. It then calls `0x447CE0` and
`0x447C50`, waits through `0x413D50`/`0x412C80` until an event is available,
consumes one event through `0x413D20`, and calls `0x41F4C0`. That last helper
loops until three successful render-context acquisitions and draws the string
at `DAT_00507C5C` each time. `advance_alternate_test_prompt` models the optional
branch's retries, message pumps, prompt draw point, input consumption, and the
final helper call. `plan_alternate_test_prompt_text` records its three fixed
text calls, while `advance_alternate_three_frame_helper` models one attempt in
`0x41F4C0`'s tight loop and stops after three successful context acquisitions.
Failed attempts repeat without message pumping. The resumable
`advance_alternate_test_prompt_workflow` connects the prompt gate, fixed text
operations, input wait, and three-frame helper so each step consumes only the
observed result for its current stage. Evidence for `0x41F4C0`:
`ev_e58629ca1f2baa1264f04706137ef3037e60a8a407099f5a7effce7a165b623e`.
Evidence IDs: `ev_f0a8e7b2ae351dd2dafe46078ede8a68cd8c6df289c20105f05b8545643dcd04`
(`0x46AF90`), `ev_594928a5a0b327c25c89455dbadad33636ef9e0cb21a045d5f8ff9cb9b7697c0`
(`0x413CE0`), `ev_16c4b5e7de993beae38684f6613537a60efff3196a18607000748850cfd7cdd1`
(`0x447C90`), `ev_cf35c82b92bd2934da9d31e11ed090d161beaa76e1186fd42af331c41521b238`
(`0x412C80`), `ev_058e0c3f7e5b572691a88eb49238bb3e1814ddd143b456fdc3d26fd0268ac1af`
(`0x413D50`), `ev_8ce0630c03f67735c0558cd7436184506b177624d06c9d97a713615c08992bff`
(`0x413D20`), `ev_a1be018b8e20b54615ddb099860ac5ac51b52a72ff7bc8a84874e1c07d31047e`
(`0x447CE0`), `ev_d2989fb01467437e9afae98241cb5ce472be2eeea3513d4bfd775c15fd7bacaa`
(`0x447C50`), and `ev_8e0105869fef4c77c1114aeae94a303aeb4707ccd0f40c9d95480492c843f83a`
(`0x41F4C0`).

The polled state at `0x40A160` returns `DAT_004D0C54`; its setter at `0x40A170`
stores the supplied value. The recovered `0x43C2E0` helper clears four fields
at `+0x54`, `+0x64`, `+0x5C`, and `+0x60`, then sets that state to 1.
Incoming-reference evidence enumerates seven direct setter calls: `0x4260F2`
and `0x43CE62` pass 3; `0x43C2F0`, `0x43C3E0`, and `0x43CF26` pass 1; and
`0x43CB55` and `0x43CFB9` pass 0. Six call sites are in addresses Ghidra
does not assign to a procedure, but the direct call targets and source
addresses are decoded.

At `0x43CE62`, the value 3 is stored only on the state-1 path when
the initial `0x426E50` gate is false, `0x49BE40` is false, `0x45C230` returns
a nonnegative index, the latch at object `+0x7D8` is set, and the stored time
at `+0x7D4` is strictly earlier than current time. It then scans the records
counted by `0x45DFA0`. Records
whose object byte at `+0x6A` is zero and whose corresponding byte at
`0x005094C0 + index` is nonzero invoke virtual `+0x40(index)`; any zero return
prevents the state-3 store. The zero-record case reaches the store because no
callback rejects it. `should_switch_to_alternate_mode_three` models this
numeric gate without naming the state semantically. A second state-3 call is
at `0x4260F2`. The call is in the small callback at `0x4260F0`, referenced at
`+8` in six vtables. Factory `0x426000` installs two of those tables
(`0x4C7810` and `0x4C7880`) for numeric object types 1 and 3. This identifies
the setter as a shared virtual callback for those table entries, while the
object types' gameplay meaning remains unknown.

The first eight vtable entries read from the shipped image show that types 1
and 3 share these methods apart from their deleting destructor: `+8` is
`0x4260F0` (store mode 3), `+0x0C` is `0x43BEE0` (true only when object fields
`+0x44`, `+0x48`, and `+0x4C` are all nonzero), and `+0x14` is `0x43C2E0`
(clear `+0x54`, `+0x5C`, `+0x60`, `+0x64`, then store mode 1). Factory
`0x426000` invokes virtual `+0x14` after constructing the object. The portable
helpers `alternate_mode_object_is_ready`, `reset_alternate_mode_object`, and
`store_alternate_mode_three` capture these numeric effects. Vtable byte evidence:
`ev_83908097b486d9a7d61ae8b8667d24de157e2e59890c62b549addc3045a1044e`
(`0x4C7810`), `ev_73fa6970075e2acaabad48e7bda65995b2e3e49485cb8ed0a3f701381b05beb5`
(`0x4C7880`), and `ev_2b5d1097e10c597fa911959cea8832d4fc90daf1d5a108167731b95dc8c346ae`
(`0x426000`).

The same two factory-installed vtables point slot `+0x38` at the shared method
beginning `0x43BF00` (the disassembler does not define a procedure there).
`update_alternate_mode_interface` reconstructs its transformed position at
`+0x38`, the 1000-unit adjustment steps around the other object's `+0x64`
target, and the four-state message/timer progression at interface `+0x64`.
The state-specific `0x43A880` X/Z region predicate is reconstructed from its
four bounds when supplied. The slot `+0x64` method returns the constant 60.0;
slot `+0x68` returns true only when the vehicle velocity squared is strictly
greater than the constant at `0x4C83A0` (raw bits `0x4431C71C`). These gates
now derive from constants and the velocity input. Display-helper effects and
the state-3 result helper's native mutations remain observations. The reached
message path now applies its `+0x29=1,+0x2B=0,+0x2A=0` target stores when the
resource lookup returns an active object, then appends its address token under
the `0x46E4F0` critical section to the 32-entry queue. A full queue reports
the `0x411000` panic route. Resource lookup results and the external queue
state are caller inputs. Evidence:
`0x43A880`
`ev_06979066b49eac1dc764d60124407aa440f208537c112e84a09fd1554cda86c6`, the
vtable slot bytes `ev_8e755e86668f3d97a72fd13661058b87601f155d9504987996c8d420a1852f75`,
`ev_7146801c6fc5f9a3af5ec456158d8d80466687b9ec7ae64de8ec62dbaf9beee2`,
`ev_6be9b94863db024e947cf2fc0956f3ec1e58a03d52b37235fdb9c7785e067c68`,
`ev_cee01cfe2dc3a4fc9104d5631b93772f1c7acbee9cc8192162115e5e55fb790f0`,
and constant bytes `ev_21d09103933c2439df1f93f888a5c3cf3a41d19ace5b4f6f49bca4e36d7e6a81`,
`ev_bb64f06eacc95ed921f4a224e430ea4e0d23b7cda9b0a24698833abf2699af5d0`;
display path evidence: `0x46BFE0` `ev_24f37a961525a35d4675ac58a31284a5f9540e00294afdfae6e305daa0cba65e`,
`0x46C0D0` `ev_df2f6279277fd284aa039694c3bc521e14b550f164bbf749d55a173af6689713`,
and `0x46E4F0` `ev_9b4531dbb5371cdeda301bda772de57a3d6d8dcd6f9d4f44a68228280fc97c8f`;
invalid state values route to the observed `0x411000` panic helper.
`ev_6eaa26f360d1ef80114e74300fa6f1df68ec8b46da35e99d0006eff692b2544c`
(`0x43BF00..0x43C1BA`).

Evidence IDs:
`ev_abb821a0cca18f299a81c61910ca62e040f8492fd2761df9bdf8929cda52732c`
(`0x40A160`), `ev_f0ff7fa7550b4451671191a39a355d1fec071bdd54e0e60b2bfa9bfdf6751898`
(`0x40A170`), `ev_b6a35335212fbcde7eb1056861ca2763e33aee51c1270943ed67f118cd0140e5`
(`0x43C2E0`), and `ev_fb473e818228f2b9bb1f0aa8bbfafac540796516ebafe164c924f8a9e1aa8996`
(`0x40A170` caller inventory; partial classification), `ev_35d61d73b08ca645d5a5e5f763b1e04a212a2e1b8c8ecd4a27baa86ded397888`
(`0x40A170` incoming reference list), `ev_3446404dd1a13a46ee7b6bc5060de31da2b797debcee9d1c6aaa6f8fb46c2cdc`
(`0x43CE62` call instruction), `ev_ec8b979e991286612dba8c582a8b140070e3dcd7b5621b44a94652622abf5dbb`
(`0x4260F2` call instruction), `ev_b77d78c7d680283e4e63aa42a86f8d927d8dec78ff0471318a56afce8c96dce3`
(`0x4260F0` references from vtable slots), and
`ev_2b5d1097e10c597fa911959cea8832d4fc90daf1d5a108167731b95dc8c346ae`
(`0x426000` object-type/vtable factory).

The factory's type selector is `param_1[6]` and is reached in `0x420480`
through the record returned by `0x45C2C0` (`&DAT_0053B17C`). The separate
`track.obt` parser at `0x45C630` dispatches textual `obj` records to
`0x45D270`, which recognizes `car`, `obstacle`, `wobble`, `static`, and
`checkpoint`; its `car` subtypes select `PCAR`, `ACAR`, `NCAR`, or `GCAR`.
That parser establishes the scene-record format, but does not establish that
its textual object subtype supplies the factory's `param_1[6]` value. The
factory type-1/type-3 gameplay roles and their value source remain unresolved.
The `0x45C2C0` accessor returns the object at `0x53B17C`; direct address xrefs
identify that pointer return and the later selector read at `0x45D9E2`. The
15 direct callers of the accessor were inspected, including `0x420480`, which
passes the pointer to `0x426000`; no write to the `+0x18` selector was observed
in those caller bodies. This does not exclude indirect writes through unresolved
aliases or a source outside the analyzed caller set.
Evidence: `ev_f0b883adbd848f2664e51cb6ba5badacca072ffb56068ff45b06a058e5a0b18b`
(`0x45C630`), `ev_7e2fd437460b56cf40e509f370a9ae91cc2c47ac5d476f84011adb084b95bba5`
(`0x45D270`), and
`ev_0c21c172a16c58e01c47ed385c88c6018c7190ca4bd9f9ecdbedda281414dedd`
(`0x45CAE0`).

## Startup mode switch at `0x401460`

The call graph in `0x401460` enters `0x417270`, switches on `0x4CF5B0`, and
reaches shared finalizer `0x4172A0` for every observed selector. Mode 0 calls
`0x4014D0`, repeats `0x401E10` until its low-byte result is zero, then calls
`0x4BA310` and `0x401650`. Modes 1, 2, 3, and 4 call `0x402BE0`, `0x402A90`,
`0x402A80`, and `0x402AA0`, respectively. Values above 4 skip the mode body
and still reach the finalizer. `plan_startup_frontend` records this call order;
the number of loop iterations is an input because it depends on frontend
interactions.
Evidence: Ghidra 12.1.4 function dossier for `0x401460`,
`ev_650e90e131a7057cf611879075e0ca00aa6b5ac0812c04a5dea8bde3066d04ca`.

## Reconstruction mapping

- `include/viper/frontend_dispatch.hpp`
- `src/frontend_dispatch.cpp`

This models the recovered visible-item selector values and dispatcher, not a
replacement for the UI or a full implementation of the original handler
effects. `plan_startup_frontend` records call routing, while
`plan_startup_mode_entry` projects selected setup writes and call sequences.

`plan_startup_mode_entry` now expands the mode 1-4 handlers. Mode 1 (`0x402BE0`)
sets the runtime mode global to 4, initializes its local `0xCD4` record through
`0x45C5B0`, and copies the three observed string sources at `0x4CF9A4`,
`0x4CF9AC`, and `0x4CF9B4` into record offsets `0x08`, `0x2C`, and `0x39`.
Mode 4 (`0x402AA0`) sets the same global to 11, uses the three source addresses
`0x4EB130`, `0x4CF994`, and `0x4CF99C` at those offsets, and copies 12 global
words from `0x4EB1B8` to `0x5081A8`. Both then call `0x4014D0`, `0x40A5D0`,
`0x401EA0`, `0x40A600`, and `0x401650` in order. Mode 2 (`0x402A90`) is an
observed no-op; mode 3 (`0x402A80`) only calls `0x4014D0` and `0x401650`.
Runtime string and word contents are supplied as observations, and the
intermediate record's field meanings remain unresolved.

Evidence: `0x402BE0` (`ev_e18a83bf2fb7d728b9f2118ad77e7ea29ee8b97c48f7eb808a94fed0fe4c068f`),
`0x402AA0` (`ev_4bef3b2b1e5ba9f3a63671572665759917150cd59a5ddfcb06c0552ff88de86b`),
`0x402A90` (`ev_0460e87b077789528e52d4f1c0df2e3a17e0a9a44f1bad789b9b4b02dddcfeaf`),
`0x402A80` (`ev_998fa7e40eab551f228ce06d099e09c245076f8fe499a75b63c6b8cd0acc82b2`),
`0x4CF9A4` (`ev_a735212b6b27c4bde542577d4d465e06baf0f4ae987ef90cd60d10341cd21d3e`),
`0x4CF9AC` (`ev_dc22b3dd6e3b1b46f30a13c3f45d0245964e300fdc0e6aee0b5aed753cf3fbed`),
`0x4CF9B4` (`ev_5bea9ae92ad44871e26bc6a52ab098d9ed5a4c2078b43ba0e409852d5b96a90f`),
`0x4EB130` (`ev_442a14482cafae572ca409962946b3141ebb1aab2cc83adf1de181bc7766e4b3`),
`0x4CF994` (`ev_a5b5c2bc6da773fded0f45fba6836e1633615d2d74b6afd8324dd3564aa8df5b`),
`0x4CF99C` (`ev_71f73be6ccdac1d2473b3b04799aac0e6c9e45e505102687f41ee1424701a92c`),
and `0x4EB1B8` (`ev_356b4feae49b5db108d61b96bbd2820af28ef199bcd62d58c8382b3698651008`).

## Multiplayer worker callback and status supervisor

The thread callback passed by `0x49C670` starts at `0x49BD40`, but Ghidra does
not assign that address to a function: it resolves as an instruction fragment
outside any procedure. The 123-byte fragment sets the worker helper argument to
`0x0F` with `SetThreadPriority`, then calls `Sleep(0xFA)` and polls `0x414E20`;
a true poll result calls `0x414E80`. Priority-setting failure logs and enters
the panic helper, whose continuation is not assumed. The `0x414E20` getter reads
thread-table byte `+0x18`. `0x414E80` sets bit 0 at `+0x1A`, clears `+0x18`,
and calls `SuspendThread` on the thread handle at `+0x0C`. The `0x414E40` getter
reads thread-table byte `+0x19`, which controls the callback's return: true
returns, false repeats the loop. The loop checks the published object at
`0x56434C` and byte `+0x00`.
For an active object it conditionally calls `0x49EB60` for non-null `+0x0C`,
virtual `+0x04` on non-null `+0x08`, and virtual `+0x08` on non-null `+0x04`.
It then repeats the 250 ms sleep and thread-table polls. `plan_multiplayer_worker`
models those raw flag observations, the priority-failure panic gate, and the
object-presence gates; it does not execute OS thread operations or infer the
virtual methods' effects.

`plan_multiplayer_status_supervisor` models `0x49C2E0` from caller-supplied
status snapshots. The thread-ID comparison uses `0x49C8A0`; a mismatch calls
the worker cleanup at `0x49C7A0`, which waits up to 3000 ms and enters the panic
helper if the worker has not stopped. The observation model keeps that timeout
result explicit and marks post-panic continuation as unknown.
It lazily initializes the Synchronizing and Waiting text resources, draws the
initial status, and sets thread priority 2. Numeric states `5..16` take the
update-tick path; state `0x0F` additionally calls virtual `+0x6C` and `+0x68`,
looks up the resulting `+0x30` string with `0x4065A0`, copies through the
terminating NUL without a capacity check, then calls virtual `+0x50`. Outside
that range, a non-null communication object is polled through virtual `+0x24`;
false results while state is greater than 4 trigger another update tick within
that readiness loop. State `0x11` has its own repeated tick loop. The model
preserves these path-specific status snapshots rather than re-dispatching every
observed state through the outer `5..16` test. The terminal paths draw the waiting/final
status, set thread priority 0, then either publish the completion global and
call the no-op `0x49C4E0` for states below 4 (return low byte zero, upper bits
unknown), or return low byte one with the observed state's upper 24 bits.
`0x4A1680` resolves the 22-entry label table used by `Race's ChatLine`:
`PGS_INVALID`, `ServerRefusedConnection`, `ServerNotFound`, `Unconnected`,
`PGS_ERROR` (not a state), `SearchingForServer`,
`ServerFoundWaitingForReply`, `PGS_CONNECTED` (not a state), `PGS_HELLO`,
`PGS_RUNNING`, `PGS_PROCEEDING`, `PGS_CAR_CHOICE`, `PGS_CAR_WAITING`,
`PGS_CAR_GET`, `PGS_RACE_WAITING`, `PGS_RACE_BEGIN`, `PGS_SYNCHRONIZING`,
`PGS_POSTSYNC_WAITING`, `PGS_GO_WAITING`, `PGS_GO_READY`,
`PGS_GREENFLAG_WAITING`, and `PGS_GREENFLAG`. The ServerRefusedConnection,
ServerNotFound, Unconnected, SearchingForServer, and
ServerFoundWaitingForReply labels are resource-managed strings; the code model
retains stable label IDs rather than assuming a fixed localized rendering.

The status callback at `0x49C5C0` returns a zero low byte when its global
object is null or the signed gate field at `((global + 8) + 4)` is at most 4.
When the gate passes, it invokes the `+0x84` method with the callback's
original argument unchanged; the gate field and argument are distinct inputs.
For the `Race's ChatLine` implementation, slot `+0x84` resolves through vtable
`0x4CB5A0` to the shipped callback at `0x4A1B70`. That callback performs a
label lookup through `0x4A1680`, passes the `status >= 15` test and label to
`0x4A1AF0` (a `RET` stub) when the test passes, then returns the byte at object
offset `0x1C8 + argument * 0xD9`. The portable
`plan_multiplayer_status_callback` reports the label ID and no-op call, models
the gate field and callback argument separately, then reads this byte from a
bounded raw object span. The label ID captures the stable 22-entry mapping;
resource refresh and one-time registration behavior remain outside this
callback projection. The supervisor's state 15 is
`PGS_RACE_BEGIN`; its
vtable `+0x50` target `0x4A2F10` calls `0x4A3AC0` and writes state 16
(`PGS_SYNCHRONIZING`) when the current state is greater than 4. `0x4A3AC0`
passes a 0x34-byte message to `0x49FEB0`; that helper sends through `0x4A8BE0`
only when the selected connection record has flag bit 0 set at `+0x19`.
`plan_multiplayer_status_supervisor` now records the helper call and resulting
state-16 store for status 15. Its send projection reads a bounded table of
0x1C-byte connection records using the ChatLine's `+0x18` index. With flag bit
0 set, it reports the message's known bytes (offsets 0, 2-7), length, target
record field `+0x0C`, and the nested `0x4A8BE0` -> `0x4A8D90` -> `0x4A9C40`
call path. After packet allocation, `0x4A9B50` copies the complete message to
packet offset zero, stores length at `+0xE4`, zeros `+0xE8`, and copies the
three-DWORD remote address from connection-row `+0x0C` to packet `+0xEC`.
Allocation failure logs `SendReliable: Can't alloc packet`. The sender then
either calls `0x4A9C10` for immediate dispatch or links the packet through
`0x4A9A80`; the choice depends on unobserved sender fields at `+0x20`, `+0x24`,
and `+0x28`. The plan now accepts those fields plus the allocation result as
observations and reproduces the exact branch: immediate dispatch requires no
pending queue head and either a null `+0x20` state pointer or an unsigned
8-bit sequence difference below 2. Otherwise it selects the pending-queue
append helper. Message byte 1 and bytes beyond the initialized prefix remain
unknown; packet memory changes and the eventual socket result are not modeled.
Other protocol transitions remain open.
Direct evidence for the `+0x84` target is in `multiplayer_chatline_status.evidence.json`:
vtable bytes `ev_62f95674d52695482bb8fd8c882ef36b3a9d6cd80141bf9e60fcfd7c12347919`,
callback bytes `ev_744d0b3fbb77444168a0d68c02e8ebf95f75e895b350902400ce5d202bf5180e`,
and the no-op helper dossier `ev_1f22f54d41a75387063e4f0a02922c1567191c190957008aa4d561e7e9bb16e0`.
The `+0x50` vtable entry and transition bytes are in
`multiplayer_status_transition.evidence.json`; sender and nested helper bodies
are in `multiplayer_status_send.evidence.json`. Packet allocation, layout, and
queue dispatch dossiers are in `multiplayer_packet_queue.evidence.json`.

Evidence: constructor `0x49C670`
(`ev_1b1b16e0d46018109901a947f5acfabb80f256a09a65b05e553d13a7e0902d21`),
callback bytes at `0x49BD40` (`ev_16c59f1f02b7278658015c2cc7c31d99080e5c4193ae360670d1552c5ac83948`),
`0x49C2E0` (`ev_cc38133465aea6a6270a54acf73b71c9e8bc156a986ca08c8d3c9c20859609c3`),
`0x49C5C0` (`ev_8aa76c590e0dfe52e5a00d2fcb9e0b94170ef3aeb99fe51595c709343ed47710`),
`0x49C850` (`ev_b306cd7655ae500b3817b67f74b810ce55195e9c1095e6896d5d22fc4fad1697`),
and `0x49C8A0` (`ev_d26c7a2706002627197aa284f45bb90e17ec56407bd9fffc60570c2ec13423c6`).
Thread helpers: `0x414D40` (`ev_e55d3db07a2b6ac66e736584c4e5733db5a52a2c5b3f9e538864483cd02b0cdd`),
`0x414E70` (`ev_33f5c572d632d7bfa09353b1e5af1cc009e9840c5549d51fea9f23240c8b4986`),
`0x414E20` (`ev_613f178ae1c89951372266afba662e2686306aade48948a81b659bce551193d0`),
`0x414E80` (`ev_75d1dc789d043eada6e402d991f1f75ea2d27e4b23b62bf416117ebdcbb39638`),
and `0x414E40` (`ev_18a7597d73c64b4373d137141faff76d43000e62a3782e64f4700a90991283a6`).
Supervisor helpers: `0x49C7A0` (`ev_0e4cae09590de2e6eea747d730b5f1d08ec6ed7dc1f7dfba0e5cfdc573624561`),
`0x49C4E0` (`ev_ca3f1fe034f5a420acd1e1fa2d6bf4dcdd4efbd406d03e0f7d52603a4e9afc00`),
`0x49C190` (`ev_7bfedfa6f4c9502a323c17b97324d9f72c1ef95d60ee0852ca7dc7ac74032bfd`),
and `0x4065A0` (`ev_dbe6861f15b8e568e7a960811bfeb01c7d972f97b05a11e5626b0d36ca9d0ce6`).
Status-label table `0x4A1680` (`ev_c63c6b92c1d8de5b9b3807364e549757e75855b1d24e4b25a70589e19038b253`),
ChatLine vtable bytes at `0x4CB5A0` (`ev_fba7e3d0db2606c9f41c1326e588338f2704b4acfe8dec48fc435bd14ccddcb2`),
and callback/transition instruction observations at `0x4A1B70`, `0x4A2F10`,
and `0x49C5C0` are preserved in `multiplayer_protocol_states.evidence.json`:
`0x4A1B80` (`ev_2387c1dbb30cb57afd291af391564a5eb1d648aa055bd98cf5989e6352f2c8dd`),
`0x4A1BAC` (`ev_5b0051a14a742975444f638cf122d19e777045c28ee1cf062c932b346f51b95f`),
`0x4A1BB3` (`ev_057681112069f395d2de983e54cb51ba0468a6e010802487c2585a52df5f0774`),
`0x4A2F5C` (`ev_c4312b35b30a3687cc931af1e6e689f8ed02ecd49a16c55a857302857a033cfa`),
`0x4A2F64` (`ev_71490f2132af218210f4bdfcdbc356fc3534357e23395430ad183fc52ee94259`),
and `0x4A2F69` (`ev_efa0ed141ba5fcf0360896ded263d8f0d364e2a438e314be6dd457a703198e57`).
# Replay save callback

The save callback at `0x407C60` returns when replay visibility byte
`0x4D0668` is clear. Otherwise it tears down the current replay through
`0x4265C0`, runs `0x407E70` / `0x4119B0` to ensure the directory at
`0x4EC820` exists, and initializes four localized controls once using mask
`0x4EC52C`. It conditionally refreshes cache objects `0x4EC540` and
`0x4EC7D8`, then calls the Save dialog helper `0x474770` with a `0x104`-byte
path buffer. The helper contains the `UI_Save_DangerOverwritingDialog`
resource and returns whether a path was accepted.
The callback is stored into a stack control record in the main replay screen
at instruction `0x406D40` and passed as constructor argument 8 by the analysis
screen at `0x4084CE`.

On acceptance, `0x4112A0` opens the chosen path with `GENERIC_WRITE`,
`CREATE_ALWAYS`, share mode 1, and normal file attributes. If the helper
returns no handle, the callback refreshes `0x4EB7E0` and `0x4EB818` as needed
and shows the `0x472F00` error dialog. With a valid handle it writes the
fixed `0xCD4` replay record via `0x4116E0`, calls serializer `0x426960`, and
closes the handle via `0x411500`. The serializer attempts a `0x1C`-byte header
with DWORD literal `0x5245504C` (on-disk bytes `4C 50 45 52`) and version 6, followed by a word array
(`0x508A70`, `0x508A58 * 4` bytes) and a tail (`0x508A7C`, length
`0x508A3C`). The original callback ignores `WriteFile` results; the model
records the calls and sizes without asserting successful writes.

Implemented by `plan_replay_save` in `src/replay_save.cpp`. Evidence:
callback entry `ev_024be3ebe97e5a66714ad1059ae401940918d492b49354414c68a4014551384a`;
main-screen callback binding `ev_131eac83dc86beb59a6ddf3e0409654bb680ec73ec9b9d56ba000f1f15efeb6a`;
analysis-screen callback binding `ev_865776941fc300912ec22a42389651987b9620cbae55d7bb48627f4dbb9981b0`;
Save dialog call `ev_f4ab3e7fe205294f5479ad1dad75d8b4f1ced64a3c60eae511a4838707c311b6`;
file open call `ev_4380ec23615ca972ea33c1fb6713b6f0aa3159d63d82c8e1904b90293ea126fa`;
fixed-record size `ev_75a722f2eca026d9c247ed8257ddd8bae945b99a684f7b4095540ba7b1c595d8`;
Save dialog dossier `ev_0501370e8104b61f4f7d1f7582409564ef28851acc82538813c9a3c6ed6a417f`;
open helper `ev_d7f14db03605ea27644e1afa106e04feb46db400df329dad6791f1f59a67a33b`;
write helper `ev_b4dc5c25f3e13611b6765669f86b6a7afe3ed2cf415f82e42e48dddb59bbe8c0`;
serializer `ev_71639991338a9d550d82b23de7353c21f51e66f541cb54c9886ec65cc5cdf7ee`;
close helper `ev_ded57c7ae28f00fe650c9fa9e5c2f6ebc7b6bb0cca22d135d83b6c02f99f0901`;
label constructor `ev_64a486a18cbeec2d9e04272cddd580fd25212a3fc68557b6eab31da01816e7e3`;
replay teardown `ev_a1dcb470a377530ace98e6812d924fd680041f60f9f4c71148c6a8d85bf39c2b`.

# Replay file load parser

`0x426790` reads a fixed `0x1C`-byte header from the file handle and checks
the DWORD cookie `0x5245504C`, format version 6, file word count against
`0x508A58`, payload length against signed limit `0x200000`, frame count in
`[0, 0x508A58]`, and base packet size against `0x508AD0`. It then reads the
full `0x508A58 * 4`-byte frame table and the declared payload. On success it
sets `0x508A34` and calls `0x426580`. It reports the exact error-string
addresses for header read, cookie, version, word count, payload size, frame
count, packet-size mismatch, and buffer-read failures. On a packet-size
mismatch, both the specific mismatch and generic buffer-read errors are
reported. `plan_replay_file_load` models these ordered gates and is composed
into `plan_replay_load` when raw parser observations are supplied.

The replay-open helper `0x411430` uses `GENERIC_READ`, share mode 1,
`OPEN_EXISTING`, and flags `0x08000001`; `0x411560` requires an exact-length
`ReadFile`. The preceding `0x45C590` validator accepts record type 3 and size
`0xCD4`. Evidence: open helper
`ev_15578b7f616310e7b7b5f9e78af58b1941260b9ca36e21a54696bf224c06f8dc`;
read helper `ev_85391766502a6d1aea6fdb7831513bc3e48ae1081444872da5c29db550501402`;
record validator `ev_12373767ae142af422d9663eb119ad1eae12c02249beab4dc5e7fd29a0d2df9d`;
parser `ev_755f78caf1b538eacabe2f2bbe13f931431a55df97448083c2db2f9b79688fd4`.
