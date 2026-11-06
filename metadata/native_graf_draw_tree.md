# Native GRAF draw-tree evidence

## Evidence and scope

This note records static observations from the supplied `race.bin`, using the
local disassembly in `disasm/engine_.text.asm`. Addresses below are virtual
addresses in that binary. The observations describe native control flow and
data access; they do not claim runtime parity. The pinned community format
reference is useful for locating serialized fields, but where it conflicts
with executable behavior, native instructions take priority.

## Dispatcher and link behavior

`0x00467790` is the render-pass orchestrator. It calls the recursive scene
walker at `0x00467B10` with the root pointer held at `[0x00540AD4]`.
`0x00467B10` dispatches node types and treats `+0x08` as the recursive branch
link; after type-specific work it advances through `+0x04` as a chain.
This differs from the loader's pointer-fixup walk at `0x00467C80`, which
recurses through `+0x04` and advances through `+0x08`. The parser therefore
retains both serialized links without assigning global child/sibling names.

Type 3 nodes submit ordinary model geometry when their runtime handle at
`+0x0c` is valid. Loader routine `0x00467CC0` creates that handle when the
material count at `+0x18` is positive and stores `-1` otherwise; the serialized
word at `+0x0c` is not the runtime handle. Type 4 nodes are dispatched by mode
at `+0x38`: modes 1 and 2 are queued for later passes, and mode 4 is registered
in a separate 512-entry lookup table rather than drawn by the ordinary tree
pass. The reconstruction derives handle availability from the loader rule,
applies parent gates, and submits selected type 3 and type 4 mode 1/2 groups by
pass category. Modes 1 and 2 now have reconstructed transforms. The loader's
512-slot mode-4 ID table is reconstructed for static resource lookup; the
dynamic object constructor/draw path is identified but not connected to the
reconstruction's world-object renderer.

The later mode-1 and mode-2 passes both reject facing centers at squared
camera distance `>= 90000` (`0x00467AF0` initializes that cutoff to 300 units).
Mode 2 also rejects squared distance `<= 0.01` before constructing its view
basis, avoiding a near-zero normalization. Mode 2 normalizes the
camera-to-center vector, computes `right = world_up × camera_to_center`, and
normalizes it. If its squared length is at most `0.01`, native code substitutes
`(1,0,0)`. It computes the third basis vector as `right × world_up`, with the
model center as translation. The reconstruction feeds that basis to the D3D
vertex path for selected mode-2 groups. Treating the basis vectors as matrix
columns is still an implementation inference; validate it in a normal 3D
capture before calling the pass visually matched. Mode 1 copies the global 3x3
camera basis, negates its third vector, computes the first vector as camera-up
crossed with camera-forward,
and uses the model center as translation. The reconstruction derives the same
right/up/forward basis from its normalized chase-camera forward vector; this
assumes the current renderer has no camera roll. Static registry entries map
each valid ID to the first active mode-4 node, matching `0x00467770`'s
ID-bound checks and `0x00467CC0`'s first-entry-wins duplicate behavior.
Constructor `0x00463600` reads the ID at instance-record `+0x18`, calls lookup
`0x00467770`, stores the returned handle at object `+0x3c`, and serializes the
instance record. Its draw method at `0x00463640` submits that handle with
the object transform. Track parser `0x0045D5D0` constructs this object from an
`obj wobble` record; its `+0x18` integer therefore selects the registered GRAF
model. The reconstruction's read-only `--inspect-wobble-models` command checks
that source-record-to-registry link. The object's changing runtime transform
and wobble update path remain unimplemented.

The separate physics WOBL task constructor `0x00436ED0` uses the same record
`+0x18` as an ID into the shared WOBL resource table. Its vtable at
`0x004C8058` points slot `+0x08` to update method `0x00436F70`; during its
temporary update window this method updates task-object basis fields at
`+0x38..+0x58` and copies the resource vector at `+0x4A0..+0x4A8` into
`+0x5C..+0x64`. No direct data flow has yet been established from those task
fields to the drawable instance's matrix at `+0x04..+0x30`.

## Type 2 distance gates

For type 2 nodes, `+0x0c..+0x14` is a float3 LOD center, `+0x18` is the upper
distance threshold, and `+0x1c` is the lower threshold. The loader path at
`0x00467CC0` squares both thresholds in place. The walker skips the gated
branch entirely when the lower threshold is exactly `-1.0`; otherwise it
computes squared Euclidean distance from the LOD center
to the native camera position through helper `0x00448C60`, adds the global
bias at `[0x004DD0CC]`, and multiplies by the global scale at `[0x004DD0D8]`.
The gated `+0x08` branch is traversed when the adjusted value is at least the
squared lower bound and strictly below the squared upper bound.

The setup routine `0x00467AD0` squares its input into the bias global; the
observed default setup supplies `2`, yielding a squared bias of `4`. The scale
setup at `0x00467AE0` squares the configured detail factor. The local stock
`options.def` has detail level `1.0`, which yields scale squared `1.0` under
the observed `2 - detail_level` mapping. The renderer now reads `GX/detail_level`
and evaluates type 2 gates per frame. Its camera position still comes from the
reconstruction's chase camera, so camera parity is unverified.

## Type 1 visibility gates

Type 1 records use a center at `+0x10..+0x18` and radius at `+0x1c`. A zero
flag at `+0x0c` bypasses the sphere gate and traverses `+0x08` directly;
otherwise the walker calls `0x00448DB0` with the center and radius before
traversing that branch. The helper projects center-minus-camera onto the
native forward vector at `[0x00509B48..0x00509B50]` and accepts it when
`-radius <= depth < [0x00509B1C] + radius`. This is an axial near/far sphere
test; it does not test horizontal or vertical frustum planes. The renderer now
applies this predicate each frame using its chase-camera position/forward and
the recovered 1000-unit axial limit; camera pose parity remains unverified.

Camera setup at `0x0048940` copies camera position from its input matrix at
`+0x24` to `[0x00509B58..0x00509B60]` and its forward vector at `+0x18` to
`[0x00509B48..0x00509B50]`. The function is called from scene-camera setup
paths including `0x00410500` and `0x0045BD20`. This identifies the native camera
state consumed by type-1 and type-2 selection, but the reconstruction's chase
camera has not yet been mapped to those scene-camera matrices or conventions.

## Reconstruction status

`TrackGraphicsSceneNode` preserves the LOD and visibility parameters, both raw
links, type 4 mode, and mesh-to-chunk mapping. The stock-track inventory shows
complete loader-style graph traversal and one-to-one mapping of every active
geometry node. The renderer now selects type 3 and type 4 mode 1/2 nodes using
the recovered type-1/type-2 gates, applies the native mode-1 camera basis and
mode-2 billboard basis, then issues them in native pass categories and
graph-walk order. The mode-4 registry is represented by ID-to-node mappings,
but dynamic lookup callers are not implemented. No source
reconstruction can be called 1:1 until these rules are applied and compared
against the original in a controlled ordinary 3D view.

## Next implementation sequence

1. Map the reconstruction chase-camera pose to native matrix fields at
   `+0x18/+0x24`, then compare selected node sets against a bounded native
   capture.
2. Trace dynamic mode-4 registry consumers; verify mode-1/mode-2 matrix
   orientation and native pass order.
3. Validate the selected node set and ordinary 3D frame against the original,
   with game launches kept to explicitly bounded captures.


## WOBL visual transform handoff (2026-11-06)

The earlier statement that `0x00463600` copies the instance transform is incorrect. Its call to `0x00463510` passes the WOBL record and the address of visual object `+0x34` to the shared record serializer `0x00425720`. The visual matrix at `+0x04..+0x30` is initially identity (`0x00463440`).

The scene update loop calls `0x00463530` for its registered visual objects (`0x0045BCC1`). That method adds a supplied offset to the record buffer at object `+0x34`, reads the leading 16-bit marker, and, when the marker changes or `0x00425830` requests an update, invokes virtual slot `+0x24` with that record pointer. The WOBL visual object's vtable at `0x004C9328` selects `0x00463570`, which copies twelve DWORDs from the record at `+0x04` into the visual matrix at object `+0x04`. This identifies the render-side handoff; the producer, timing, and correspondence of those buffered records to WOBL physics state still need tracing before connecting live motion.
