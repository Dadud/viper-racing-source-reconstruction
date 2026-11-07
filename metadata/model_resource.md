# FNIM model resource reconstruction

`ModelResource` parses the FNIM payload embedded in RST0 bundles. The native
loader accepts resource versions 0 and 1. Version 0 is normalized to version 1
by clearing the observed legacy metadata bytes; the parser applies the
name-field portion of that conversion before interpreting material labels.

## Layout confirmed by native code

The 40-byte header has five count fields at offsets `0x00`, `0x08`, `0x10`,
`0x18`, and `0x20`; the intervening DWORDs are pointer slots rewritten by the
native loader. Starting at `0x28`, the sections have strides 32, 32, 8, 16,
and 4 bytes. Counts and strides are checked against the payload before parsing,
and any unaccounted trailing bytes are rejected.

Section 0 contains the vertex rows. Section 1 contains 32-byte material groups
with a 16-byte resource-name field and four 16-bit range endpoints at offsets
`0x18` through `0x1e`: first/end vertex and first/end triangle. Section 2
contains 8-byte triangle records; their first three 16-bit values are global
vertex indices. The native decoder subtracts each group's first vertex and
emits three local indices per triangle. The portable parser does the same and
checks every index against that group's vertex range.

The 32-byte vertex rows are retained as eight float fields. The renderer treats
fields 0-2 as local XYZ; sample values and bounds support that interpretation.
The separate Viper modding reference describes the same eight floats as
position XYZ, normal XYZ, and UV. UV use in the renderer is therefore
secondary-evidence-driven until corroborated against the engine's vertex
declaration or a controlled render. The original object transform remains
unresolved. The player Viper's `VIPER.tex` group samples the configured opaque
`paintN.tex`; AI Viper body groups use the native modulo-7 selector when their
profile bundle is available. Same-named material textures resolve from the
vehicle bundle first, then `race.res`. Extracted bundle hashes and TEX header
values are pinned in [texture_resource_inventory.md](texture_resource_inventory.md).

## Shipped Viper0 observations

The external `viper.car` (SHA-256
`840ae64a76dab82d81cdcd91547664ccac25fd265a21220cac4f43a4f8d175a`) has a
Viper0 FNIM resource with version 1 and a 13,744-byte payload. Its section
counts are `(325, 6, 389, 0, 0)`, and its size matches
the five counted sections exactly. The six groups partition 325 vertices and
389 triangles; all referenced indices fall in the associated vertex ranges.
The WARP development view now loads those groups and draws their triangles at
the first Bemidji ideal-line point, aligned to the first route segment. This is
a placement visualization, not a race simulation or a validated original
vehicle pose.

Evidence from the shipped `race.bin` (SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`):

- FNIM loader and section pointer fixups: `ev_14d72bf694974be6079b51b326cb1bfa5245d2be81c45423b94dfb1952167e13`, assembly
  `ev_e78a8398a6e961dbd2300cf688547b138491fc4117a32c5f51f1567347c7e6e8`.
- Native indexed geometry and group ranges in `0x44f9c0`:
  `ev_044442c81ba639e4a89bd06a3462a2607e6900698a4ab455359cc3f09270f510`.
- Index buffer allocation at six bytes per triangle in `0x44f610`:
  `ev_83f35800c89cf9ef42c9b8d1715eb53b3d48744ef399c127d1ee7f5b6a45d106`.
- Group material-name lookup and runtime binding in `0x44fbc0`:
  `ev_a3eb054e62cba83ffc061de529ce0963c4f2edbd08ca71a5cfeb8a0599a255f9`.
- Native serialization size formula: `ev_f9111254a63033a6541182dad436f048e140e7a0f41d8ad578983d01c12907fe`.

The parser's synthetic malformed-input regression runs in CTest. The local
asset validation and three-frame WARP smoke also exercise loading and drawing
the external Viper0 payload when the game install is present beside the repo.
`GameAssets` separately parses `viper0.mod` from `viper.car` for AI opponents
when the selected player vehicle is not Viper; the renderer assigns that mesh
to non-human grid rows. This validates resource loading and scene submission,
not native material bindings, model alignment, or opponent motion.

## Empty material labels

FNIM group names are stored as fixed 16-byte labels. A group with an all-zero
label is present in the shipped `plane1.mod` model, while its vertex and
triangle ranges are valid. The parser therefore retains an empty material
name instead of rejecting an otherwise structurally valid mesh. Material
resolution for that group remains an engine-level question; the development
renderer currently draws it with its fallback color.

## Material-name inspection

`viper_reconstruction --inspect-model BUNDLE RESOURCE` reports FNIM material
names, vertex and triangle ranges, and local index counts without assigning
semantic meanings to packed vertex fields. Against the external `viper.car`,
`Viper0.mod` reports two `VIPER.tex` groups, one `UCAR.tex` group, one
`WHEELS.tex` group, and two `EFFECTS.tex` groups. The same bundle contains
`viperd.tex`, `Viperd1.tex` through `Viperd4.tex`, and `Viperw.tex`; the
external configuration directory also contains paint0-8, sedan, sports, and
exotic TEX bundles. This establishes available resource names and group
references. The native alias/paint selection and supported TEX base-level
decoding are traced below. The renderer binds human and AI body paint to
`VIPER.tex` groups and resolves exact-name vehicle/shared textures for the
other Viper groups.

## Native IMG! image-resource structure and car texture naming

The shipped `viper.car` stores TEX resources as version 3 entries whose chunk
header ends in the `IMG!` resource tag. For example, `viperd.tex` has a
174,828-byte payload; `Viperw.tex` has 349,656 bytes. The standalone
`Config/paint0.tex` is 174,848 bytes total: its 20-byte prefix contains the
`RES0`/`TEX `/version-3 header, a zero dword, and the `IMG!` tag, leaving a
174,828-byte image payload. The file SHA-256 is
`3f23ee8091b6f6153f3dbeac868b9115ce3f9cfc7006f4dd116e618d898c2c6c`. That
matches the `viperd.tex` payload length, but
does not establish equal dimensions or pixel content. These are container
observations; they do not by themselves identify an image codec.

The `experiments` branch of
[HerbFargus's Viper Racing Modding reference](https://github.com/HerbFargus/viper-racing-modding/blob/e8435ddce817eb37502d1dd1080b64ee27f44515/docs/reference/file-formats.md)
provides independent byte-level TEX findings: the post-envelope 16-byte
header, flags and colorkey fields, mip count, RGB565 conversion quirk, the
full-resolution mip at the end of the chain, and a well-supported ARGB4444
path. This project uses those documented field observations as a decoder
lead while keeping the native-code path and supplied assets as separate
evidence. The four-byte-per-pixel `0x03` case remains unresolved in that
reference and is rejected by the new portable decoder rather than guessed.

The checked-in `disasm/engine_.text.asm` narrows the native path:

- At `0x449740`, the code serializes the image resource in bounded writes,
  including a 36-byte record, then writes a pixel block whose byte count is
  the product of record dwords at offsets `+0x0c` and `+0x10`. The dword at
  `+0x04` is passed as the source buffer. This classification follows the
  exact IAT mapping: `0x56D564` is `WriteFile`, while `0x56D568` is `ReadFile`.
- At `0x4495E1`, a related existing-image path checks the `IMG!` tag and
  compares selected leading bytes and dwords at offsets `+0x08`, `+0x0c`, and
  `+0x10`. If those compared fields match, it reads the computed byte span into
  the existing image buffer at `+0x04`; a mismatch skips that transfer and
  takes the changed/error return path. This is a conditional image comparison
  and transfer, not a demonstrated car-material load. The factors at `+0x0c`
  and `+0x10` are used as a storage span; their independent meanings are not
  established here.
- The listing contains the diagnostics `tmap: unknown texture format` and
  `bad texture name: %s`, but strings alone do not establish which formats or
  resolver branches are used for these car materials.
- Helper `0x45BB90` compares its supplied base-name string with the literal
  `viper` (`0x4DC294`). For that case, a negative selector is transformed as
  `-1 - selector` and formatted with `~paint%d.tex`; a nonnegative selector is
  incremented and formatted with `%s%d.tex`. Other base names use
  `~%s.tex` for a negative selector and `%s.tex` otherwise. This establishes
  the native filename templates; the model caller's supplied values are
  traced below.
- The checked-in listing has four direct calls to `0x45BB90`, at `0x41015A`,
  `0x4662A9`, `0x4961F6`, and `0x498637`. The last passes selector `-1`, so
  that call necessarily takes a negative-selector filename branch. At
  `0x4662A9`, the helper's output is stored at object `+0x1cc` and immediately
  checked by `0x417A20`; when that candidate is absent, the caller formats a
  `%s1.tex` fallback (`0x4DCE3C`) into the same field.
- Caller provenance is now resolved for this model path. The object-building
  routine `0x463A91` calls `0x465E20` with the participant row's string at
  `row+0x11` as its first argument and the DWORD at `row+0xc0` as its second;
  it also supplies a third flag argument. In `0x465E20`, those first two
  arguments remain at `[entry_esp+4]` and `[entry_esp+8]`, respectively, and
  are forwarded as the base-name and selector to `0x45BB90` (the brief stack
  adjustment around `0x44F390` accounts for the apparent reversed offsets in
  the listing). Therefore the participant row's car stem and selector directly
  determine the candidate filename checked by `0x417A20`.
- Resource resolver `0x417EF0` treats a leading `~` as a separate lookup path.
  It passes the suffix after `~` and the global prefix returned by `0x412D50`
  (global buffer `0x4EF0C0`) to format string `0x4D3784`, whose bytes are
  `%s%s`, then opens the resulting path through `0x411430`. The prefix is
  configured at runtime, so its concrete directory is not established by the
  static listing. For `~paintN.tex`, the resolved path is therefore the
  configured prefix concatenated with `paintN.tex`.
- The existence checker `0x417A20` has the matching tilde branch: it formats
  the same prefix plus suffix using `0x4D36F0` (`%s%s`), then calls `0x417C10`
  to open and close that path. Thus the formatted candidate at `0x4662A9` is
  tested through the configured external path when it starts with `~`; this
  confirms the car/model lookup connection without requiring a game run.
- Frontend race construction writes the human selector to participant row
  `+0xc0` as `-1 - GLOBAL/player_paintjob`; AI rows use `driver_order[i] % 7`
  (see `metadata/frontend_dispatch.md`). Because `0x463A91` passes that exact
  row field to `0x465E20`, and `0x465E20` forwards it to `0x45BB90`, the human
  Viper path is established statically: the helper's special-case transform
  returns `player_paintjob` and produces `~paint<player_paintjob>.tex`. The
  tilde existence check joins `paintN.tex` to the configured prefix as
  described above. This proves the native static call/data flow; it does not
  prove which runtime branch executes for a particular race setup or the
  configured prefix value without runtime observation.
- Under the context gate at `0x46653E`, the routine calls `0x447D10` with the
  selected filename stored at object `+0x1cc` and a size/count value derived
  from object `+0x1c8`, then passes the returned resource handle to `0x47EA0`
  and stores it at `+0x310`. This establishes that the selected `.tex` name is
  submitted to the generic resource loader on that branch. The exact IMG!
  native format-handler behavior and the mapping from the returned handle to rendered material
  textures remain unproven.
- The generic loader path continues `0x447D10 -> 0x453C10 -> 0x454120`; a
  cache miss enters `0x459830`. That parser obtains a data pointer through
  `0x5AC80`, reads its leading flags byte as a discriminator, and dispatches
  through
  the five-entry table at `0x459A30`. The table in the shipped `race.bin`
  maps values 0, 1, and 3 to one setup mode and values 2 and 4 to another.
  Supplied `Config/paint0.tex` has flags byte 0 immediately after its 20-byte
  `RES0`/`TEX `/`IMG!` header. The parser then calls `0x459ED0` to
  create a resource through the engine's graphics-device path. These facts
  connect the selected TEX candidate to the native resource-format dispatch,
  but the numeric type values do not yet identify pixel encoding or row
  layout; the five branches must be followed through `0x5AC80` and its callees
  before broader decoder support is implemented.

The new `viper_texture_resource` library implements bounded base-mip decoding
for TEX flags `0x00` (RGB565), `0x01` (RGB565 plus the header colorkey), and
`0x02` (ARGB4444). `viper_reconstruction --inspect-texture BUNDLE RESOURCE`
reports its decoded dimensions and metadata without launching the game. It
rejects flags `0x03`, whose four-byte-per-pixel encoding is still unknown. The
renderer loads `Config/paintN.tex` from the selected
`GLOBAL/player_paintjob` for the human Viper, parses the standalone envelope,
and uploads supported opaque RGB565 base-level pixels as an immutable D3D11
texture. It samples that texture using FNIM fields 6-7 for human `VIPER.tex`
groups; AI selectors use the entrant row's recovered `word_at_0xc0` value and
the selected difficulty/career bundle's `Viper1.tex` through `Viper7.tex`.
Other groups resolve their same-named TEX entry in the vehicle bundle, then
`race.res`. The reconstructed renderer treats flag 1's decoded binary color
key as a cutout: zero-alpha samples are discarded and write depth. Flag 2's
ARGB4444 samples use alpha blending with depth writes disabled. This follows
the two distinct decoded pixel formats, but native per-format blending and
depth-state parity have not been independently proven. The extracted
data has Easy, Medium, Hard, and Career 1-4 livery bundles; `career5.res` is
absent, so that profile retains fallback body colors. The parser copies the
wrap byte to object offset `+0x17` at `0x4598E4`; the inspected texture
creation routine `0x459ED0` does not consume it. The native state updater maps
zero to clamp and every nonzero value, including 2, to wrap; see
[`texture_resource_inventory.md`](texture_resource_inventory.md) for the
instruction-level evidence.
Blended groups are drawn in model group order rather than sorted
back-to-front. UV field order, runtime path selection,
remaining helper callers, native color-key parity, native lighting, and
independent pixel-level comparison remain open. The generic
`0x4495E1`/`0x449740` IMG! compare/write routines are still not proven to
implement this car texture path. Flags `0x03` remain unsupported until their
channel layout is established.
