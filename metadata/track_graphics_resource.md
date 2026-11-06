# Track GRAF resource reconstruction

## Current implementation

`TrackGraphicsResourcePayload` reads the `GRAF` (`track.grf`) member from the
selected track bundle. It scans for geometry chunks using the pinned
[Viper Racing Modding format reference, commit `e8435dd`](https://github.com/HerbFargus/viper-racing-modding/blob/e8435ddce817eb37502d1dd1080b64ee27f44515/docs/reference/file-formats.md):
16-byte centers, 32-byte corner records, chained 32-byte material ranges, and
8-byte local face records. A chunk is accepted only when its material ranges
chain from zero and cover every corner, and all face indices and pads are
within the documented bounds. Empty material labels are retained.

The parser copies the original payload and records the byte ranges between
recognized chunks as opaque data. It does not rewrite the GRF.
`--inspect-grf BUNDLE RESOURCE` reports geometry/material counts, opaque byte totals, and
every material's corner and face ranges. The game loader parses this resource
when present. The D3D renderer expands material face ranges to vertices, uses
the corner positions/UVs/baked color, and resolves textures in the track
bundle, then `race.res`, then `common.res`. BPP remains the physics query
source; it is used for display only when no GRF draw groups are available.

## Supplied stock-track inventory

Release `viper_reconstruction --inspect-grf <track>.trk track.grf` parsed all
eight supplied stock archives. A separate resource-index comparison found no
missing named material textures when resolving against each track bundle,
`race.res`, and `common.res`. This proves coverage of the parser's recognized
material groups and named texture entries; it does not prove all scene nodes
are rendered or that the selected pixels match the original.

| Track | Archive SHA-256 | Chunks | Corners | Faces | Materials | Named textures | Opaque bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Bemidji | `ae3cb0c9318c487f6cfe217edf65f2bbbe4b7b44de7f6bfea78e473552944464` | 324 | 5,677 | 3,865 | 446 | 22 | 26,572 |
| Dundas | `075737c9357324ab4984fcc5c157c2e0126c3ef05edb25583fb7b1673109f3c5` | 861 | 10,400 | 6,455 | 1,117 | 36 | 85,388 |
| Hastings | `4aa9e5fae7098cb6f42bbbfd75411244a59f0968ea62f2e1d3f5fafdedf49eaa` | 838 | 12,463 | 8,158 | 1,247 | 30 | 70,724 |
| Castlegreen (`heaven`) | `db19ca1046deb84d48b502060d022a075c08eccb3f2f6335537e2ef36843ce6a` | 704 | 10,767 | 7,380 | 1,064 | 45 | 61,012 |
| Rock Island (`kenyon`) | `48c0d4ce9f7a4cbf7dd52c6e2248371bf80b291075d2f06f137cde66c237825d` | 493 | 15,912 | 10,026 | 1,150 | 25 | 63,984 |
| Dayton (`limbo`) | `d2f517d8362e13cf2d60ba0602d373e23926553ad9cf9d933ccdf10ef71a1d20` | 412 | 6,533 | 4,792 | 570 | 32 | 34,528 |
| Sunset Mesa (`nfield`) | `cd37020edea0c26bf5e71b5ec9e7eee861a812c7e8a9c13365bb32d8c4a3b0ce` | 1,021 | 15,635 | 10,071 | 1,566 | 29 | 101,232 |
| Silverdale (`uptown`) | `4334fd3e4c5513bf4dd9ce3fbfdc0b2bf09450385064b7a2aa650f27b6133c7b` | 408 | 9,632 | 6,130 | 688 | 34 | 47,972 |

The linked scene graph is traversed independently from the flat mesh scan. The
parser retains each visited node's type, raw `+0x04`/`+0x08` links, type-2 LOD
center/thresholds, type-1 visibility sphere, type-4 facing mode, geometry
counts, and registered facing ID. It checks node links and inline model-array
extents. All stock graphs traverse without an invalid or repeated link. The
parser maps type-3 nodes to chunks at node offset `+0x28` and type-4 nodes at
`+0x3c`, then checks the graph's declared vertex/material/face counts against
the parsed chunk. Every active geometry node maps to exactly one chunk on all
eight tracks; the additional geometry nodes are all-zero placeholders.

Native `race.bin` disassembly changes the renderer interpretation: loader
fixup walks `+0x04` recursively and continues through `+0x08`, while
`0x00467B10` draw-tree dispatch recurses through gated `+0x08` and advances
through `+0x04`. Type-2 LOD intervals gate that branch, type-1 nodes can apply
sphere visibility, and type-4 modes are submitted in later passes or registered
for dynamic lookup. The D3D renderer now traverses the raw graph links with the
native node gates per frame and submits selected type-3/type-4 nodes in ordinary,
mode-1, and mode-2 pass order, including native 300-unit facing cutoffs and
the mode-2 0.1-unit singularity guard. It omits type-4 mode 4 from drawing, as
the native ordinary tree pass only registers those handles. Type-4 basis
transforms now apply for modes 1 and 2. Valid mode-4 IDs map to their first
active graph node; dynamic registry consumers remain unresolved. See [`native_graf_draw_tree.md`](native_graf_draw_tree.md)
for the function-level evidence and remaining implementation sequence.

| Track | Graph nodes | Geometry nodes | Empty placeholders | Active geometry = parsed chunks | LOD nodes | Registered facing nodes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Bemidji | 680 | 352 | 28 | 324 | 112 | 0 |
| Dundas | 2,060 | 915 | 54 | 861 | 362 | 170 |
| Hastings | 1,875 | 922 | 84 | 838 | 341 | 15 |
| Castlegreen (`heaven`) | 1,590 | 759 | 55 | 704 | 228 | 72 |
| Rock Island (`kenyon`) | 1,705 | 596 | 103 | 493 | 448 | 59 |
| Dayton (`limbo`) | 925 | 435 | 23 | 412 | 178 | 0 |
| Sunset Mesa (`nfield`) | 2,552 | 1,125 | 104 | 1,021 | 447 | 50 |
| Silverdale (`uptown`) | 1,276 | 465 | 57 | 408 | 274 | 47 |

The parser identifies 0 registered facing models in Bemidji, 170 in Dundas, 15 in
Hastings, 72 in Castlegreen, 59 in Rock Island, 0 in Dayton, 50 in Sunset
Mesa, and 47 in Silverdale. The format-reference facing transform is applied
to parsed facing chunks. Native mode 1 and mode 2 transforms now construct
per-node bases and apply them in the vertex shader. Visibility/LOD node
selection is applied per frame.

The native 512-slot mode-4 registry is represented as ID-to-first-node-offset
entries. `--inspect-grf` reports registry counts; all eight stock track bundles
have valid unique IDs (0/170/15/72/59/0/50/47 entries respectively) and no
duplicate or out-of-range IDs. Native `obj wobble` records pass their `+0x18`
integer to this lookup. The new read-only `--inspect-wobble-models` tool
resolved every WOBL row in all eight stock tracks to a registered graph node
(0/170/15/72/59/0/50/47); none were malformed or unresolved. This proves the
ID link and static geometry association, not the instance's changing transform;
all eight tracks also have zero duplicate WOBL model IDs.

Facing-model offsets use the reference's observed local-to-world mapping and
retain the facing ID. Packed vertex color is interpreted as `0xAARRGGBB` for
the current D3D path; that channel interpretation has not been confirmed
against the native vertex declaration. The current scene uses the reconstruction's
perspective chase camera, not a recovered native camera.

## Limits and next evidence

The parser inventories linked nodes and maps active geometry to chunks. The
renderer now applies the recovered type-1 axial visibility check and type-2
distance intervals, plus type-4 mode pass distance bounds, using the
reconstructed chase-camera position/forward and native default settings.
Camera pose parity, mode-4 instance transforms/animation, lighting, and transparent
material ordering remain open. Mode 1 assumes the reconstructed chase camera
has no roll; both basis orientations need visual comparison against the native
renderer. The new
read-only `--inspect-grf-view BUNDLE RESOURCE X Y Z FX FY FZ [DETAIL_LEVEL]`
command reports selected node counts without launching the game. Supply world
coordinates and a normalized world-forward vector.

[`native_graf_draw_tree.md`](native_graf_draw_tree.md) records the recovered
dispatcher, raw-link semantics, LOD interval formula, settings path, and native
axial depth test. Next, map the reconstructed chase camera to the native camera
matrix and reproduce mode-4 instance transforms/animation. Confirm packed color interpretation from the actual vertex
declaration and compare a controlled normal-camera capture before visual
acceptance. Both Release targets compiled with Visual Studio 18, and the
read-only inventory reconfirmed all eight stock graphs and chunk mappings. The
game was not launched and no tests were run. The view-selection inspection
completed with valid traversal on all eight stock tracks; its selected-node
counts depend on the supplied camera and are not a retail parity comparison.
