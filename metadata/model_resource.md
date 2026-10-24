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

The 32-byte vertex rows are retained as eight float fields. In the current
development renderer, fields 0-2 are treated as local XYZ and projected into
the top-down track view. Sample values and bounds support this interpretation,
but the exact GPU declaration, normal/UV field semantics, and original object
transform remain unresolved. Vehicle materials are drawn with temporary flat
colors keyed by group name; no texture decoding or original lighting is
claimed.

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
