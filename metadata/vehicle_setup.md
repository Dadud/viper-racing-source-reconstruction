# Vehicle setup evidence

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA.

## Car setup validation at `0x45C5D0`

The function reads the car count from `param_1 + 0xCA8`. It walks records
starting at `param_1 + 0x28`, advancing 200 bytes per car. For each record it
reads a 32-bit field at record offset `0xBC`. If the field's unsigned bit
pattern is below `0x3C23D70A` or above `0x3F8147AE`, it calls the fatal/error
helper `0x411000` with the diagnostic format `setup_cars: car %d has bad wheel
lock: %f`.

Those IEEE-754 bit patterns represent approximately `0.01` and `1.01`; the
code compares the bit patterns as unsigned integers. The portable
`valid_wheel_lock_bits` preserves that exact comparison. The wider record
layout and other field meanings are not inferred from this one validation.

### Human setup file/resource versus configured race row

The human vehicle path does not load its active setup from the 0xC8-byte
participant row. `0x45EFD0` calls `0x45EE40`, which constructs a per-user
`setups\\<name>.csu` path and accepts a 0xD4-byte file; it copies the first
0x8C bytes into the active human vehicle. On an open/size failure,
`0x45F150` tries bundled `<car>.ccs`, caller car fallback, and
`default.ccs` resources; each is an 0x8C-byte CCS v2 payload. The validated
external install has the bundled `.ccs` entries but no `.csu` setup files.

The reconstruction implements this selection in `GameAssets::load`. It reads
`Config/setups/<player_name>.csu` only for a bounded ASCII filename stem and
only when the file is exactly 0xD4 bytes. The first 0x8C bytes become the
active setup; the uint32 at +0x8C is retained and a value other than 1 raises
the native-style warning without rejecting the file. Missing, malformed, or
unreadable files use the selected car's `<car>.ccs`, then
`common.res/default.ccs`. The CLI accepts `--config-dir PATH`; otherwise it
checks `Config` beside the data directory, beside its parent, and at the
supplied install layout's sibling `GameRoot/Config`. This implements the
observed per-user setup source and documented bundled fallback, but does not
implement setup editing or save `.csu` files yet.

Separately, the CARF mapper at `0x45E4B0` reads 35 DWORDs (0x8C bytes) from
its second argument. In the garage path, `0x48AC20` first calls `0x45EFD0` to
load the active `.csu`/CCS bytes into `DAT_005620B8`, then calls `0x45EC20`
with that buffer as its second argument. `0x45EC20` forwards the same pointer
to `0x45E4B0`; this confirms that the first 0x8C bytes of the active setup
payload are the mapper's 35 words in this path. The new
`prepare_car_file_runtime_mapping_from_setup_payload` adapter consumes that
bounded payload directly. UPGS selection flags and FDIV mode remain explicit
inputs. The single-race configured-row words are populated after the menu
builder: `0x40A5D0` calls `0x40A7B0`, `0x40A710`, then `0x40A620`;
`0x40A620` walks 0xC8-byte rows and dispatches marker 0 to the human
initializer `0x45EFD0` with `row + 0x34`, and marker 1 to the AI initializer
`0x41A000` with the same row offset. Thus `0x40A850` leaving those words
unwritten is expected; it does not leave the configured row uninitialized.
The entry dispatch plan and regression now cover this order and the two
initializer calls. The human initializer loads the active `.csu`/CCS and uses
its car name; the AI initializer starts with track `aidef.ccs` (or numeric
fallbacks) and applies entrant, driver, and difficulty inputs. The exact
field-by-field AI mutations and the resulting configured words are not yet
integrated into live runtime rows. Evidence for the post-read caller is
`0x40A5D0` `ev_c62745c06d284358d84017187404fd05c8ee70061d4d007510a611a51c31d280`
and for the row dispatch is `0x40A620`
`ev_975a3cf2e208f3080f652baa79972d58265218201781eed5a8f8690b445e8630`.
The single-race setup owner
`0x45C5B0` clears its complete 0x335-DWORD object before the menu callback;
`0x40A850` then writes marker, display name, car stem, and the words at
`+0xC0/+0xC4`; setup dispatch follows afterward. Evidence:
`0x48AC20` `ev_65957331b5b8c2fc010ff52677e05f3e445d64945de3207032abafa50a4a4a81`,
`0x45EC20` `ev_d180d832414b860ca1b05f8b253f39249f194104a7d5658e5205575f73d68958`,
`0x45C5B0`
`ev_ce5077ebc485f33b39f286697b2bcc7e5db06146f4f0a813f5c2a13eb3d47612`,
`0x40A850` `ev_e52a864e503a5c943f75c6f007a442fd95fca2d59653187e01047142354d9bec`,
`0x45EFD0`
`ev_adb94e23d97afe66b379995ca90ed5ccce81d943c2e3b7d3e776824dd966026f`,
`0x45EE40` `ev_d5349302be5d3e8df0415e2df0b15fc7f1b0bf5fe3018f4e0ba7bef8c501b36f`,
and `0x45E4B0` `ev_520f4b2d9aec343dddb2f2127431bfe0c6d4c25a99ab20ffa06eec1a05a08317`.

The AI path has a distinct setup source: `0x41A000` loads `aidef.ccs` from
the active track resources into its 35-word AI setup block; on failure it
copies the numeric defaults from `0x45F2A0`. It then uses the entrant ordinal,
selected driver record, and difficulty table to update the AI setup through
`0x41A0C0` and several direct scalar writes. All eight shipped `.trk` bundles
contain `aidef.ccs` as CCS0 version 2 with a 0x8C-byte payload; the runtime now
owns this raw payload from the selected track bundle in `GameAssets`. This is
the AI initializer's base setup; it is then mutated before the configured row
is passed to the CARF mapper. The runtime has not yet applied the AI
difficulty/driver mutations or passed the setup into an initialized physics
vehicle. Evidence: `0x41A000`
`ev_67a3cffd701fba3131c3793b441c8ae76089194d822832bebb5902ef73ac6b54`,
`0x41A0C0` `ev_85870aa4ba6b470ea3e074c852c0ac3a55794497857bea8dd0235a0a713f8549`.

`build_ai_configured_setup` now composes the validated 0x8C-byte base payload,
the three documented driver/track writes, and `0x41A0C0`'s coefficient outputs
into the 35-word AI setup. It writes setup word 1 from track-row `+0` times
driver-record `+0x20`, scales words `0x12` and `0x13` by track-row `+0xC`
times driver-record `+0x6C`, and merges the seven exact fallback coefficients
or the loaded CARF coefficient loop at words `0x14..0x1A`. The existing
coefficient planner retains the X87/FDIV numeric approximation caveat.
Regression coverage exercises both coefficient branches, preserved base words,
scalar destinations, and short-payload rejection. This is a reusable setup
composition; the runtime still does not resolve each AI's difficulty group,
track mode, CARF resource/fallback block, or merge this setup into a live row
and vehicle constructor. Missing inputs therefore remain explicit at the API
boundary.

## World car variant tags at `0x45CAE0`

The world parser reads a numeric subtype from each `obj_car` record and writes
an object tag into the factory record before constructing its separate scene
object. The observed mapping is subtype 0 → `PCAR` (`0x50434152`), 1 → `ACAR`
(`0x41434152`), 2 → `NCAR` (`0x4E434152`), and 3 → `GCAR` (`0x47434152`).
Other subtype values take the error path. Subtype 1 also calls `0x41A2A0` with
the generated record; its effect is not modeled here. The portable
`world_car_object_tag` reproduces only the verified numeric mapping and returns
no tag for other values. This scene-object construction path is distinct from
the physics-task factory; see `physics_task_objects.md`.

### Scene-record identity handoff

Instruction evidence for `0x45CAE0` shows that the local `0x1E4`-byte
construction record begins at stack address `[ESP+0x18]`. Immediately before
`0x45EC20` parses the car fields into it, the parser stores the current zero-
based car-record ordinal (`EBP`) at `[ESP+0x1AC]`, which is record offset
`+0x194`. The constructor `0x463920` reads `param_1[0x65]` (the same `+0x194`
slot), copies it to scene-object `+0x1C8`, and passes it as the index to
`0x45DEB0` while registering the source car strings. Thus this ordinal ties the
parsed car record to both the scene object's identity and its string-table row.
`WorldCarSceneRecordIdentity` and
`make_world_car_scene_record_identity` model the verified FourCC/ordinal pair;
they do not synthesize the remaining 0x1E4-byte record.

Evidence: `0x45CAE0` instruction listing
`ev_8a9c62f46bd9da928258938b70ce593354bcf7ab443308fbae3fcae2819b71c5` and
`0x463920` pseudocode
`ev_b49bd77798d2eb9f50d5c282a91ae0e40234bd80899de1f9d2925f4270c57795`.

## Wheel/tire setup at `0x441C10`

This routine reads wheel configuration values from a larger vehicle record,
applies the constants `0.0254` and `0.017453292` to selected values, constructs
a tire asset path with the format `%s%s.tir`, and falls back to
`def_tire.tir` when the requested path is not found by helper `0x417A20`. It
then calls tire loader `0x435B50` with derived dimensions and stores returned
tire-object fields into the wheel object.

The factors match inches-to-meters and degrees-to-radians. Their use is
implemented in `include/viper/vehicle_setup.hpp` and
`src/vehicle_setup.cpp`. `0x435B50` copies 30 DWORDs (120 bytes) from each TIRE
resource into its in-memory record. In `common.res` and the four extracted car
bundles that contain TIRE records, the first 32 bytes form a fixed-size
human-readable tire label and the remaining 88 bytes are 22 little-endian
float fields. The portable `parse_tire_resource` decoder preserves those
fields in order without assigning meanings. The setter outputs below reveal
part of the allocated tire-curve record, but downstream force semantics and
the full `.tir` grammar still need separate analysis before a full wheel model
can be written. `--inspect-tire BUNDLE NAME` exposes the recovered label and
numbered float values through the CLI.

### Front/rear tire resource selection

The wheel initializer `0x441C10` chooses suffix byte `f` for wheel indices 0
and 1, and `r` for indices 2 and 3. It formats the selected car stem and that
suffix as `%s%s.tir`, checks the result through `0x417A20`, and copies the
`def_tire.tir` name into the path buffer when the selected resource is absent.
The format and fallback strings at `0x4D5D98` are `f`, `r`, `%s%s.tir`, and
`def_tire.tir` (read-bytes evidence
`ev_f15b9198220e754e68c51a792a17b62d3fa62166d33a8fcc5935964e088b4056`).
The selected path is then passed to the TIRE loader `0x435B50` by
`0x441C10` (function evidence
`ev_f58fb7c53b9e5301e4a52e4cf10624d8af065857ba0bac9b90aac634d649bb59`).

`GameAssets` now resolves and owns four parsed tire records in that wheel
order, preferring `<car>f.tir` / `<car>r.tir` from the selected car bundle and
falling back to `common.res` `def_tire.tir`. Inspection of the external
installation confirms that Exotic, Plane, Sedan, and Sports have both axle
resources, while Viper has neither and therefore takes the verified default
path. This supplies the existing four-wheel tire-curve composition with
asset-backed tire definitions; it does not initialize wheel geometry or wire
the composition into live physics. The `0x433C00` call loop increments its
wheel ordinal from 0 through 3 and its destination by `0x1A4` each time
(`ncar_wheel_callback_setup.evidence.json`, evidence
`ev_a957a079d7cb019a7d9f11b607222893011d8db06a2f29fc193137619c0e6eeb`).

The CARF mapper `0x45E4B0` writes raw CARF words 12 and 13 at runtime-record
offsets `+0x58` and `+0x5C`. The `0x433C00` initializer reads those as width
and ratio and stores `+0xD84 = (1 - ratio * 0.01) * width * 0.0254` and
`+0xD90 = -(ratio * 0.01 * width * 0.0254)`. The mapper also writes runtime
record `+0x12C` from CARF word `0x67` times setup word `0x22`; the constructor
stores this value multiplied by `0.0174` at `+0xDCC`. The implementation now
exposes `initialize_ncar_integrator_wheel_callback_setup_from_runtime_record`,
which reads all three little-endian inputs from a bounded runtime record. A
regression composes the CARF mapper and this adapter for synthetic source
values. The same mapper stores CARF word `0x66` unchanged at runtime-record
`+0x128`; this is now preserved by the adapter alongside the scaled word
`0x67` at `+0x12C`. A regression composes the word `0x66` store through to the
FNIM-derived NCAR reset-proxy bounds. This resolves the record source but does
not establish the field's semantic name; remaining 3D wheel placement inputs
and live collision integration are still unresolved.
Evidence: mapper `0x45E4B0` `ev_520f4b2d9aec343dddb2f2127431bfe0c6d4c25a99ab20ffa06eec1a05a08317`;
constructor `0x433C00` `ev_a957a079d7cb019a7d9f11b607222893011d8db06a2f29fc193137619c0e6eeb`.
The direct `+0x128 <- CARF[0x66]` store is visible in dossier
`ev_472e5bfc79a705e4af49ea09d44a0d3d3c3d05308100792e1c336237ca9ca4ff` (`0x45E4B0`).

The `0x4509B0` call in `0x433C00` is a separate model-extents scan, not a
steering transform. It obtains the model vertex count and pointer through
`0x50D00`, scans XYZ at offsets `0`, `4`, and `8` of each 32-byte vertex, and
writes six minimum/maximum outputs. `measure_model_vertex_bounds` reproduces
that scan and its initial `+/-10000` sentinels for an empty vertex array. The
FNIM regression checks both a synthetic triangle and the empty case. This
provides asset-backed extents when called with a parsed model's vertices; it
now feeds the existing NCAR reset-sphere geometry observation through its
precomputed-bounds field, so the native twelve-sphere proxy can consume parsed
FNIM bounds without rescanning vertices. The CARF mapper stores word `0x66`
unchanged at runtime-record `+0x128`; the setup regression composes that value
through the actual geometry function. The geometry regression checks all
twelve generated centers/radii and the aggregate group radius. The geometry
function rejects non-finite model bounds and vehicle-record inputs. The proxy
is still not connected to live collision handling. Evidence: `0x4509B0`
`ev_f3e69307c431f783075d86dbeed730ee32736a6159e31f8b9633cd65593d0208`.

### Mesh-derived wheel anchors in `0x433C00`

After measuring model extents, the NCAR constructor selects one FNIM vertex for
each wheel. It forms four candidate positions in the model-local basis:
X alternates between `-0.4` and `+0.4`, Y is
`-runtime[0x128] * 0.0254`, and Z is
`(1 - runtime[0x5C] * 0.01) * runtime[0x58] * 0.0254` for indices 0/1 or
`-(runtime[0x5C] * 0.01 * runtime[0x58] * 0.0254)` for indices 2/3. It scans
the 32-byte model rows using XYZ at offsets 0/4/8, computes Euclidean distance,
and replaces the current candidate only on a strict decrease. Equal-distance
ties therefore retain the earliest vertex. The four selected pointers are
passed in wheel order to `0x441C10`, whose subobjects advance by `0x1A4` bytes.

`select_ncar_wheel_vertex_anchors` reconstructs these formulas and scan using
the three mapped runtime fields and parsed model-space XYZ positions. It leaves
an index unset for an empty mesh. This is setup geometry only: it does not
establish a body pose, suspension travel, contact state, or a live physics step.
`select_ncar_wheel_vertex_anchors_from_runtime_record` reads the three
little-endian mapped fields from a bounded record and consumes parsed FNIM rows
directly; it rejects records shorter than the last required DWORD.
Evidence: `0x433C00` pseudocode and body range
`0x00433C00..0x00434A51`, dossier
`ev_f2bb786a74070a51b02f0626e35bf0593a3b38ab0f124dd60bb1a06abe4d65dd`.

`initialize_ncar_wheel_core` reconstructs the bounded, deterministic wheel
constructor outputs that follow anchor selection: axle and side classification,
the model-space wheel candidate and offset relative to the selected FNIM vertex,
steering angles at `+0x60/+0x64`, and geometry values at `+0x3C/+0x5C/+0x44/+0x48`.
Its input explicitly includes the car object's mass-like scalar at `+0x1F0` and
the observed FDIV-compatibility flag. A short runtime record or wheel index
outside 0..3 is rejected. The final suspension-height correction uses the
existing VC98 compatibility division model when that flag is set. Tire loading,
helper-owned wheel transforms and remaining `0x441C10` stores are still outside
this core plan. Evidence: `0x441C10` pseudocode, dossier
`ev_c5aa0c77b19958eb9c8ba45643b19d858c19899f5cfc958f00b97ab6df232f06`.

### TIRE field consumption at `0x435BF0`

The tire-object initializer reads the 22-float record at `param_2 + 0x20` and
passes values to nine small setter routines. The following table translates
those reads to the decoder's zero-based float indices. A transformation of
`identity` means the bits are passed through unchanged; it does not establish
the field's physical meaning.

| Float index | Record offset | Observed consumer |
| ---: | ---: | --- |
| 5–8 | `0x34`–`0x40` | `0x435510`, passed unchanged as four values |
| 3 | `0x2C` | `0x435600`, multiplied by `0.4545` |
| 4 | `0x30` | `0x435600`, multiplied by `0.042279862` |
| 9, 11 | `0x44`, `0x4C` | `0x435540`, each multiplied by `4.45` |
| 10, 12 | `0x48`, `0x50` | `0x435540`, passed unchanged |
| 13, 15 | `0x54`, `0x5C` | `0x435580`, each multiplied by `4.45` |
| 14, 16 | `0x58`, `0x60` | `0x435580`, each multiplied by `255.74712` |
| 17 | `0x64` | `0x435620`, passed unchanged |
| 20 | `0x70` | converted from degrees and passed through `sin` to `0x435630` |
| 21 | `0x74` | passed unchanged to `0x435630` with the preceding result |
| 18 | `0x68` | `0x435650`, passed unchanged |
| 19 | `0x6C` | `0x435660`, passed unchanged |

Indices 0–2 are not read by this initializer. Some later reads overlap with
earlier ones because the tire setup derives multiple object properties from
the same source value. This is direct data-flow evidence from the shipped
binary; identifying units or physical concepts from the constants alone would
be speculative.

The allocation passed to these setters is `0x40` bytes (`0x435B50`). Setter
implementations establish these internal destinations:

| Tire-object byte offset | Assignment |
| ---: | --- |
| `0x00`, `0x04` | `0x435580` writes slope and intercept for its transformed pair |
| `0x08`, `0x0C` | `0x435540` writes slope and intercept through two input points |
| `0x10`, `0x14`, `0x18` | values passed from float indices 17, 18, and 19 |
| `0x1C`–`0x28` | four values from float indices 5–8 |
| `0x2C` | external argument supplied by `0x435B50` |
| `0x30`, `0x34` | transformed float indices 3 and 4 |
| `0x38`, `0x3C` | `sin(index 20 in radians)` and float index 21 |

For `0x435540`, the stored pair is
`m = (y2 - y1) / (x2 - x1)`, `b = y1 - m*x1`, from its four inputs
`(x1, y1, x2, y2)`. For `0x435580`, the pair is the same line construction
after replacing each ordinate with `y/x`: inputs form points
`(x1, y1/x1)` and `(x2, y2/x2)`. These are verified arithmetic behaviors;
calling them particular physical tire curves remains a hypothesis until the
record consumers are traced.

### Car-to-wheel update path and runtime curve consumers

The base constructor's body initializer `0x43E490` calls `0x4375C0`, initializes
eight 0x30-byte subobjects at `+0x6C` and sixteen 0x1C-byte subobjects at
`+0x2B0`, then sets vtable `0x4C85E0`, clears byte `+0x474` and dword `+0x1EC`,
and derives four scaled dimensions from record offsets `+0x08..+0x14` using
`0.4545` for the first and `0.042279862` for the remaining three. It computes
their reciprocals into `+0x200..+0x20C`, builds the diagonal matrix at
`+0x210..+0x230` from the last three reciprocals, copies that matrix to
`+0x264..+0x284`, and calls `0x43E650`. The inherited helper `0x4375C0`
initializes its vtable and fields, taking `+0x0C` from the first source-record
DWORD and `+0x08` from the other constructor argument. The eight `0x43F460`
subobject initializers write six fields each (including their ordered byte
write at `+0x0B`); the sixteen `0x43F4A0` calls are identity returns with no
observed writes. The finalizer increments the inherited 16-bit `+0x10` field,
clears its observed velocity/auxiliary ranges and `+0x470`, and leaves helper
boundaries explicit. On the ordinary path the off-diagonal matrix slots receive
zero; under the VC98 FDIV compatibility path their shared value comes from the
helper return register, which remains an explicit optional input.
`plan_ncar_body_constructor` projects these writes and helper boundaries.
Evidence `ev_5ad16d1080f575ad747c1bcdb26a9b322b07a5d7407c79517ec899d09c08d4fa`,
`ev_0b042fb4f6d77c50d28d57ecd3312bc6061e230cae69e8d682b71b1aa2b17a26`,
`ev_ac9afffb7afcffe36e6d413ee5c362325f9a87a56808788e5bd0a17432fc241f`,
`ev_aca9b2f651bc44518c585436809f7442545c9924c8390f910def2f7422fb5ec3`, and
`ev_52c84de9710ca279934fee09bd5d66de457cd35450593accf6789cb90ebecf58`.

The shared PCAR constructor chain corroborates the source layout: `0x42FF90`
calls `0x43E490` with the same source-record pointer and constructor argument,
and its derived PCAR constructor `0x4381E0` calls `0x43DFB0`, which reaches
`0x42FF90`. The body initializer reads source DWORDs `+0x08..+0x14`, so
`make_ncar_body_constructor_observation_from_runtime_record` now adapts those
four floats and the source word at `+0x00` from a bounded runtime record. The
second constructor argument remains caller supplied. This adapter does not
instantiate the PCAR object or imply that runtime physics is initialized; the
remaining constructor resources, callback wiring, and persistent body state
still need to be connected. Evidence: `0x4381E0`
(`ev_a6fffec1a3d0d9b9d8808f8e4c74b4a5becf32bd64dc6d925d28250c80f84b28`),
`0x43DFB0` (`ev_e7d523e8b77f8fc1d8059a4b99b406d1d2e609dc2341e5e700baf34d4440f375`),
and `0x43E490` (`ev_429bba68149b6bd6316a116c77c7c0781a4d72f95fa6f80312b519ed161ce3b3`).

The base car constructor `0x42FF90` builds five resource-group vertex maps.
Group 0 maps each vertex to its own truncated 16-bit index. For groups 1–4,
each vertex maps to the nearest group-0 vertex by squared XYZ distance; the
search uses a strict improvement test and a `100,000,000` initial distance
limit, so ties keep the first match and candidates at or above the limit leave
index zero. `plan_ncar_vertex_maps` reproduces this mapping with bounded vertex
spans and reports incomplete input coverage. Allocation success and original
x87 rounding are not emulated. The constructor then calls `0x433C00`, which
calls
`0x441C10` four times, starting at car-object offset `0x554` and advancing
`0x1A4` bytes per wheel subobject. `0x441C10` stores the allocated tire
response object at wheel offset `0x8C`.

The same constructor initializes embedded defaults before configuring the
vehicle. `0x43F7D0` clears five DWORDs at `+0xC7C`, `+0xC80`, `+0xC84`,
`+0xC88`, and `+0xC94`. `0x43FA30` initializes the selector at `+0xC98`,
including its `0x43F8F0` base fields and selector-relative `+0x68 = 4`. `0x43F6D0` then
initializes three seven-DWORD state blocks at `+0xD04`, `+0xD24`, and `+0xD44`
with vtable `0x4C8768`. `plan_ncar_constructor_subobject_defaults` records their
ordered absolute field writes. Evidence `ev_65ae89a16cc6388298e63e4567d2149cfc732cb3781bdf7034580cd50b06a266`,
`ev_9a82f989670fc71e23178542fe9d9779edcd836aa7e8f7dc7a82e36d2626eff3`,
`ev_73f5b6ed9af9ed2d9eff36b17ac24ee521542e92d28b3d67fef257d25cc7f571`, and
`ev_baa76dcbd6acd6420ebde137d4df5a9632759769c9a2c6679053d1a1fdca16d2`.

`0x42FF90` also calls `0x4419A0` for four wheel slots at `+0x554 + 0x1A4*i`.
`plan_ncar_wheel_constructor` records the ordered per-slot writes, including
both vtable assignments, the `+0x18C = 1800`, `+0x194 = 290`, reciprocal at
`+0x190`, constants at `+0x34`, `+0x38`, `+0x5C`, `+0xB8`, `+0x114`, and
`+0x188`, and the zero fields. The ordinary path fills helper-dependent values
with zero; compatibility mode preserves an observed helper EDX value or lists
the affected offsets as unresolved. Evidence `ev_acb07d5cd1487043da9c665197a9902d94a44439d68a4c5285b4feeabba1255c`.

The constructor-default projection also includes the `0x440100` curve-object
defaults and the direct tail of `0x42FF90`: root vtable, mode/default fields,
the eight zero DWORDs beginning at unaligned `+0x48E`, and the source-record
word `+0x65` copied to `+0x510` and `+0xD78` when supplied. That source word
remains an explicit optional input. Sound-object allocation, resource lookup,
and filename resolution remain runtime inputs to `plan_ncar_sound_constructor`.
That planner records the seven factory results in their car slots, the six
ordinary/shift sound objects' `+0x10` owner writes, and conditional shift-sound
updates at object `+0x30/+0x34` and `+0x08/+0x2C`. Returned handles and the
preexisting shift fields must be supplied; unresolved ones are reported.
The wrappers allocate `0x40` bytes before `0x46C080`/`0x46BE60` and `0x3C`
bytes before `0x46C030`/`0x46BF60`; both reject and release a nonnull object
whose resource handle at `+0x0C` is zero. The engine-sound wrapper allocates
`0xF8`, initializes seven embedded sound slots, and rejects/releases its result
when the parsed/loaded slot count at `+0xF4` is zero. Resource file contents,
allocation outcomes, and callbacks remain inputs rather than guessed state.
Evidence `ev_fb568c133bd592fd606067f6b5921d7536ad52e13ad301ad1358df8c8efafbb4`,
`ev_e63422f6b69cb8cfbe5794c88c36e8f0248ae5a054a4519fe73adc16c65c73cb`,
`ev_48e21344f4b98a407c1a5e6228080475bf6cf3f7b24b653b95c865d050c90e34`, and
`ev_919704cbfe71304dc119f67fa5f2a8498870e9aa7e592b6e2531f21cae79ade1`,
`ev_b4e45adc8ddb7cac5d7469bf678012b12680fb0af2909e81d1382275c11bca56`,
`ev_bfeac000c13a89849a50710fcc703a937a6356c61d66c792c398611e43e5aa12`,
`ev_9b6c4bbcae1a3860704fcd8427642d9f5fe1d156ba33b0d64beb765650471d7b`, and
`ev_89e24ce1c4e9e6d9eae23c9b0297e9ec22e14f1a9fa41ef37cb7e90098e895aa`.

The sound-manager calls in `race.bin` resolve the previously missing engine
resource-selection path. `0x46C410` first requests a car-specific engine setup
resource using type code `0x45534658` (`ESFX`) and name `%se.ens` (for example,
`vipere.ens`). If that lookup fails, it reads `engine.txt` and constructs up
to seven `engine%d.sfx` slots from its seven-float rows. With the setup
resource present, it requests `%si.sfx` for the idle sound (for example,
`viperi.sfx`) and `%s%d.sfx` for each listed engine band (for example,
`viper0.sfx`), falling back to `engine%d.sfx` when the named band file is
absent. Each
sound object is made by `0x46C080`, which allocates `0x40` bytes, constructs
the `0x46BE60` object, and rejects it if its resource handle at `+0x0C` is
zero. The engine manager stores the number of successfully loaded slots at
`+0xF4`; its wrapper rejects the manager when this count is zero. This
recovers selection and lifetime gates, not the `.sfx` sample codec or audible
playback. Evidence against shipped `GameBin/race.bin` SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`:
`ev_95ac1d7d4a2c2f609f292ff4acb2474fdb3a8f68663b954e3c548f99976b729c`
(`0x46C410`), `ev_c185d0a19fe0a146190bc9217ba166579a6b919f7bddeb39ea811fe23a1f9b63`
(`0x46B9E0`), `ev_b4e45adc8ddb7cac5d7469bf678012b12680fb0af2909e81d1382275c11bca56`
(`0x46BE60`), and `ev_ac73f4a64c8db118ba3572f6d79b8454bd109e04b0fc11041ec614055ec5e385`
(`0x46E6A0`).

The `ESFX` payload begins with a DWORD band count followed by seven-float
records. `parse_engine_sound_setup` bounds the count to the engine manager's
seven slots, checks every used float is finite, and retains bytes after the
records without assigning them meaning. All five car bundles contain one
setup; Viper has three bands and 140 trailing bytes, while the other four have
two bands and 168 trailing bytes. Native `0x46C410` copies the seven
record values into manager state in this order: fields 3, 4, 5, 6, 1, 2, 0.
The update routine `0x46C7A0` obtains two values from the vehicle object and
passes the first through `0x46CA10`; that function uses per-band record data
for sound gain and playback frequency. The runtime has not connected its own
vehicle RPM/throttle state to these native inputs, so DirectSound band
selection remains provisional. Evidence IDs:
`ev_f5ee7b4ebd6c8e0d68ffdb0224d85bf883046a27e5b16642e7fb67ac89e34236`
(`0x46C410`), `ev_f86b4719b9039a7866207664763dbe8a6fbda6300526ef0b1ab25dc20f5de927`
(`0x46C7A0`), `ev_92cd5da97ed44864efc27dc372f8558d6815fc2f90583ddc019bf39e83564583`
(`0x46C930`), and `ev_f9144a31f0be970da7eb404b2cafaf57105061875543db365173f22a99503b36`
(`0x46CA10`).

The selected player car's `SFX0` (`0x53465830`) entries contain a 20-byte
header: sample byte count, PCM format tag, channel count, sample rate, average
byte rate, block alignment, and bits per sample. `parse_pcm_sfx0_resource`
accepts only consistent integer PCM headers and returns the declared sample
span separately from remaining opaque bytes. Every `SFX0` entry in the five
shipped car bundles passes the check; each has a 4,098-byte opaque tail.
`viper.car` is tied to SHA-256
`840AE64A76DAB82D81CDCDD91547664CCAC25FD265A21220CAC4F43A4F8D175A`. The
current Win32 runtime creates DirectSound PCM buffers for the selected car's
idle and available numbered engine samples, loops them, selects a sample band
using provisional speed/throttle thresholds, and changes pitch with speed.
The `vipere.ens` seven-float setup values are not yet parsed or used by the
runtime. DirectSound startup and buffer-play calls succeeded in a 30-frame
Viper/Bemidji audio smoke run; audible output, native band mixing, other sound
classes, and original-versus-reconstruction capture remain unverified.

The car vtable at `0x4C7F20` has `0x431150` at byte offset `+8`. The entity
iteration in `0x45BD20` dispatches the `+8` method for each listed entity. In
that car method, `0x431150` calls `0x442530` four times, advancing by `0x69`
DWORDs (`0x1A4` bytes) for each wheel. The wheel routine reads the tire
response pointer at wheel offset `0x8C`, evaluates its stored response
coefficients, and sends derived vectors into car-body routines including
`0x43E6F0`, `0x43E830`, and `0x431130`. `0x43E6F0` accumulates a vector in the
body force fields and adds the moment `(application_point - body_center) x
vector` to the torque fields. `0x43E830` accumulates a scaled vector and its
moment, plus the scale value in a scalar accumulator. This establishes a
concrete asset-to-wheel-to-rigid-body update path. The binary's individual
wheel vector components and tire coefficients still lack reliable physical
labels.

The `0x443F60` call after contact/collision handling and before tire-response
assembly is modeled by `advance_wheel_animation_basis`. It computes a side sign from `+0x04` and
`+0x14`, updates angle `+0xD4` using the damage-flag-dependent formula, stores
its sine/cosine at `+0xB0/+0xB8` and zero at `+0xB4`, then updates `+0xD8` and
the three body-relative basis values at `+0x110/+0x114/+0x118`. The nine body
basis inputs and wheel orientation values retain their raw offsets; physical
names are not inferred. The x87 trig and extended-intermediate path is
approximated with host `long double` and libm. Evidence: `0x443F60` pseudocode
`ev_097bb58840651e249e79c5862638c0311b95dc36d775ac33c682e857e5f6230e` and
instruction listing `ev_52d9ccc4a378e0ed9af396c825a67d0b14fa19ff4f652e9e6970793ba9b5ef01`.

The separate `0x443F10` timer routine is modeled by
`advance_wheel_animation_timer`. Given the value returned by `0x4257B0` and
the caller parameter, it computes and temporarily stores `+0x198`, advances
`+0x194` using `+0x190 * +0x198`, then clears `+0x198`. The four-wheel caller
`advance_four_wheel_animation_timers` applies this to four records in order,
matching the `0x1A4` stride loop in `0x4323E0`. Its trailing `0x440440` call
is modeled by `advance_vehicle_animation_channels`. The combined
`advance_vehicle_animation_frame` preserves the full caller order: four wheel
records, then the vehicle block. The vehicle helper updates two animation
channels `(rate, scale,
elapsed, pending)` at `(+0x18,+0x20,+0x24,+0x28)` and
`(+0x5C,+0x64,+0x68,+0x6C)` relative to the block at vehicle `+0xBFC`, clearing
each pending accumulator after folding it into elapsed time.
Evidence: `0x443F10` assembly `0x00443F10..0x00443F5D` and `0x4323E0`
assembly `0x004323E0..0x00432418`, plus `0x440440` assembly
`0x00440440..0x00440491`.

### Terrain contact and normal response

The wheel routine `0x442530` conditionally calls `0x45F760` with a wheel
position. That helper calls `0x4672B0`, which quantizes the horizontal query
coordinates to `0.001`-unit increments, looks up a terrain plane, and computes
the corresponding vertical coordinate from the plane equation. `0x45F760`
returns the resulting point, the first three plane coefficients, and a
surface-associated short value. The wheel code tests the dot product between
its wheel direction and those plane coefficients against `0.1`; it also
excludes surface value `0xE` from that contact-response branch. The enum's
meaning is not established.

The direct contact branch is modeled by `update_wheel_contact`. Each wheel
update first clears feature `+0x19C`; when the body query gate is enabled, the
ground query may then write its feature output there. The branch requires
query success, direction/normal dot greater than
`0.1`, and a feature other than `0xE`. On that path it may clear the feature
through the observed hook, calls `0x443F00`, and stores the returned point at
`+0x130`. It projects point-minus-query-origin onto wheel direction. In the
actual `0x442530` caller that origin is the transformed point at `+0xC8`, not
the local wheel impulse point at `+0x200`; the observation names it
`contact_projection_origin`. A
negative projection leaves contact inactive; a nonnegative projection sets
`+0x168`, scales depth by `clamp(abs(dot)*2 - 0.5, 0, 1)`, and caps the
result at wheel `+0x44 + 3`. Failed gates clear `+0x168` but do not overwrite
the prior contact point. This captures the caller's branch and stores, not
the query helper's terrain lookup or the global reset hook. The call to
`0x443F00` is a direct call to an observed empty routine and has no recovered
state effect (`ev_f8618067a91dd813de20b3cb89e18bd09a63aac451ea4e8d976525435816f29e`).

The next scalar section is represented by `evaluate_wheel_suspension`. It
subtracts the previous depth at `+0xE8`, scales travel by `62.5`, computes a
threshold at `+0x44 - 0.0254`, applies the excess-depth factor at `+0x4C`,
evaluates the wheel's `0x4405B0` piecewise response, and combines that result
with values at `+0x40`, `+0x158`, and the `+0x3C` depth product. When the raw
flag at `+0x20` is set, the `+0x3C` product is reduced by 90 percent. The
combined scalar corresponds to the wheel store at `+0x178`; field meanings
remain raw offsets.

After that base scalar, `adjust_wheel_suspension_for_surface` models the next
adjustment. The updater projects a wheel-motion vector onto body basis values,
takes the absolute projection, caps a copy at `1`, then forms
`capped_abs * (abs_projection * +0xA0 + projection^2 * +0x9C)`. A global
polarity helper supplies a sign before multiplication by the suspension
response. The resulting adjustment is multiplied by `9` for feature `10` when
the `+0x1A0` flag is clear, set to zero for feature `0`, and otherwise
multiplied by `3`; 30 percent is added back to the base response and stored at
`+0x178`. This model accepts the projection and polarity as inputs because
their upstream vector calculation and global helper have separate effects.

`finalize_wheel_suspension` covers the following common tail. A negative
response (detected with the binary's raw sign-bit comparison) or an inactive
contact clears the response and contact flag, then computes the next depth
from the previous depth, piecewise spring response, `+0x158`, and `+0x3C`.
The response is capped at `17800`, saved as the force magnitude, and multiplied
by wheel direction before the body force-at-point helper `0x431130`. The
callback `0x443F60` is modeled separately by `advance_wheel_animation_basis`;
the suspension pipeline does not invoke it automatically. Division-by-zero
and x87 rounding behavior are not normalized by this portable model.
`evaluate_wheel_suspension_pipeline` connects the three observed scalar stages
in order, while keeping contact query results, projected motion, and
polarity-helper output as explicit inputs.

`advance_wheel_update_pipeline` now composes that suspension pipeline with the
reconstructed transformed-point terrain query. It passes the clamped contact
depth and activation result into the spring stage, the post-hook contact
feature into the surface adjustment, and the wheel direction into force
finalization. It applies the final contact reset back to `+0x168`, then runs
the feature dispatch and returns its profile plus resulting animation/response
state. This follows the order at `0x0044302F..0x0044324B`. Spring/tire tuning
values, motion projection, and the global polarity result remain explicit
inputs because they come from separate wheel fields or unresolved helper
paths. The later body-accumulator call remains separate.

At the start of each `0x442530` wheel update, `+0xAC` is advanced by
`+0x124 * 0x4C88DC` and stored as a float. The stored float's raw bits are
then compared as signed integers against `0x40C90FDB`; when greater, one
`0x4C88E0` interval is subtracted and stored. A second unsigned comparison
against `0xC0C90FDB` adds one interval when the bits are greater. This is a
single wrap step, not a general modulo operation, and preserves the binary's
bitwise compare behavior for unusual values. `advance_wheel_angle_accumulator`
models this using the three constants as explicit inputs; it does not infer
their symbolic units or integrate the rest of the wheel update.

Evidence: `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`
(`0x442530` dossier), assembly `0x004425BB..0x00442609`.

`advance_wheel_update_prefix` joins that angle step to the preceding entry
operations. It stores `+0xA8 = +0x124 * +0x5C`; when global byte `0x50878C`
(returned by the trivial helper `0x425870`) is nonzero, it computes squared
distance from the point behind wheel pointer `+0x10` to wheel coordinates
`+0x14..+0x1C` and sets flag `+0x20` only when the value is greater than the
supplied threshold (`0x4C88D8`). Existing flag state is preserved otherwise.
The global and constants remain explicit inputs, so callers control runtime
state without hiding environmental reads in the portable helper.

`adjust_wheel_rate_blend` models the conditional block at
`0x00442805..0x0044286D` and is called by spin preprojection. It enters only
when `abs(+0x124)` has raw signed bits below `0x41100000` (9.0). It computes
`blend = min(1, (abs(+0x124) * -1/9 + 1) * abs(+0xE0) * 1.5)`, multiplies the
signed response local by `1 - blend`, and multiplies the original signed
`+0x124` by `1 - blend * 0.2`. This consolidates the recovered `<9` branch in
one helper.

Evidence: `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`
(`0x442530` dossier), assembly `0x00442805..0x0044286D`.

`adjust_wheel_excess_weight` models the following isolated update at
`0x004429AB..0x004429DF`. When its upstream response path was reached and
the excess local `+0xC4` passes the raw signed-positive test, it stores
`min(1, (excess * 0x4C8924 + 1) * weight_0x2C)` back to the weight local.
The upstream path flag, excess, and incoming weight remain supplied by the
caller; the prior speed and body-velocity calculations are separate. Evidence:
the same `0x442530` dossier, assembly `0x004429AB..0x004429DF`.

Evidence: `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`,
assembly `0x0044254C..0x00442609`.

For a later response branch, `0x442530` calls `0x42F100` with body position,
velocity, mass, a 3-by-3 angular-response matrix, contact point, plane normal,
and restitution `0.0`. The routine returns zero unless normal velocity is
negative and the effective denominator is non-negligible. Otherwise its
observed calculation is:

```text
n = normalize(contact_normal)
r = contact_point - body_center
q = angular_response_matrix * (n x r)
denominator = 1 / body_mass + n dot (r x q)
response = -(1 + restitution) * (n dot body_velocity) / denominator
```

Positive results are capped at `100000`. This recovered calculation is
implemented by `solve_normal_contact_impulse` in `src/vehicle_setup.cpp`. The
wheel caller invokes it only when the excess-depth response passes the target's
signed raw-bit comparison against `0x3C23D70A` and wheel flag `+0x20` is
clear. `prepare_wheel_collision_impulse` now takes that local response and
reproduces the signed-bit gate itself. It also models the strict normal
`response > 0.01` test and high-response damage hook gate (raw response bits
above `0x4534C7FF`); the hook reads global byte `0x50878C`, and a nonzero result
sets wheel flag `+0x20`. The branch thresholds and flag load are at
`0x00442F7F..0x00442FE7` and `0x00425870..0x00425875`.
The caller dossier is preserved in `metadata/wheel_collision_branch.evidence.json`.
For an accepted impulse it forms `contact_normal * response` and duration
`contact_depth * 1000`, matching the arguments passed to `0x43E830`.
`apply_wheel_collision_impulse` in `wheel_body_coupling.cpp` now joins the
response gate, conditional `+0x20` damage-byte write, and the body point-impulse
accumulator stores (`+0x2AC`, `+0x294..+0x29C`, and `+0x2A0..+0x2A8`) in the
observed order using wheel position `+0x200` and body center `+0x5C`. This names
the numerical response and data flow; it does not establish how the game's full
contact, tire, and ground models are integrated over time.

Evidence for the wheel query gate and scalar suspension sequence: `0x442530`
Ghidra pseudocode `ev_7031c5908694232f0bcb6e0042e737b8f91c507c42431b064e587de9795c5e95`
and dossier `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`.

Recovered coefficient evaluators now have portable equivalents in
`src/vehicle_setup.cpp`, along with the normal contact-response calculation:

| Binary routine | Formula | Tire-object source |
| --- | --- | --- |
| `0x435670` | `linear*x + quadratic*x*x` | offsets `0x00`, `0x04` |
| `0x435750` | `slope*x + intercept` | offsets `0x08`, `0x0C` |
| `0x435690`, `0x4356B0` | `0x435670(x) * multiplier` | offsets `0x14`, `0x10` respectively |
| `0x435760` | `0x435750(x) * multiplier` | offset `0x18` |

In `0x442530`, the quadratic result is multiplied by tire-object offsets
`0x10` or `0x14`; the linear result is multiplied by offset `0x18`. The same
wheel update also calls `0x4356D0`, which sign-adjusts and clamps an input to
the limit at `0x38` before returning `1 + input*(value_at_0x3C/value_at_0x38)`,
and `0x435780`, a trigonometric response using object offsets `0x1C`–`0x28`.
The latter is now modeled by `evaluate_tire_trigonometric_response`; its four
runtime coefficients are sourced from TIRE fields 5–8 through
`derive_tire_trigonometric_terms`. Ghidra's observed arithmetic is
`a = atan(+0x1C * x)`, `b = atan(((1 - +0x28) * x + (+0x28 / +0x1C) * a) * +0x1C)`,
then `sin(b * +0x20) * +0x24`. Host `long double` and libm trigonometric
results may differ from the target x87 `FPATAN` sequence and its optional
math-compatibility helper paths.
These transforms feed the wheel's broader computation; the names and physical
units of the resulting values remain unknown.

The curve pairs can be rebuilt directly from the TIRE fields. For the linear
pair, `0x435540` receives points
`(index 9 * 4.45, index 10)` and `(index 11 * 4.45, index 12)`. For the
quadratic pair, `0x435580` receives those same point slots from indices 13–16,
with each ordinate multiplied by `255.74712`, then divides each ordinate by
its abscissa before calculating the line coefficients. Those derived
coefficients are later evaluated as `x*x*coefficient[0] +
x*coefficient[1]`. Indices 17, 18, and 19 are the multipliers at tire-object
offsets `0x10`, `0x14`, and `0x18`. `derive_tire_response_curves` and
`evaluate_tire_response_curves` implement this complete coefficient path
without assigning physical units or labels.

`evaluate_tire_slip_curve` models `0x4356D0`: it conditionally negates the
fourth argument when the third argument's raw unsigned value exceeds
`0x80000000`, clamps the signed value to `[-+0x38,+0x38]`, and returns
`1 + clamped*(+0x3C/+0x38)`. `derive_tire_response_curves` also carries the
two source fields for this evaluator: `+0x38` is
`sin(TIRE[20] * 0.017453292)` and `+0x3C` is TIRE field 21. The sign-control
word comes from the caller's synthesized float-status bits and remains an
explicit input. The helper returns an x87 extended result in the original;
the portable API rounds it to `float`, and software division behavior remains
unmodeled.

### Wheel response and tire-curve inputs

Before evaluating the TIRE curves, `0x442530` evaluates a separate
four-coefficient response at `0x4405B0`. Its coefficients are read from wheel
offsets `0x24`, `0x28`, `0x2C`, and `0x30`; the input is a wheel-position
difference multiplied by `62.5`. The binary branches on a signed comparison
of the input's 32-bit representation, then computes and clamps the response:

```text
if signed_bits(input) <= 0:
    response = max(0, coefficient[3] - coefficient[2] * input)
else:
    response = max(0, coefficient[0] * input + coefficient[1])
```

This output contributes to a wheel response that is later passed through the
contact and rigid-body calculations. It is modeled by
`evaluate_wheel_piecewise_response`; its exact physical interpretation is
still unconfirmed.

The routine then projects body-relative velocity onto wheel-oriented
directions. One projection is divided by the absolute clamped forward speed;
another quantity is the negative difference between the wheel's stored
angular-state value times the wheel scalar at offset `0x5C` and the projected
forward velocity, again divided by that speed. These are implemented as
`derive_wheel_kinematic_ratios`. They are slip-like ratios by their arithmetic,
but their field names and sign convention remain provisional.

The point velocity feeding those projections is now isolated as
`derive_body_point_velocity_0x43e9b0`. The recovered `0x43E9B0` helper takes
the wheel point minus body center, computes angular velocity crossed with
that offset, then adds body linear velocity componentwise. In the wheel call,
the point is the world-space wheel value at `+0x130`; the body center is
`+0x5C`, linear velocity is `+0x234`, and angular velocity is `+0x240`. This
When the optional contact pipeline is enabled, the four-wheel composition
feeds the updated wheel `+0x130` point and current body kinematics to this
primitive, then projects the result onto the supplied forward axis and the
contact-normal/forward-axis cross product. Those projections replace the
caller-supplied lateral and longitudinal velocities; slip input is rolling
surface speed minus projected forward velocity. If the caller supplies the
post-preprojection wheel spin rate at `+0x124` and wheel scalar at `+0x5C`,
the composed path derives rolling surface speed as their product, matching
the source expression; otherwise it preserves the explicit rolling-speed
observation. The path remains optional
because callers that do not enable the contact pipeline still provide these
scalar observations directly.

Before this projection, `update_wheel_spin_preprojection` models the early
spin/filter block: it computes the capped response from `abs(+0x124)` and
wheel response, including the selector `+0x164` override gated by positive
`+0xE0`, clear damage, and initial `+0xA8 > 1`. The filter uses
`max(abs(+0x178), 444.99997)`, low-pass updates `+0x17C`, derives the low-speed
blend, writes the auxiliary `+0x12C` value, and damps `+0x124` below speed 9.
It also adds the `+0xE4/+0x5C` ratio to `local_90`, with the `+0x15C` /
`+0x160` gate, mode attenuation from `+0x174/+0x170`, and optional body-speed
attenuation. The four-wheel composition derives that projection from body
axis `+0x50..+0x58` and linear velocity `+0x234..+0x23C`, and sources the
prior-frame `+0x170/+0x174/+0x178/+0x17C` values from
`WheelTireDynamicsState`. It writes its `+0x17C` and `+0x12C` results back to
the wheel/timer state, and the updated `+0x124` becomes the base for the final
rate increment. The four-wheel composition can enable this block per call,
then uses the updated spin in the rolling-speed product. Extended-precision
division/rounding and subsequent contact and tire-vector updates to
`local_90` remain separate reconstruction work.

The TIRE coefficient curves are evaluated using a separate value assembled
from the contact-plane normal, wheel response, and three wheel state values.
The value is floored at `1.0` and divided by wheel offset `0x90` before it is
passed to the quadratic and linear evaluators. This identifies the curves'
input data flow; it does not prove that the value is a particular physical
load measurement. Thus `0x442530` contains at least three distinguishable
stages: wheel-position response, normalized wheel-relative velocity, and
TIRE-record curve evaluation before force and impulse accumulation.

The final tire-response scalars can be written directly from the assignments
at `0x442530`. With `t` as the trigonometric response, `r_f` and `r_l` as the
forward and lateral ratios, `n` as the ratio normalizer, `b` as the lateral
blend, `d` as the lateral direction factor, `c` as the lateral correction,
`w` as the low-speed weight, `x` as the tire-curve input, and `q_f`/`q_l` as
the forward/lateral curve values, the routine computes:

```text
forward_response = t * r_f * n * w * x * q_f
lateral_response = (t * r_l * n * b * d + c) * w * x * q_l
```

`combine_tire_wheel_response` models these two formulas. The wheel then forms
its lateral basis as `contact_normal x forward_basis` and assembles the
unadjusted vector:

```text
response_vector = -wheel_scale *
    (forward_basis * forward_response + lateral_basis * lateral_response)
```

`assemble_tire_response_vector` models that step. The original routine then
applies a basis projection using the wheel's body-space basis vector:

```text
corrected_vector = response_vector - basis * dot(basis, response_vector)
```

`correct_tire_response_projection` models this correction before the routine
accumulates the final vector and its moment with `0x43E6F0`. The basis is used
as stored; the helper does not assume it is normalized.
`compose_wheel_tire_response` now connects the recovered response factors,
contact-normal cross product, negated response-vector assembly, and projection
in the observed order. The earlier ground/contact query, curve-input derivation,
wheel animation/state writes, and force accumulator mutation remain separate
inputs or effects.

`apply_wheel_tire_basis_update` captures the following direct stores: the
scaled response vector is copied to `+0xF4..+0xFC`, and
`vector - basis(+0x110..+0x118) * dot(basis, vector)` is stored at
`+0x100..+0x108`. It also subtracts a second dot product, using caller-supplied
basis values from stack locals `+0x58..+0x60`, from response local `+0x7C`.
The input vector is the already scaled/normalized local generated by the
preceding response calculation; normalization is not repeated here. Evidence:
`0x442530` assembly `0x00443AA7..0x00443B1D` and `0x00443BBD..0x00443BEE`.
This `+0x7C` store is distinct from the tail's `local_90`; it is not the
response input to the final wheel-rate increment.

`advance_four_wheel_body_step` now combines each wheel's supplied response
factors after replacing their trigonometric term with the result of
`advance_wheel_tire_dynamics`. It constructs the lateral vector from the
contact-normal/forward-basis
cross product, forms the weighted response vector, and applies the observed
`1e8` squared-length normalization cap and `10000` vector limit before basis
projection. Its corrected
`+0x100..+0x108` vector feeds both subsequent tire-force calls. The final
wheel-rate update keeps its evolved `local_90` as a separate explicit input;
the tire-basis `+0x7C` result does not feed it. The tire-curve portion
of the factors is now evaluated from the `TireResponseCurveSet`, including the
signed/clamped forward slip correction, and call-site scales. The two
wheel-relative ratios are computed through `derive_wheel_kinematic_ratios`;
their mathematical mapping to the recovered forward/lateral factor labels
remains provisional. The same stage reproduces the projection floor at ten
times the mode scale and its low-speed weight. It also derives the bounded
direction adjustment from the quadratic correction, signed slip, and adjusted
forward response when the divisor is nonzero. The curve input itself is
computed from the three suspension-response plus stored-wheel-vector terms
projected on the contact normal, floored at `1.0`, and divided by wheel
`+0x90`. The component terms, contact normal, wheel scalar, velocity
projections, tire curves, and remaining mode scales stay explicit observations.

The factors correspond to the post-dynamics locals in `0x442530`: local `0xE4`
is the `0x435780` trigonometric result; `0x74/0x9C` are forward/lateral ratios;
`0xE8` is ratio normalization; `0xD4/0xE0/0x70` are lateral blend, direction,
and correction terms; and `0x98/0xC4/0x50/0x4C` are the low-speed weight, curve
input, and forward/lateral curve values. Their upstream derivation is still
partially supplied to the composition.

The body-step input can now enable `advance_wheel_update_pipeline` inside each
wheel iteration, after the `0x43E7F0` moment addition and before the collision
impulse calculation. Its valid query normal/depth and finalized suspension
force feed the same wheel's collision inputs and `+0xC4` projection. The
collision solver's transformed wheel application point and other body inputs
remain explicit. Disabling this option preserves callers that provide those
observations directly.

The four-wheel body step also runs `advance_wheel_animation_basis` after that
wheel's collision result and before response-vector assembly. It takes the
post-collision damage byte and, when the contact pipeline is enabled, the
finalized `+0xE8` depth. Its computed `+0x110..+0x118` basis now feeds
`apply_wheel_tire_basis_update`; wheel orientation and body-basis values remain
explicit observations. Evidence: `0x442530` calls `0x443F60` before the later
response assembly; helper dossier at `0x443F60` cited above.

With contact composition enabled, the same body step now sends the finalized
suspension force through `accumulate_ncar_force_moment` after `0x443F60`, at the
transformed wheel `+0xC8` point and body center. This is the modeled effect of
the `0x431130` forwarding wrapper to `0x43E6F0`; the transformed point is then
reused by the tire-force calls. The accumulator's observed squared-length gate
also applies to this force. Evidence: `0x442530`'s call to `0x431130` and the
`0x431130` pseudocode evidence documented in `car_update_branches.md`.

`finalize_wheel_rate_state` models the routine's exit stores: it adds
`(1 - blend_local_0xE4) * wheel_0x38 * response_local_0x7C * c` to `+0x124`,
stores `+0x128 = +0x124 * c2`, and ORs animation flag `0x40` when byte
`+0x20` is nonzero. The four-wheel composition supplies the collision-updated
damage byte and basis-update response scalar; coefficients, current rate,
blend, wheel scalar, and existing animation flags remain explicit inputs.
Evidence: `0x442530` assembly `0x00443E6C..0x00443EAF`.

The first tire force application is now connected across modules by
`accumulate_wheel_tire_body_force`. It passes the projection-corrected
`+0x100..+0x108` vector, transformed application point `+0xC8..+0xD0`, and body
center `+0x5C..+0x64` to `accumulate_ncar_force_moment`, matching the call to
`0x43E6F0` at `0x00443BBD..0x00443C09`. That shared helper retains its observed
near-zero vector gate and force/moment accumulation.

`accumulate_wheel_secondary_body_force` connects the next unconditional
`0x43E6F0` call. It scales the supplied vector by three ordered factors, then
negates it; its application point is the weighted sum of the transformed
wheel point and body center. It passes both to the same NCAR accumulator, which
computes the threshold gate and moment. The raw locals and weights remain
caller inputs. Evidence: `0x442530` assembly
`0x00443C1B..0x00443CBC`.
`accumulate_wheel_body_force_sequence` now applies both calls in their observed
order against one mutable `NcarForceMomentAccumulator` and reuses the same
transformed point and body center for both.

`advance_wheel_tire_accumulators` applies two additional persistent writes
after the forward/lateral response locals are formed. It updates `+0xA4` from
the previous accumulator plus scaled forward response, divides by
`1 + +0x11C` and stores that intermediate, then adds the signed lateral term;
the sign comes from the unsigned raw-bit comparison of wheel `+0x04` against
`0x80000000`. It also updates `+0x10C` as its previous value times one global
coefficient plus forward response times another. All four global coefficients
and incoming response locals are explicit inputs. When contact byte `+0x168`
is set, the helper also writes `+0xEC = local_0x38 / local_0x1C` and
`+0xF0 = local_0x2C / (local_0x48 * local_0xC0)`. It reports whether the
Pentium-FDIV compatibility path is active; host division only approximates
that path's result. Evidence: `0x442530` assembly
`0x004438F3..0x0044399B`.

The four-wheel composition can now execute this accumulator stage directly
after the per-wheel tire-dynamics update and before tire response vector and
body-force application. When contact updates are active, it takes the current
`+0x168` byte from the updated wheel contact state and `+0x11C` from the
updated tire-dynamics state. It now also derives the accumulator's forward
`local_0x38` and lateral `local_0x2C` values from the evolved tire factors,
using the newly smoothed `0x435780` trigonometric response. Previous accumulator
values now live in `WheelTireAccumulatorState` and are carried from the prior
four-wheel result into the next call. Global coefficients and denominator
factors remain supplied observations. These locals are formed directly before
the accumulator writes at `0x004438F3..0x0044399B`.

`advance_wheel_tire_dynamics` now models the stateful smoothing immediately
before the `0x435780` call. It can apply the preceding invalid-response reset,
stores the instantaneous component at `+0x180`, smooths the values into
`+0x174` and `+0x170`, computes the combined magnitude, and writes the
contact-gated nonnegative response scalar to `+0x11C`. The low-speed branch is
preserved as the raw signed-bit comparison against `0x41200001`; its smoothing
weights are `0.6/0.2` below that gate and `0.4/0.0` otherwise. The resulting
magnitude is passed into the recovered trigonometric tire response. Inputs
remain named by stack-local or wheel offsets where their physical roles are
not established.

`classify_wheel_contact_feature` models the following contact-feature switch
in `0x442530`. It preserves raw feature codes, strict signed-bit gates at
response thresholds 0.7, 0.9, and 1.5, the resulting animation bytes at
`+0x120/+0x121`, and the two local response scalars. Codes 10, 11, 13, and 15
select `(0.8, 2.5)`; code 21 selects `(0.9, 0.2)` and forces flag `0x24`;
other nonzero codes use `(0.9, 0.1)`, subject to the observed flag gate.
When the preceding response reset occurred or contact is inactive, the two
animation bytes are cleared and the result signals the caller to clear fields
`+0xEC`, `+0xF0`, and `+0x11C`. `apply_wheel_contact_feature_profile` applies
those direct stores to an offset-based state record without assigning semantic
names to feature codes or response scalars.

Evidence: `0x442530` Ghidra pseudocode
`ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`.
Evidence for the trigonometric evaluator: `0x435780` pseudocode
`ev_f50c5fd32adf872f989a5e5484245372b1e8f2bf7d9a371a736267dcda8c5f1a` and
instruction listing `ev_aa8c029e1574d2f94e9b741fbe691b0fde26183f0541b70d819e9917dc01e6f6`.
Evidence for the signed clamp is `0x4356D0` pseudocode
`ev_c7962c6961aa00c8910125da95ccabd93a31a36f9fcc7fdfe0ed72ce8da9897f` and
instructions `ev_432aedb741de965c262134b0feed88a0117c4ceb41332ab5a713b29984642990`;
the initializer's angle conversion and setter calls are in `0x435BF0`
pseudocode `ev_84db288b1638262730a33555ba36d246fbf7b66ee739b36b2b4863e7fce55332`.
The smoothing and `+0x11C` state writes are also observed in the `0x442530`
pseudocode cited above.
The multiplier wrappers are confirmed by `0x435690`
(`ev_d5c3ea94a8ef57143af66178fe81589900ead9393705225ab54753ed0198140b`),
`0x4356B0` (`ev_2b4749e729e8d04414093122a794ee6e51ae597c390e76dc7b3333f6160afa6e`),
and `0x435760` (`ev_6d03387b2986298f6e12f91296344da99a9140cc6b995ea3e0a3d92051ae10b2`).

### Car update order and force accumulator consumption

The active car callback `0x431150` performs four calls to `0x442530` in a
wheel-stride loop before calling `0x43ECE0` on the car. The loop advances its
wheel pointer by `0x69` DWORDs (`0x1A4` bytes), matching the wheel subobject
stride established during construction. Thus the four wheel updates accumulate
their results before the same callback enters the body update routine.

`advance_four_wheel_contact_updates` now applies the recovered contact,
suspension, and feature-state projection to four bounded wheel snapshots in
that same order. It returns the post-update contact states, each wheel result,
and byte offsets `0`, `0x1A4`, `0x348`, and `0x4EC`. This composes the modeled
contact portion of the callback only: tire dynamics, the remaining `0x442530`
branches, and its body-accumulator effects are not inferred by this helper.
`apply_four_wheel_collision_impulses` composes the separately reconstructed
collision gate for each wheel in the same slot order, carrying each wheel's
damage byte forward and accumulating accepted impulses into one shared body
point-impulse accumulator. This links the four per-wheel `0x43E830` writes to
the `0x43ECE0` duration-normalization stage below; contact depths, normals,
response matrices, and initial wheel flags remain supplied state. The gate and
accumulator call are documented in `car_update_branches.md` and
`wheel_collision_branch.evidence.json`.

The `advance_four_wheel_body_step` composition now also accepts each wheel's
already-composed `WheelTireBasisUpdateResult`, its `+0xC8` application point,
and the secondary-force inputs. It adds the two consecutive `0x43E6F0` forces
per wheel after that wheel's collision impulse, using the existing tire-vector
and secondary-force models. The `0x442530` dossier shows the initial collision
impulse call before these two force calls. Their exact local inputs remain
observations. It also models the conditional third `0x43E6F0` call: wheel byte
`+0x168` must be nonzero and car mode `+0x4F4` must equal zero; the force uses a
capped `(tire scalar - 0.5) * 0.5` weight, projects the corrected tire vector
onto the car's `+0x38` axis, scales by an explicit response factor, and applies
a velocity-aligned correction only when body velocity squared is strictly
above 0.1. It accumulates at the body center. The response factor and wheel
scalar remain caller observations in the standalone helper. The integrated
four-wheel path derives its feature gate from contact byte `+0x168`, the
traction limiter from raw feature word `+0x19C != 0`, and the multiplier from
the function's constant local `1.0`. Its tire scalar also comes from the first
float of the wheel's `+0x8C` tire object, which is the quadratic coefficient
written at tire-object `+0x00` by `0x435580`; the composition therefore takes
`TireResponseCurveSet::quadratic.quadratic`. Only the standalone force helper
requires the caller to provide this value. Evidence: `0x442530`
`ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7` and
the TIRE initializer/setter mapping above.

The four-wheel body-step API can now accept the four raw parsed `TireDefinition`
records directly. With `derive_wheel_tire_setup` enabled, each wheel derives its
`TireResponseCurveSet` through the recovered `0x435540`/`0x435580` setters and
its four trigonometric terms from TIRE fields 5–8 before curve evaluation and
wheel dynamics. The resulting curve sets are exposed in the step result and are
also used for the conditional tire-force scalar. This connects the 120-byte
TIRE resource decoder to the already-composed wheel update; tire-file selection
and loader side effects remain outside this step.
The direct tail after that conditional call is also modeled: `+0x120` ORs in
bit `0x40` when wheel byte `+0x20` is nonzero, `+0x124` accumulates
`(1-local_28) * +0x38 * local_90 * 0.016`, and `+0x128` stores `+0x124 *
9.549296`. When spin preprojection is enabled, the four-wheel path carries
the damped `+0x124` rate and computed `local_28`/`local_90` into this tail.
Otherwise it accepts an observed preprojection `local_90`. It subtracts the
capped response vector's dot product with the wheel-forward axis before
applying the rate update. The resulting local is exposed in
`wheel_rate_response_local_0x90`. The standalone
spin helper still accepts the projection explicitly; the four-wheel path
derives it from body state. Extended-precision behavior remains approximate.
The same function dossier supports these stores.

`advance_four_wheel_body_step` now composes those four collision gates with the
`+0xD78` eighth-call gate and its `0x4323E0` four-wheel/vehicle animation
updates, then the body's counted embedded-record batch, gravity and
point-impulse force collection, per-record callback gate and timeout effects
from the duration-gated virtual `+0x38` calls, and
linear/angular/orientation/basis kinematics and tail clears.
The incoming `+0x24C/+0x258` force/moment values seed the shared accumulator;
the embedded-record rows update that state and energy before the body consumes
it. This connects the reconstructed wheel and terrain-contact effects to the
body update in executable order. It does not derive wheel collision inputs
from the rest of `0x442530`. Exact x87 arithmetic and the unresolved local basis
stack words also remain noted below.

The body-step composition now reuses its current kinematics for the remaining
body-force branches: mode force gets current linear/angular velocity and center,
common and velocity forces get the current center and velocity, and attached
object forces get the current center. When the wheel contact pipeline is active,
the resulting `+0x168` bytes feed the mode, common-force, and post-body contact
gates; the conditional tire force also consumes the current contact byte and
body motion. The counted terrain-backed embedded-record pass now receives the
same pre-integration body position and velocities. After integration, the
post-body tail receives the updated angular and linear velocities. Axes,
transformed helper results, material coefficients, and callback mutations
remain explicit inputs.

When the full body-step composition runs, the collision solve now takes body
center and linear velocity from the same `BodyIntegrationKinematicsInput` used
by the body update. With contact updates enabled, it also takes the contact
point from the resulting wheel `+0x130` state, including the retained prior
point on contact misses. This matches the shared body/wheel objects in the
`0x442530` call path and avoids stale duplicate snapshots. The angular response
matrix, body mass, and response threshold remain explicit observations.

At the start of `0x43ECE0`, the vertical component at body field `+0x250`
(DWORD index `0x94`) receives `field[0x7c] * -9.81`. The routine next visits
`field[0x7b]` embedded records, each 12 DWORDs apart, through `0x4473D0`.
If the scalar accumulator at DWORD index `0xAB` has magnitude above the
single-precision epsilon `1.1920929e-7`, six values at indices `0xA5` through
`0xAA` are divided by that scalar and multiplied by `62.5`; they are added to
the six running values at `0x93` through `0x98`; only on this gated path are the
six source values cleared. The duration at `0xAB` is cleared whether or not its
magnitude passes the gate. This maps the first three accumulator values to running linear
force and the next three to running angular moment. The new
`consume_body_point_impulse_accumulator` reproduces this transfer and buffer
clear. Under that same duration gate, each of `field[0x11c]` seven-float
records starting at index `0xAC` has its first three components divided by the
same scalar and is passed, with the following three components and seventh
scalar, to the car's virtual method at vtable offset `+0x38`.
`materialize_body_integration_virtual_record_calls` decodes those callback
arguments from a bounded packed span and reports missing seven-float rows. The
shipped PCAR/NCAR vtable slot `+0x38` resolves to `0x430BF0`. That callback
checks global gate `0x425870`, then compares the normalized first vector's
squared magnitude strictly against `550069.4`; on success it sets `+0x48C` and
`+0x488` from `0x425790`, builds a type-5, 14-byte event through `0x4273C0`,
and reaches the secondary virtual wrapper `0x427E00` / target `0x430610`.
The type-5 payload is now encoded: bytes 0-2 hold the three local-position
coordinates divided by 10, converted with x87 `__ftol`, then shifted right by
8; bytes 3-8 hold the three transformed force coordinates divided by 30000
and stored as little-endian 16-bit values; bytes 9-12 are zero; byte 13 is the
vehicle ordinal. The conversion values, body transform, and compatibility
divider are represented in `apply_ncar_integrator_virtual_record_callback`.
The `+0x3C` wrapper forwards the force and position pointers and passes zero
for both target flags, enabling vertex displacement without sign reversal.
`apply_ncar_integrator_mesh_response` now models the secondary handler's
mesh deformation when its resource views are supplied: it finds the nearest
0x20-stride vertex to the adjusted impact position, offsets the focus by
`0.48000002` along normalized force, and applies the capped displacement to
vertices within strict squared radius `0.36`. It performs two UV-band passes,
`[0,0.75)` and `[0.75,1)`, computes the `+0x18/+0x1C` rectangle bounds for the
metadata index interval, and calls the `+0x48E` mask projection for both bands.
Afterward it copies each group's indexed main-mesh XYZ coordinates and models
the main plus five group descriptor refreshes that clear `+0x15`. Vertex/group
spans and descriptor pointers remain explicit caller inputs; out-of-range
indices are reported as unprocessed rather than dereferenced.
Evidence:
`0x430BF0` `ev_87738c6e1979745e751f9357b19ee919aadf3b5e649373d76b826a4db0238731`,
event enqueue `0x4273C0` `ev_4b5569385e553dacbb0ddc508b7d9467e3208f081cc0b277ac6b27ad9a05d436`,
and nested handler `0x430610` `ev_1d43c9b43216ba441eb9a29e9156ce92b767363305dda9c9f0449178b15faca5`.
Wrapper bytes `ev_06388009b91840790c955a840d7f13b192b6081e6e6cc9598e894fdefc18ddc6`
confirm both target flags are passed as zero.
Evidence bundle: `metadata/ncar_integrator_virtual_record.evidence.json`.

The nested handler calls `0x430B70` twice to OR each band's rectangular bit
range into the same `+0x48E` 16-bit mask base. `apply_ncar_mesh_bounds_mask`
models its four float boundaries after the shared `15.99` scale and `__ftol`
conversion. It applies only rows present in the supplied span and reports
out-of-range rows instead of reproducing native out-of-bounds writes. The
resource data itself must still be supplied by the caller. Evidence:
`0x430B70` `ev_ce9b5c3a5ae397c8a061c290479bb584c98eeb9454e658600384011e1ac641e0`,
scale bytes `0x4C7DA8` `ev_c3ef9ca6494f43d2e2252d646d0e6d7fc012722f81c7ae96bcf30b2863aca4bc`.
Descriptor-refresh flag behavior is supported by `0x44F750`
`ev_0e3f082206eadcd208a106544e27f61ba1c7f6e9d6dc64293634a909a82d21cc` and
`0x44F9C0` `ev_f8b14f5c934c3ebeaec98b7b4d2c274e69d0c4b6db674daed52393fbeeae7a50`.
The group view is grounded in `0x450B40`, which reads the vertex count and
pointer from the descriptor at handle `+0x10`; `0x450B20` returns that same
descriptor for refresh, and `0x450D00` is an identity wrapper. Evidence:
`ev_b913ec97be8ebfb6069ea1b20d66c6e25622fda813f9bf797aa93f3e71bafc7d`,
`ev_e847b4ac74ef670d80d95bdb26cc027edda8eb1b6c997ce12702c7793ed6e939`, and
`ev_6e135c46f0d2c230000042107b505b911a7c4088f0b59cbd12de1ec0f2c999ee`.

The embedded-record helper `0x4473D0` transforms its stored point through
`0x437BA0`, which multiplies the local point by the basis at body `+0x38..+0x58`
and adds the center at `+0x5C..+0x64`. `transform_body_local_point` now models
that mapping. Its point-velocity query through `0x43E9B0` is already represented
by `transform_ncar_vector_at_point` in `ncar_state.cpp`; it computes linear
velocity plus angular velocity crossed with the point's offset from the same
center. Once `0x45F700` finds a terrain contact, `0x4473D0` computes signed
penetration from the point delta and returned normal, then forms a spring force.
Feature `0x0E` scales that response by `0.04` and applies velocity-proportional
damping at `-80`; other features project out normal velocity and apply tangent
friction at `-3000`, capped by `1.3299999` times the projected spring force
(maximum `2e7`). The previous penetration at record `+0x1C` selects the
observed `-1` or `-0.010000001` spring sign scale. The new
`prepare_embedded_record_contact` implements these pre-callback force values;
it also reports the later effect-call predicate when the non-`0x0E` friction
force magnitude exceeds `50000`. Terrain-query wiring, the car's virtual
`+0x34` force mutation, and the effect routine's global throttling and event
emission remain separate stages.
`make_embedded_record_contact_input` connects a successful `TerrainSurfaceQueryResult`
to the contact-response model, carrying the sampled point, plane normal, and
surface value into the recovered force calculation. The query position and
record/body transforms remain explicit caller inputs. After the virtual
`+0x34` callback, `0x4473D0` stores the callback-mutated force X at record
`+0x1C`; on a query miss it clears that field. Both branches refresh the world
point cache at `+0x0C` through `0x437BA0`. A set byte at `+0x2C` first copies
the body's center from `+0x5C` into the record's local point at `+0x20`, then
clears itself. `finish_embedded_record_update` models these ordered state
writes while taking the callback's force X as an input. The observed physics
car vtables resolve that callback: base/PCAR slot `+0x34` points to `0x430560`,
and NCAR slot `+0x34` points to a forwarding thunk at `0x43A280` which calls
`0x430560`. That target does not mutate the force vector. It conditionally adds
`|force|^2 * clamp((|body velocity|^2 - 4) / 6, 0, 1)` to the energy accumulator
at `+0xE40` when body speed squared exceeds `4`, then calls `0x43E6F0` to add
force and point moment to the running body accumulators. The new
`apply_embedded_record_force_to_body` implements both effects; its force/moment
write retains the existing near-zero vector gate from `0x43E6F0`. Evidence:
vtable slots `ev_d09467f82540fc470d62cf2e8a484ef4282c60b9bc69968dfdbed81cdd8db7cd`,
`ev_ad4ee053085fcff5549c703db4a498a6facf8787ecc70342c810e57bbd86de6f`,
`ev_3434388f663cd04bb924d978fc89360e282fe7176cda3d68946d2bb35b44fc9d`;
targets `ev_4c7c3669182d99de4ba68ea1d12c6ade7fc3d9813fcbb543bd5bece79ec1aad7`,
`ev_78011aa7f2f9c5cfc2a6718edd8710755e7c814cf96b93c47186decf3287fa90`,
and `ev_c1273cbd2377d72b7877c49fa43a075e32602b7284b8b6abb85aa25d1b2f2e79`.
`query_terrain_segment` now models the separate `0x45F700` terrain query: it
walks the partition tree from the first endpoint side, checks the opposite
subtree when the segment crosses a split, intersects candidate planes, and
accepts only intersections inside the triangle stored in the plane's trailing
XYZ values. The segment endpoints come directly from the observed callsite:
the record `+0x20` vector with Y increased by 10, and the body-transformed
record `+0x00` point. `update_embedded_record_contact_from_terrain` builds that
segment from parsed track terrain and feeds a successful intersection into
the existing force and state path. Partition recursion is bounded by the
parsed node count. Direct dossiers for `0x466EB0`, `0x466C10`, and
`0x466DB0` now anchor the zero-side branch order, ±0.001 split/plane handling,
un-clamped `[-0.0001, 1.0001)` segment parameter, and the game's three edge
cross-product tests with 0.0001 tolerance. The plane-intersection division
uses the existing `divide_with_vc98_fdiv_compatibility` model under the
provided runtime flag. The traversal counter global and runtime edge behavior
are not modeled/validated. Evidence is retained in
`terrain_segment_query.evidence.json` (`0x466EB0`:
`ev_14d1a23ebb8379159b5bcb5be0bc2a973e62a603d3ba2e27125bc942d509e3ec`,
`0x466C10`: `ev_f021390f43d6e167b9d6ae700de6c4d2a6274c851b2890490ec76aedf5c467e5`,
`0x466DB0`: `ev_a1203e291466220900cfa5f63d1c5069be5244df7b95249ca310f37900242049`).

The helper `0x436030`, reached from the large-force path, is now represented
by `plan_embedded_record_auxiliary_effect`: it emits type 1, payload ID
`0x4D2`, three copied floats, and `min(|second vector| * 0.0001, 1)`, and
reports the optional `+0x0C` follow-up callback. It deliberately preserves
neutral vector names. The observed `0x4473D0` caller pushes a zero first
argument to `0x4361E0`, while the callee's decoded body reads a pointer field
at first-argument `+4`; resolve this ABI/data-flow mismatch before composing
the full throttle and effect queue. Evidence: `0x436030`
`ev_78008bb53a1fad99000e2b17a8eb1e14d3dfe39955109758b047007d9abaded9`,
`0x4361E0` `ev_a759bb660c3cce8efbc54f6a9f6e7e3cf8943f6f1beeed577a28a7b6dcd7e0c1`,
and caller `0x4473D0` `ev_b41be4f3332134bc2982af94ada2a60bd7d1d6240c10101ad3bdceb4728c0664`.
The optional `0x435FE0` callback now has an explicit state updater:
`apply_embedded_record_auxiliary_effect` writes the cached vector at record
`+0x1C`; only a changed scale sets target byte `+0x2C` and writes target
`+0x08`; then it sets target `+0x29` and clears `+0x2A/+0x2B`. A missing
`+0x0C` target leaves all state untouched. Evidence:
`embedded_record_effect_followup.evidence.json`,
`ev_04253f3643c884dbc389a7b25838c9b860f180b996d81c377f063b4e569269d0`.
`append_embedded_record_effect_event` then serializes that effect as a type
byte followed by the 20-byte `0x4D2` payload into a caller-selected event
buffer. It reproduces the `0x4273C0` capacity check (`used + payload + 1 <=
0x600`), byte-count advance, and overflow-warning throttle (first warning or
more than 3000 ticks since the last one). The buffer slot/index and base offset
remain explicit observations; `0x426BB0` is an observed no-op. Evidence:
`embedded_record_event_stream.evidence.json`, `0x4273C0`
`ev_342f1f75db503fb6f2aa346d7f79c95cdc30f8f7dbc2970228fb48d5e2d4f8ef`,
`0x426BB0` `ev_3b1bb0f4056c75ca6e175b670c4de03dc041ad2fcf0722fd62e20e6366e0470c`.

`update_embedded_record_contact` composes the record-local point transform,
terrain-result adapter, point-velocity calculation, contact-force preparation,
resolved `+0x34` body application, and hit/miss tail writes. The exact five
stack arguments used to invoke the terrain query remain supplied as a
`TerrainSurfaceQueryResult`; the resource-backed overload now reconstructs the
terrain segment setup, though virtual force mutation/effects and runtime
comparison remain open.

`update_embedded_record_contact_batch_from_terrain` now applies that
resource-backed update to the counted embedded-record array visited by
`0x43ECE0`. It processes the minimum of the observed count and both bounded
input spans, preserves index order and reports each row's `0x30`-byte offset,
and shares the body's `+0xE40` energy and `+0x24C/+0x258` force/moment
accumulators across rows. The byte count, state records, and source `+0x00`
points remain caller inputs; truncated spans are reported without reading past
their ends. This models the loop's known record effects but not unresolved
auxiliary-event queue behavior or target-specific x87 rounding.

The car constructor `0x42FF90` calls base/body initializer `0x43E490`.
That initializer stores `param_1[2] * 0.4545` at body offset `+0x1F0`
(DWORD index `0x7C`) and stores its reciprocal at `+0x200` (DWORD index
`0x80`). The body update uses the first value for the gravity contribution
and multiplies accumulated linear force by the reciprocal before velocity
integration. This establishes the pair as a mass-like scalar and its inverse
by their data flow; their physical units are not independently confirmed.

After this accumulation step, `0x43ECE0` clears indices `0xAB` and `0x11C`.
It updates the three velocity-like values at `0x8D`–`0x8F` by the respective
running values at `0x93`–`0x95` times `field[0x80] * 0.016`. The three
angular velocity-like values at `0x90`–`0x92` are advanced by multiplying
the 3-vector at `0x96`–`0x98` by the matrix assembled from `field + 0x96` and
`field + 0x99`, then scaling by `0.016`. Subsequent code caps velocity-vector
magnitudes, advances position and orientation, and clears the six running
values at `0x93`–`0x98` before returning.

This proves the within-callback order and the observed scaling, but does not
establish the game-wide frame timestep or the physical units of these fields.
The four-wheel composition now carries the suspension surface stage's adjusted
response into tire state `+0x178` after the early spin preprojection has consumed
the prior-frame value. This follows the binary's read-then-update order; the
later tire-dynamics clear-filter branch retains its ability to reset `+0x178`.
The contact-feature dispatch also now starts from the carried `+0xEC/+0xF0`
accumulator values and prior `+0x11C` tire gate, then writes its clear-or-retain
results back into those states before tire dynamics runs. Its `+0x120` animation
flags feed the final wheel-rate tail, where the damage bit is applied afterward.
The profile's `response_scale` (`ESP+0x1C`) now supplies the `+0xEC`
accumulator divisor, and `secondary_response` (`ESP+0x78`) supplies the scale
on the second `0x43E6F0` tire-force vector. The same `ESP+0x1C` profile scale
sets the longitudinal reference-speed clamp to `scale * 10`; it is kept
separate from the other curve mode scale used by the response evaluators.
The clamp's sign selection follows the source's signed comparison of the raw
longitudinal float bits against zero, including its behavior for negative and
unordered encodings.

The optional entry-prefix stage now runs first in the per-wheel composition. It
derives `+0xA8`, advances the wrapped `+0xAC` angle, and carries any distance
gate damage flag into spin preprojection and collision handling. When spin
preprojection is enabled, the prefix reuses its raw `+0x124` rate and `+0x5C`
scalar; the resulting stored `+0xA8` is also consumed by its selector gate.
When the contact pipeline is enabled, the prefix takes its distance-query
reference and wheel position from the same point-transform input. The global
query gate and distance threshold remain explicit.

The constant `0.016` is applied once per invocation of this body update; the
measured clock deltas are not passed into it. The linear velocity expression
is implemented by `advance_body_linear_velocity`. Immediately after the
increment, the routine tests the squared magnitude of indices `0x8D`–`0x8F`;
when it is greater than `90000`, it scales those three values to magnitude
`300`. It then advances position indices `0x17`–`0x19` by the capped velocity
times `0.016`. `advance_body_linear_step` models this force increment, cap, and
translation in sequence. Evidence: `ev_a39a0df0b76df8e646f3d5399b4c39e51d6d36c1d3e47868ce2cadb111123100`
(`0x43ECE0` pseudocode).

The same function transforms the running values at `0x96`–`0x98` through the
matrix helper at `0x422F30`, scales the result by `0.016`, and adds it to the
angular velocity-like values at `0x90`–`0x92`. If the squared magnitude is
strictly greater than `100`, it rescales that vector to magnitude `10`.
`transform_body_angular_accumulator` models `0x422F30`'s row-vector multiply
against the nine values at `+0x264` (DWORD index `0x99`), and
`advance_body_angular_velocity` applies the add-and-cap stage. The routine then
forms a rotation vector from angular velocity times `0.016`. For angle
`<= 0.0001`, it selects identity; otherwise it normalizes the axis and builds
the transposed Rodrigues matrix used by the subsequent row-major multiply.
`advance_body_orientation_matrix` models this matrix and its right-multiplication
into the 3x3 matrix at `+0x38` (DWORD index `0x0E`). The following
basis synchronization multiplies the local 3x3 matrix by the matrix at
`+0x210`, then multiplies that result by the updated orientation matrix and
writes it to `+0x264`. `synchronize_body_basis_matrices` models these two
row-major products. `build_body_integrator_local_basis_matrix` derives the
temporary matrix from the updated orientation using the observed stack copy
and ordered overlapping DWORD moves.

The earlier interpretation that two words depended on prior stack contents
was incorrect. `0x402DD0` reserves and releases its own 0x0C-byte local area,
leaving its three caller arguments on the stack. At `0x43F2ED`, the caller
forms the copy destination while those arguments are still present, then adds
0x0C to `ESP`; together these instructions place the nine-float copy at the
same effective address later passed to the second matrix multiply. The later
loads at `[ESP+0x30]` and `[ESP+0x34]` therefore read copied matrix elements.
Instruction-order simulation of the subsequent moves yields the local array
`{m0, m3, m6, m6, m4, m7, m2, m5, m8}` from input `{m0..m8}`; it is not a
simple transpose. The prior model accepted this matrix as an external
observation, but it is now derived internally. At the function tail,
six floats at `+0x24C..+0x260`
(DWORD indices `0x93..0x98`) are zeroed and the 16-bit field at `+0x10` is
incremented. `finish_body_integration_tail` models these writes.
`prepare_body_integration_force_collection` composes the gravity term, supplied
embedded-record callback accumulator deltas, point-impulse transfer, and
duration-gated `+0x38` callback argument decoding before kinematics. It returns
force and moment accumulators for the caller to pass onward; virtual callback
effects stay explicit. `advance_body_integration_kinematics` composes the
ordered linear step, angular transform/add/cap, orientation update, derived
local-matrix construction, basis synchronization, and tail clear/counter
increment. Callback effects remain caller inputs, so this is still a partial
model of `0x43ECE0`.
Trigonometric and normalization rounding can differ from the original x87
path. Evidence: `ev_a39a0df0b76df8e646f3d5399b4c39e51d6d36c1d3e47868ce2cadb111123100`
(`0x43ECE0`), `ev_30831a54df2ae382a871251911b4ba4827d7bd1a872b386959900f129c8a4d76`
(`0x422F30`), `ev_992cd7155517f76c8cc616564326fcf41c211162a7ca6bc3c536611634a468da`
(`0x422C10`), `ev_3ef7088764745fe92c5ab7e738c9de6f50d056c32a0e93326ba4bd0f06df575f`
(`0x422C60`), and `ev_ec726d0b852826c649a449b840806c9423465dfe4a1340a71c268f97a5ee`
(`0x402DD0`). The stack-delta review uses dossier `ev_eeac45221c20dabe425f430d776ba783e9648cae4d51a76551839c7db16aa05e`,
the `0x402DD0` callee frame `ev_4e1add91036877b65abd945402943c23237a26df219cd1ad88a52fdc0a02e6f6`,
and instruction observations `ev_e207e53b6ce534f059f6388a90478b90b698217e2fed8b8c4fa465283b33606c`,
`ev_f1487fdd641a4e27217e5c271c961399d6e544a72641c71130f4cc84229e5c20`,
`ev_afc8f06c7b881d0f8068fd75fad818ef510b8f8985513c0fb95da2d6ef2e20b0`,
`ev_fd591ff032f45b611d42a016e5ab4b96edd6302f0ec26ac622769c371a8a5784`,
and `ev_82056189f255c5af6de3192f59a3763034da03f54f6bfa8c04ab93833d8387f0`;
the complete session bundle is `body_integration_basis_sync.evidence.json`.

The constant `0.016` is applied once per invocation of this body update; the
measured clock deltas are not passed into it. The outer loop's pacing and
whether it guarantees one integration per 0.016 seconds remain unresolved, so
the constant is not proof of a fixed 60 Hz simulation.

## Shared terrain surface query

The shared terrain query used by wheel contact, camera placement, and track-object queries is reconstructed in `terrain_surface`. It quantizes each input coordinate by dividing by `0.001`, adding `0.5`, converting to a signed integer with truncation, then multiplying by `0.001`. It uses quantized X/Z for a binary partition walk; each node evaluates `split_x * x + split_z * z + offset` and follows the positive side only when the result is greater than zero. A selected plane is valid only when `abs(normal_y) > 2^-23`. Height is computed from the original, unquantized X/Z as `-(x*normal_x + z*normal_z + offset)/normal_y`, and the returned X/Z remain the original coordinates. The 56-byte plane row retains its nine unused float values and signed feature at `+0x34`.

`update_wheel_contact_from_terrain` now feeds a successful terrain result into the already reconstructed contact gate and stores: the sampled point, plane normal, and sign-extended feature. The query runs only when the body-query gate is enabled. The `track.bpp` TPPB v2 resource loader, 20-byte header, 56-byte planes, 28-byte partition nodes, and child-index fixup are now modeled in `terrain_surface`; its adapter supplies the parsed arrays to the sampler. The bounded parser rejects malformed records before building spans, while the original loader reports invalid child references through its panic helper. Portable long-double arithmetic approximates the original x87/`FDIV` paths; exact rounding and the compatibility software-division implementation remain open. Evidence: `ev_02a12b56b8da3a7c9bbd70acea0ae1696edd55d1a9b5d5edc3fa23baaf6dc868` (`0x4672B0`), `ev_3b54b33fed139732b6d29cd5b02c95e11ebe5e2b455a9b2de89bc3e48b4e4ae5` (`0x467260`), and `ev_4acbfe6d2397810fe55e429197e8f54269e4440fad4631decd123f188e3a5652` (`0x45F760`).

`transform_wheel_point_0xc8` reconstructs the caller path before that query.
The wheel adds its offsets `+0x04..+0x0C` to the vector pointed to by `+0x10`,
then `0x437BA0` multiplies by the body's matrix at `+0x38..+0x58`, stores
the intermediate vector, and adds translation `+0x5C..+0x64`. The resulting
point feeds `update_wheel_contact_from_transformed_point`, which connects the
transform to terrain sampling and the contact state stores, and supplies the
same transformed point as the projection origin used by the binary at
`+0xC8..+0xD0`. The body transform is supplied as raw-offset data; the helper's
aliasing path is unnecessary for this caller because its stack input and wheel
output do not overlap. Evidence:
`0x442530` assembly `0x00442A00..0x00442A40` and `0x00442C3F..0x00442C66`,
plus `0x437BA0` assembly `0x00437BA0..0x00437C5D`.
