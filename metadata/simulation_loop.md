# Race simulation and entity update ordering

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA.

## Race-loop caller at `0x4020E0`

`0x4020E0` repeatedly polls `0x40A160` and updates race state. On the active
branch, after `0x45BC20` and `0x448440`, it calls the entity dispatcher at
`0x45BD20`. The callback sequence that follows includes an optional
state-indexed function, `0x448450`, `0x46B640`, `0x41AEA0`, `0x425920`,
`0x425530`, and `0x4045B0`. When the render-path condition succeeds, the loop
also invokes callbacks from three state-indexed tables, acquires a render
context through `0x447C90`, and submits it through `0x45C090` and related
helpers. This is call-order evidence; subsystem labels beyond the observed
branches and callback positions remain provisional.

The routine has an initial countdown from 10 that controls calls to
`0x49BEA0`, `0x425800`, and `0x4035D0`; it is not an elapsed-time calculation.
The function's repeated work and render branch are established, but no fixed
simulation frequency follows from this countdown.

## Render context and submission

On the successful render branch, `0x447C90` obtains the render context through
`0x44E210` and installs it as the current context with `0x449900`. The context
acquisition clears its output fields, pumps messages through `0x412C80`, checks
the renderer-ready global, and calls `0x458FB0` with a `0x6C`-byte surface
description. A successful page acquisition is converted to the context through
`0x449350`; a null page pointer reaches the observed error path. Failed page
acquisition sets a retry/error flag and calls `0x44EBD0`.

`0x45C090` invokes each registered object's virtual `+0x18` method with that
context. When `0x45DAB0` is true (positive world-count global and mode 0, 1, or
2), the submitter makes additional primitive calls through `0x44C260`. That
routine walks a raster line and writes pixels through `0x449E90`, which selects
one of two pixel-store paths according to the raw context tag at `+0`. Tag 5
uses four-byte pixels at `surface_bits + y*pitch + x*4`; other tags use
two-byte pixels at `surface_bits + y*pitch + x*2`. Tag 3 selects the upper
source word for the two-byte write, while other two-byte tags select the lower
word. Tag 5 applies the observed bitfield conversion before the four-byte
store. `encode_render_pixel` and `render_context_pixel_offset` preserve those
operations without naming the color format.

After the submitter returns, mode 0 in `0x4020E0` calls `0x4498E0` to restore
the viewport to the full surface. Immediately before submission, it calls
`0x4498A0` with bounds `(left=0, top=64, right=640, bottom=480)`. The helper
clamps left/top values below 1 to zero and limits right/bottom to the surface
dimensions; it stores far-edge coordinates rather than width/height. The
resulting half-open rectangle is `x=[0,640)`, `y=[64,480)`. The helper
`render_context_contains_pixel` reproduces that containment check.
`include/viper/render_context.hpp` and
`src/render_context.cpp` preserve these numeric field offsets and setter rules.

The page-to-context adapter is now represented by
`initialize_render_context_from_surface`. In `0x44E210`, a successful
`0x458FB0` acquisition passes the returned format tag, pixel pointer, width,
height, and pitch to `0x449350`; that helper stores them at context offsets
`+0x00`, `+0x04`, `+0x08`, `+0x0c`, and `+0x10`, sets byte `+0x01` to `0x80`,
then resets the viewport through `0x4498E0`. This captures construction of the
CPU-side context while leaving DirectDraw surface acquisition and ownership as
runtime effects. Evidence: `0x449350` (`ev_4c62c3530ed4eb9919749db8d6e9f56f28d183dd940a27d68e25e59b18a23c71`)
and `0x44E210` (`ev_69e36407f8dcd985df72374cb753e2f780f22bfbebaf0c8e48e5549d6c49f5ee`).

The lock performed by `0x458FB0` is separately projected by
`plan_surface_lock`. Each supplied HRESULT represents one call through the
surface vtable at `+0x64`, with flags `0` and the caller's surface descriptor.
Success returns the locked disposition. `DDERR_SURFACELOST` (`0x887601c2`)
uses the fatal “lost surface on lock!” path; `DDERR_WASSTILLDRAWING`
(`0x8876021c`) retries immediately with no yield; other failures are passed to
the generic `0x452770("surface lock", HRESULT)` reporter. An all-retry
observation remains waiting for another result, preserving the binary's
unbounded loop without simulating an actual DirectDraw lock. Evidence:
`0x458FB0` (`ev_1130fc8f7ab56d05acea4b7b3732f09afa5b858f4eecc179b95db527144df3ab`).

When lock acquisition fails, `0x44E210` calls `0x44EBD0`, which sets the
recovery flag and attempts `Restore` on both the primary surface and backbuffer
through `0x459190`; each failed HRESULT is independently sent to
`0x452770("Restore", HRESULT)`. `plan_surface_restore` records that ordered
two-surface recovery and its reported failures. It does not claim that a
surface was restored successfully beyond the supplied HRESULTs. Evidence:
`0x44EBD0` (`ev_57a981467c870406fa93e4026960d63cd33667ef1adc17eec0f7a1e7a07ab430`)
and `0x459190` (`ev_2359f304072f9863791601c4510f9c2023c0e62ce0b4deee24717070914362c8`).

`plan_render_context_acquisition` now composes the `0x44E210` wrapper: clear
the destination context, pump messages, honor the renderer-ready gate, project
each observed lock attempt, recover both surfaces after lock failure, and
construct the CPU context after success. It keeps the lost-surface or generic
lock reporter before recovery, and orders primary/backbuffer restores with
their per-HRESULT reports. A null acquired pixel pointer takes the observed
`0x410EA0` error route. Raw helper return words and all lock, restore, and
surface-description outputs remain explicit observations.
Evidence: `0x44E210` (`ev_69e36407f8dcd985df72374cb753e2f780f22bfbebaf0c8e48e5549d6c49f5ee`),
`0x458FB0` (`ev_1130fc8f7ab56d05acea4b7b3732f09afa5b858f4eecc179b95db527144df3ab`),
`0x44EBD0` and `0x459190` above.

The successful frame tail reaches `0x447CE0`, which brackets `0x44E340`
between `0x415090` and `0x4150A0`; `0x44E340` unlocks the acquired surface
through `0x459030` at vtable offset `+0x80`. The outer loop later reaches
`0x447C50` only when the post-frame active-state check passes and the startup
counter is below 1. That wrapper pumps messages, then, when the renderer is
ready, flips `DAT_004D9CDC` through `0x459040` at vtable offset `+0x2C`.
`DDERR_WASSTILLDRAWING` retries immediately. `DDERR_SURFACELOST` reports
“lost surface on flip!”, restores primary and backbuffer via `0x44EBD0`, and
sets the recovery flag; other failures return the HRESULT with its low byte
set. The active-frame and outer-iteration plans expose these unlock and flip
edges while DirectDraw results remain observations. Evidence:
`0x447CE0` (`ev_a0b664b74e474938a1ee3101845d6890c823faf5a3d49f19e4b973c6d7b57000`),
`0x447C50` (`ev_cb4e6ac9c2cdf1774acbbe36eb3529e43228bec95480edb198a40a818189d32b`),
`0x44E340` (`ev_3ed2f0f9260845d0f4c55f5a743770ac21588f2a65c2fe7d0f08a303b16b6f39`),
`0x459030` (`ev_23ba6ac0a7fdf8417adeb186b446f428a973140d4f2325abbe4a195619f66516`),
`0x44E350` (`ev_076be94dac6ececf90795998d575dfae61f0a089d4b24ea82e50bb2764be64ca`),
`0x459040` (`ev_ab64069ee53ccd589b835cbda4ac46de366ea09a76095cfb2b2a478f13264bf5`),
and `0x4020E0` (`ev_b26e9aad81c9f9a23aa344cb2d8cd64b866103eca50f4f93b9f2e93add7bc791`).

## Entity dispatcher at `0x45BD20`

The dispatcher reads a count from `DAT_0053A4C0` and entries from the array at
`DAT_00539430`. For every registered object it first calls virtual method
`+8`. It then calls `+0x0C`; if that returns nonzero, it calls `+0x10`.
When `+0x10` returns zero, method `+0x14` is called immediately. Otherwise,
the object and the float returned by `+0x20` are inserted into a temporary
list ordered by that float. Once the registry scan ends, `+0x14` is called for
the collected objects in that order. The insertion scan advances past existing
keys until it finds a strictly greater float, so equal keys retain registry
order. With unordered comparisons such as NaN, the scan also advances past
that key; this follows the binary's raw `<` comparison rather than imposing a
total float order. `entity_update_callback_order` records the virtual call
sequence and insertion rule from supplied callback results; it does not invoke
game objects. This is a verified two-phase update path, not a class-specific
guarantee that all objects use the same vtable layout.

Evidence: `ev_aba60868872f9fd1aac31389fdbc7feab729eaf5fa982d2b68f4e30fe46ad40f`
(`0x45BD20`).

`0x4020E0` also consults the first three DWORDs of five-DWORD rows beginning
at `0x4CF5C0`, indexed by `DAT_004CF5B8 * 5`. Its call sites establish these
roles: slot `+0x08` runs immediately after `0x45BD20`; slot `+0x00` runs later
before render-context acquisition; slot `+0x04` receives the acquired context
after `0x45C090` submission. The observed rows are:

| State | Slot `+0x00` | Slot `+0x04` (context) | Slot `+0x08` |
|---:|---:|---:|---:|
| 0 | `0x4035E0` | `0x4052D0` | `0x4052E0` |
| 1 | null | `0x41A690` | null |
| 2 | null | `0x423380` | null |
| 3 | null | `0x40D570` | null |
| 4 | `0x404960` | `0x4052E0` | null |
| 5 | null | `0x422A70` | null |

`race_loop_callbacks_for_state` preserves these raw callback addresses for the
six nonempty rows. The state-1 fourth DWORD is `0x41A370`; its entry reads a
16-bit event and dispatches through two jump tables, updating the numeric page
at `0x4D3DD0` and related state. It is consistent with an input callback, but
the complete event map remains open. The fifth DWORD and other unused row
fields remain unclassified. The slot targets are disassembly labels rather
than Ghidra-defined procedures in the current analysis.
Evidence: `ev_431721ec8ab45354bd3da0d7ec1ba49778a1a62ba0981a09651f7427c91fb2b7`
(`0x4CF5C0` table bytes), `ev_71b7f8b34b1ed1a6683e79cfb22bb722ef28367c84f1df05de6805f8a09fdca0`
(`0x4CF5C0` reference), and
`ev_d71026a69bdd9f0a3ae7668cf70259221f7df402856e59defa6b3325e2356dd1`
(`0x4020E0`), plus `ev_09f7712f6e55467366932195f79c6279b9bbd38e6678175b3a586814da8fca63`
(`0x41A370` bytes).

`plan_race_loop_active_frame` now composes the active-update branch's ordered
calls. It includes the post-dispatch state callback, fixed update callbacks,
the state-4-only `0x404810`/`0x40C2F0` render gate, the pre-acquisition state
callback, render-context acquisition, context install, state-0 viewport set,
entity submission, viewport restore, context-taking state callback, and the
post-submit `0x40AA30`/`0x40C2F0`/conditional `0x40C330` tail. Helper and
renderer results remain explicit observations; this plan does not execute the
original callbacks or backend. The order and gate structure follow the
`0x4020E0` dossier `ev_b26e9aad81c9f9a23aa344cd2b8cd64b866103eca50f4f93b9f2e93add7bc791`.

The nested `0x425920` call now has its own `plan_race_loop_simulation_step`.
It records the bracketing calls (`0x415090`/`0x4150A0`), time
read (`0x425790`), clamped delta passed to `0x43B290`, time-anchor store at
`0x4D4DF0`, and timer-object update (`0x46B620`). The observed delta is
clamped to `[0.01, 0.25]`; previous/current clock values and the two opaque
object pointers remain caller inputs. The helper updates the persistent time
anchor before planning the timer-object call. Evidence:
`race_loop_simulation_step.evidence.json`, `0x425920`
`ev_6321e495464c40ba279498410a9fd3f0332edf60d35d4f263d85b292a74efa16`.

The front and tail of `0x43B290` are further represented by
`plan_race_loop_periodic_update_dispatch`, composed with the outer timer by
`plan_race_loop_simulation_frame`. Its accumulator at `0x509424` adds
the simulation delta; at `>= 0.055555556` it resets to zero, calls `0x444E10`
with the tick flag, then calls `0x4170A0` with the three supplied DWORD values
from `0x509384`, `0x509388`, and `0x5093A4`. `0x444E10` is called on every
update. `update_race_loop_43b290` now composes the observed simulation delta,
periodic refresh snapshot, 15 binding reads, steering, two analog channels,
and control-tail writes. Platform input acquisition, binding records, tuning
globals, and force-feedback helper inputs remain observation boundaries.
Evidence: `race_loop_update_43b290.evidence.json`,
`ev_db358c27101404ca5eb5166ee5eab7df3a38f2a0eca453f836021a79b14d5b46`.

The `0x4020E0` key path for event `0x31` toggles the callback-table state:
state 0 becomes 4, and every nonzero state becomes 0. It passes that value to
the setter at `0x402960`; the setter stores `0x4CF5B8`, calls `0x4035D0`, and
re-enters the loop. `0x4035D0` sets the shared overlay dirty flag at
`0x4EB300` to 1. On the next render pass, the callback table therefore selects
state 4's shared overlay path or returns to state 0's dashboard path.
`toggle_race_overlay_from_key_0x31` preserves the transition and dirty/re-entry
effects; it models the key branch after the loop's preceding input gates.
Evidence: `0x4020E0` (`ev_d71026a69bdd9f0a3ae7668cf70259221f7df402856e59defa6b3325e2356dd1`)
and `0x4035D0` (`ev_feac0ff2c59568879672966119ceb295b90320616e998ce5cfe8382b947ae4d1`).
`plan_race_overlay_toggle_following_frame` composes that accepted key branch
with the next active-frame plan: it substitutes the resulting callback state
before selecting the observed callback row, state-4 render gate, render-context
path, and post-submit checks. Helper and context results remain observations;
the composition does not assert that rendering happens synchronously inside
the key handler.
`plan_race_loop_shared_overlay` now carries the transition through the actual
callback list and composes the shared tail only when its wrapper is present.
State 4 selects `0x404960` with second argument 0; state 0 selects
`0x4052D0` with second argument 1 after the render context is acquired. The
shared-flow observation can now supply the detailed `0x44E210` acquisition
inputs; acquisition is projected only when frame gates reach `0x447C90`, and
its success result now controls submission and later context callbacks. When
omitted, the existing acquired-context observation remains in use. The
plan includes `scene_dashboard_render_plan`'s `0x405100` rectangle/bitmap/
viewport subplan on the state-0 route and the redraw-all full-surface fill's
clipped pixel-write projection when that fill is reached, then projects the
common shared tail.
Each reached `0x44AFD0` bitmap operation is also paired with a clipped blit
plan and, when packed source bytes are supplied, its per-pixel write list. The
operation index preserves the association to its viewport/draw occurrence;
the destination context, framebuffer base, and renderer mode come from the
matching render operation/state-0 context, while source mode, pitch, base, and
source bytes remain external inputs. Destination memory is not mutated.
When either `0x404960` or `0x4052D0` is reached, it also projects the
`0x404970` gear display before the shared tail, setting its dashboard-path gate
from the wrapper argument (enabled for state 0's value 1, disabled for state
4's value 0). Its resource values, selected record `+0x34`, mode, dimensions,
and render context are supplied through the gear observation.
Other state-0 dashboard text and per-record effects remain in their separate
models. Evidence: callback rows
`ev_431721ec8ab45354bd3da0d7ec1ba49778a1a62ba0981a09651f7427c91fb2b7`,
`0x4052D0` wrapper `ev_5f60fd107734e816e45359b06c10c71e1300a8c15309a630ac4de08ed745912e`,
`0x404960` wrapper `ev_87e78984503d0e8b83d062a59c5a4fbacecead9a1f77edb091e5cd5750f256d7`,
and the ordered active-frame dossier cited above.

The adjacent event switch writes a separate numeric field at `0x4CF5B4`:
event `0x170` selects value 0, events `0x171` through `0x17A` select values 1
through 10, and event `0x37B` selects value 11. Event `0x17B` reaches a shared
handler guarded by `SI == 0x37B`, so it leaves the field unchanged. The loop
passes this value to `0x4259B0`; value 11 also calls `0x401930` first. The
field's user-visible meaning remains unknown. `transition_race_loop_page_for_event`
models only these numeric writes and the guarded no-op. Evidence: the stores and
guard at `0x402893` (`ev_8d74f9cd4adf1495628dae0232ff8e521cf178c644e0d3fc543d82b391fb9f9f`),
the event-`0x37B` guard at `0x402917` (`ev_cb95cd593822c5b023cc3afd507bd6e85af275cf341ceaf944443cf82489b9d2`),
and the selector table at `0x40292C` (`ev_7f839797e1d9c3b462309cf7a5ac0e9d20bf865028dd50639b20398c8b119e9a`).

The called `0x4259B0` helper copies its argument directly to the camera-mode
selector at `0x508758` and returns. `0x421120` switches on this value to select
the simulation camera transform. For mode 11, it copies the twelve-float free
camera transform from `0x5081A8..0x5081D4` and clears the three-float velocity
vector at `0x508640..0x508648`, then reaches its common `0x46B920` postprocess.
`store_race_camera_mode` models the setter; `prepare_race_camera_mode_11_output`
models the branch's transform and vector setup before that postprocess. The
mode-8 and mode-9 branches are also modeled; both begin with an identity
matrix, multiply by an X rotation built from `1.5707964f`, position the camera
at the common preamble's X/Z anchor and its Y plus 10 (mode 8) or 50 (mode 9),
and clear only the Y component of the camera velocity vector. The X/Z components
retain the values loaded from car fields `+0x234` and `+0x23C` in the common
preamble. `prepare_race_camera_mode_8_or_9_output` models those raw effects.
Modes 1 and 2 copy the vehicle's 3x3 orientation, transform a local offset
through it, and add that world offset to the existing camera position: mode 1
uses `(0, 0.5, -0.1)` and mode 2 uses `(0, 0.8, -0.5)`. They retain the three
velocity values copied from the vehicle in the common preamble.
`prepare_race_camera_mode_1_or_2_output` models those branches. Mode 3 copies
the vehicle orientation, left-multiplies a Y rotation using
`3.1415927f`, transforms local offset `(0, 0.8, 0)` through that orientation,
and adds the result to the existing camera position. It retains the vehicle
velocity from the common preamble; `prepare_race_camera_mode_3_output` models
these effects.

Mode 0 copies the selected vehicle transform and transforms the local camera
point at `+0x47C` through its orientation into the camera position. It samples
the world surface through `0x45F760`; a miss uses the vehicle's position-like
triplet at transform indices 3..5, while a hit blends that triplet toward the
sample point using the clamped plane-dot factor and the cached cosine of 30
degrees. The selected target is low-pass filtered in persistent globals
`0x507D00..0x507D08` with weights `0.050000012` and `0.95`. The branch forms a
midpoint between that filtered target and the vehicle triplet, rebuilds and
orthonormalizes the camera basis, and applies the vehicle-axis trailing offset
from field `+0xEA8` at scale `-0.005`. `update_race_camera_mode_0` models these
transform/state effects given the raw terrain-query result; the terrain query,
common velocity setup, and common postprocess remain external.
The shared look-at helper `0x4222A0` is also modeled: it rebuilds its matrix
when squared target distance is at least 1 and either horizontal delta has
magnitude at least 0.01. It normalizes the horizontal forward vector and the
cross product of the target delta with that vector, then forms the third basis
vector by crossing the normalized pair. The helper writes the supplied camera
position back unchanged; its repeated `0x426E50`/`0x420960` refresh calls remain
external. `update_race_camera_look_at_basis` models the matrix write.

Modes 4 through 7 now compose this helper around their follow-camera placement.
They target the common anchor at height +1.5, except mode 6 at +3.7; mode 4
uses a signed offset of -6 along the look-at basis, mode 6 uses -18, and modes
5/7 use -10. If global
`0x508760` is zero, the camera position receives the observed velocity-based
`0.144` adjustment before the second look-at update. The branch caches the
resulting transform at `0x507D10`. Mode 7, or a nonzero vehicle byte at `+0x4EC`,
then mirrors the camera position around the anchor, adds a vertical offset of
3 (8 for mode 6), and updates the look-at basis toward the anchor. The
`prepare_race_camera_modes_4_to_7_output` function accepts the common-preamble
anchor/velocity and cached transform as inputs; velocity values are retained
from the common preamble. Refresh calls remain outside this model.

Modes 10 and 12 consume the seven-DWORD camera records at `0x507E10`. When
the selector count at `0x5081DC` exceeds one, mode 12 selects record zero;
mode 10 searches records/candidate positions 1 through count minus one and
keeps the first strict minimum squared distance to the common anchor. A zero
record tag builds orientation from the three scalar fields at `+0x10/+0x14/+0x18`
as a degree-valued axis-angle vector. Nonzero tags aim at the anchor plus one
unit of Y, using the supplied near-vertical fallback pair when horizontal
delta components are both within `1.1920929e-7`. Tag 1 moves along the look-at
direction by scalar `+0x10`; tag 2 moves by
`(projected distance - scalar +0x14) * scalar +0x10`. Both modes clear the
three velocity components and copy the selected transform into manual-camera
state; they also store the selected index at `0x4D488C`. With selector count at
most one, they instead aim the current camera transform at the common anchor,
clear velocity, and leave manual-camera state and the selected index alone.
`prepare_race_camera_modes_10_or_12_output` models both paths with raw arrays
for target candidates and the near-vertical fallback inputs. The camera record
selector sources and caller stack fallback values remain external inputs.

The shared `0x46B920` camera handoff calls `0x46DD20` on object
`DAT_004DD550`. That method sets its update flag at `+0x50` to -1, copies the
selected car index to `+0x54`, copies the 12-float transform to `+0x58`, then
copies velocity X and Y to `+0x88/+0x8C`. It clears the flag to zero before
writing velocity Z at `+0x90`. `apply_race_camera_postprocess` preserves that
write order and final field layout, completing the modeled handoff after the
camera mode branches. Renderer/device effects remain outside the model.
Evidence:
`0x4259B0` (`ev_80b07d1748983f2286d6f5186c8ce259e35973247665736739ec2308dd410f5d`)
and `0x421120` (`ev_6b51aabc0271063359b6ccbd5d3a371f09cebe0b44f361eda986d2c98221a6ce`),
plus helpers `0x422C60` (`ev_56018b3f07a1ee8d7aa46cc888d2d250f15c59869a0151d556745e00d00a1c9b`),
`0x422E60` (`ev_d26400f724bc88422b7a29bcf286184aa6de7bd4f6d6542701ded5f966e4b007`),
`0x402DD0` (`ev_6a140d65910d80b76b011dd2b0a9577b2ebec891fde756ae5afc1d9650608276`),
the vector transform helper `0x422F30` (`ev_85b06e95b3134f859a52c9817b70d691eaa55e8dae49cd70bb7996c2acd6d26a`),
`0x4222A0` (`ev_2e015a1c20a81c36c7da4ef0e458eb7f52843b85a4e5ebfa6a2e8226945321bc`),
`0x422C90` (`ev_3702f3d5d08f635be44886076ed78c7a55a2b3d4e28c4cd1e6dac23eeab7999a`),
and preset axis-angle magnitude helper `0x422C10` (`ev_fcac83dca100ca552495de940289a21cdceec2d1c5a6476383f4174ffa0d8052`).
Mode 0 also uses the `0x45F760` surface-sample wrapper
(`ev_91e9c23e1300a389eaeb54a7dfa47a1edddee47c9802e723ed477a7a3eab959e`)
and the `0x4672B0` grid/plane lookup
(`ev_4ebcc9039c9fe4be21ee8f37369812e4894783e9c64041b64b8b0631585b7553`).
The shared handoff evidence is `0x46B920` (`ev_daf7c48a842bbde98447fe2cc5a472678006634ad9ab80b45f775824b17d2553`)
and `0x46DD20` (`ev_8f51fadbcbc5ef5d3712ad4b6467c7f0332630c21b7e0939ef17cd7b5a310bfa`).

### Page-11 camera controller

When the numeric page at `0x4CF5B4` is 11, the loop calls `0x401930` before
`0x4259B0`. The controller reads raw key states and selects translation and
rotation step sizes from `(key[0xFD] << 1) | key[0xFE]`: translation values
`{1.0, 0.25, 10.0, 0.05}` and rotation values
`{0.05, 0.0125, 0.15, 0.0025}`. It damps persistent translation and rotation
velocities by `0.7` each call, replacing each three-component velocity when
any corresponding directional input is active. It applies the translation
through the current row-major orientation at `0x5081A8`, adds it to the camera
position at `0x5081CC`, constructs X/Y/Z rotation matrices, and left-multiplies
their combined rotation into the current orientation. The update therefore
continues to drift and rotate under the damped stored velocity after keys are
released.

The raw directional key groups are preserved in `RaceCameraKeyState`; their
labels are not inferred. Key `0x10` is edge-latched by `0x4CF638`; its first
down call requests `0x4203E0` after applying movement, and a released call
clears the latch. `update_race_camera_control` models the numeric pose and
velocity updates plus that helper-call edge. `0x4203E0` checks the active camera
index against the count, then writes position to the selected seven-DWORD
record at offsets `+4`, `+8`, and `+0x0C`. If the record's first DWORD is zero,
it also stores angles in degrees at `+0x10` and `+0x14`: the first is
`atan2(matrix[8], matrix[6])`, the second `atan2(matrix[7], matrix[4])`. A
nonzero first DWORD leaves both angle fields untouched; the first DWORD and
DWORD `+0x18` are never changed here. Camera modes 10/12 read all three
trailing scalar fields when the record tag is zero. `capture_race_camera_preset`
models these writes from the supplied active index/count and record span. The span's
bounds and negative-index check are portable model guards; the binary only
checks `active_index < camera_count` before indexing its global array.

Both operations accept key states and preset records rather than calling the
game APIs. Host `sin`/`cos`/`atan2` and long-double arithmetic approximate the
x87 path; the runtime keyboard polling and preset selection source remain
external inputs.

Evidence: `0x401930` (`ev_0f6a57443ffce639d7dafda2e07ffbbe3ffaf601062e00635e3e8fab8f57f7fc`),
axis-matrix helpers `0x402D10` (`ev_f2ecfe559d13f46f705805690080f5ec0c8232ce2ed0f5debbfa050b554cfede`),
`0x402D50` (`ev_7d8a4715d8f4e8c7d9aa32714b9c7b2d2efba06edea7a13c9af38a684fa02027`),
`0x402D90` (`ev_c77fd7fc037e22f744ab6c17a1f9420bec46856ba6407b1725c92658e000ac09`),
matrix multiply `0x402DD0` (`ev_6a140d65910d80b76b011dd2b0a9577b2ebec891fde756ae5afc1d9650608276`),
and preset capture `0x4203E0` (`ev_e417030dba20e4c12151d0e0c8440367fd50a53159758426bf1e9df63cee647f`).
Its compatibility branch reaches `0x4BD0A5`, whose bytes are `FPATAN; RET`
(`ev_379c7a276916ddb31c1c9f3d5e05492450832fac5dad6c6d9971bd760ff094c2`).

### State-4 shared overlay callback

The state-4 slot `+0x00` wrapper at `0x404960` calls `0x404970` with the
render context and a zero second argument. `0x404970` still installs the
context and evaluates the shared banner/result/loading predicates after its
optional main-dashboard block, so state 4 reaches those shared paths without
running the state-0 dashboard body. `scene_shared_overlay_gates` models those
conditions from the raw globals: the shared banner predicates are suppressed
by either result flag `0x4EB2F8` or loading flag `0x4EB1E8`; the paused gate
also requires `0x508760` and a clear `0x4ECB40`; the cheater gate checks either
byte at `0x4EB2FC` and requires `0x53A4B8 < 12`. The `0x4EB2FA`, `0x4EB2F9`,
and `0x4EB2FB` predicates remain raw flags. Race-results and splash-loading
predicates read their respective flags directly. `scene_shared_cheater_banner_call`
adds the cheater banner's directly recovered placement: after the `0x4048B0`
gate, `0x404970` centers the text using width/height measurements from
`0x44D460`/`0x44D450`, with `x = surface_width/2 - text_width/2` and
`y = surface_height/3 - 20 - text_height/2`. The call uses resource object
`0x4EB350` and draw helper `0x44D0C0`. Its third argument starts at
`5 - world_index`; negative values take the observed low-bit reduction. The
argument's renderer meaning and the font/resource effects remain unresolved.
The model accepts the gate and measured text dimensions instead of reproducing
those helpers.
`scene_shared_cheater_banner_plan` now connects the shared suppressed gate,
both raw cheater bytes, and the `world_index < 12` limit to that placement;
surface dimensions and helper-measured text dimensions remain observations.

After raw predicate `0x404920` passes, the shared overlay draws resource handle
`0x4EB46C` through `0x44D0C0` at x=`(surface_width - resource_width)/2` and
y=`surface_height/2 - 0x82`. The width comes from `0x44D450`; the adjacent
`0x44D460` result is not used in the observed placement. The predicate's
meaning remains unknown. `scene_shared_centered_resource_call` projects this
raw resource call from the gate, surface dimensions, and measured width.
`scene_shared_centered_resource_plan` now derives that gate from shared
results/loading suppression and raw byte `0x4EB2FB` before applying the
placement. Surface size and measured resource width remain inputs; handle
`0x4EB46C`, draw helper `0x44D0C0`, and raw trailing arguments are preserved.

`scene_shared_paused_banner_call` projects the paused branch immediately
before it: when `0x404860` passes, `0x404970` compares the current resource
generation at `0x4D3868` with the cached value at `0x4EB468`. A mismatch calls
`0x418CC0` with resource object `0x4EB460`; either way the branch then calls
`0x40C930` with handle `0x4EB464`. `0x40C930` calls `0x40C790` with the
`load_stp` resource key and a zero mode byte. `scene_paused_banner_render_plan`
projects its layout: when `load_stp` resolves, it centers that resource using
width from `0x44D450` and height from `0x44D460`, then draws the text handle at
`(surface_width/2, surface_height/2 + 4)`. If lookup fails, it measures the
text using `0x479450(0x0E, handle)` and draws an outer box spanning
`center_x +/- (text_width + 20)/2` and `center_y +/- 15`, followed by an inset
box with each edge moved inward by 2. It then draws the text handle at the same
center-plus-4 location through `0x479580` with size `0x0E`. A resolved resource
is passed to `0x44D0C0` and released through `0x44D0A0`. The lookup result,
measured dimensions, and box colors remain raw inputs; renderer effects remain
outside the model.

`scene_shared_paused_banner_plan` composes the shared raw-global gate,
resource-generation refresh decision, and inner paused-banner layout. The
state-4 shared callback can therefore resolve this branch from the same
observation used by the other shared predicates. Resource lookup, measured
dimensions, and final backend submission remain supplied observations.
`scene_paused_banner_render_plan` now also emits the inner helper calls in
observed order: fallback outer and inner fills then text; or resource draw,
optional text, and release. `scene_paused_banner_box_pixel_writes` projects the
two fallback boxes through the shared clip/mode path. Text/resource submission
and acquired-resource behavior remain external calls.
Evidence: `0x404970` dossier
`ev_ca260757f720e3c0ff9c2abc12db97068c2470e44c8709aac9e856d753979e7d`,
`0x404860` gate
`ev_5a287a4e91b3a5f94a8a759a58876682a3d84d73c3ebdca9aea715361c55b951`,
and the `0x40C930`/`0x40C790` layout evidence listed below.

The `0x4048E0` and `0x404900` flag branches share another recovered sequence,
projected by `scene_shared_text_effect_plan`. Each conditionally refreshes a
resource on generation mismatch (`0x4EB428`/`0x4EB430` with text handle
`0x4EB42C`, or `0x4EB290`/`0x4EB298` with handle `0x4EB294`), then calls
`0x44DB10(0x4EB304, text_handle)`, `0x44DC50(0x4EB304)`, and `0x44DC70()`.
The `0x4048E0` draw at `0x404D6C` uses x=`surface_width - measured_text_width - 10`;
the `0x404900` draw at `0x404DC1` uses x=10. Both use y=67, flags `9`, color
source `0x4EB314`, font `0x4EB304`, and the selected resource text handle as
the format pointer. `scene_shared_text_effect_plan` now projects these raw draw
arguments; it accepts the gate, generation result, surface width, and measured
text width as inputs. The semantic meaning and final visual effect of the two
flags remain unestablished.

`scene_shared_text_effects_plan` composes these effects from the shared raw
flags: `0x4048E0` receives the common unsuppressed `0x4EB2FA` gate, while
`0x404900` receives the common unsuppressed `0x4EB2F9` gate. Each branch then
uses its own resource-generation observation and the established draw layout.
The composition fixes the observed resource object/cache/text triplets at
`0x4EB428/0x4EB430/0x4EB42C` and
`0x4EB290/0x4EB298/0x4EB294`, respectively.
The separate `0x404920`/`0x4EB2FB` resource draw is composed by
`scene_shared_centered_resource_plan`, not by the two-text-effect plan.
Evidence: common gate `ev_2d409f9f2cca7c8c137ba2687dc95a0170ec2fcdc3faa45f95cd0f5c3d465b94`,
`0x4048E0` `ev_269a5ac39029a3557fa7b0d9466c7709fb45b6765cba61e7a80557d24164f597`,
and `0x404900` `ev_aa5e142979b28027773fa22ed4c39dfc3295e6e486888ed728fa5ce7ad6a52ae`.

`scene_shared_results_heading_plan` covers the heading work before the result
row loops. The `0x404940` branch lazily registers `Dashboard:RaceResults`
(`0x4CFF0C`) under bit `0x80` of `0x4EB274`, and `Dashboard:PressESC`
(`0x4CFF24`) under bit 0 of `0x4EB37C`, each with its observed cleanup
callback. It centers the measured `0x4EB448` prompt at
`((surface_width - text_width)/2, (surface_height - text_height)/2)`, then
conditionally refreshes the results object `0x4EB248` and prompt object
`0x4EB410` before issuing both `0x44DC70` draw calls. The model accepts
the two masks, gate result, generation comparisons, and helper-measured prompt
dimensions as inputs. Its ordered operation list preserves lazy results
registration, lazy prompt registration, the centered `PressESC` resource draw,
results refresh/draw, then prompt refresh/draw. The two `0x44DC70` payloads
remain implicit global renderer state. Evidence: `0x404970` analysis
`ev_a058b9f23799a14959f9a4c014c5676ed3461968e94bbb8ac13c1c3cb3687e65`.
The nested row traversal and per-record text calls are modeled by
`scene_shared_result_rows` and `scene_shared_result_row_draws`. The two loops each span the
count returned by `0x45DFA0`: the outer pass is a 1-based displayed ordinal,
and the inner loop selects records whose type at `+0x00` is not 3 and whose
ordinal at `+0x140` equals that pass. Each selected record reaches `0x45C230`
and two `0x44DC70` calls, then invokes the global object's virtual `+0x40`
method with its record index. A true result additionally looks up time using
the record index truncated to a byte, formats it through `0x425A40` with the
second argument set to 1, and issues another `0x44DC70` call.
`scene_shared_result_row_displays` carries the selected rows through the
existing ordinal lookup and time formatter. `scene_shared_result_row_draws` now
projects the three text calls per selected row: ordinal `%d`, record text at
`+5` with `%s`, and (when virtual `+0x40` is true) the formatted `%s` time. It
uses color `0x4EB374` when `0x45C230`'s returned global DWORD at `0x4DC19C`
equals the inner record index, otherwise `0x4EB370`. Every column derives from
the shared surface-width-half stack slot: `0x404970` stores
`[0x509A4C]/2` at `0x404EA0` and re-reads it at `0x404FF0`, `0x405009`, and
`0x40505C`, so the ordinal x is `surface_width/2 - 0x91`, the record text x is
`surface_width/2 - 0x6B`, and the time x is `surface_width/2 + 0x41`; no
caller-supplied x seeds remain. The row baseline starts at
`(surface_height - 0xB4)/2 + 0x11` because `0x404F71` adds `0x14` to the
`(surface_height - 0xB4)/2 - 3` seed before the loop, and advances by `0x14`
for each selected row that reaches the text calls. The ordinal and
inner-record loops can therefore
place multiple records from one ordinal pass on successive lines.
Formatting retains the existing host CRT and x87 precision limitations; the
raw `0x4DC19C` global value remains the only per-row input.

The same stack slot also decodes both `0x404940` heading texts, which
previously kept implicit payloads. The results title draw at `0x404F32` uses
format handle `0x4EB24C` at x=`surface_width/2`, y=`(surface_height-0xB4)/2-8`
with flags `0x24`; the prompt text draw at `0x404F76` uses format handle
`0x4EB414` at x=`surface_width/2`, y=`(surface_height-0xB4)/2+0xBF` with the
same flags. Both use color `0x4EB374` and font `0x4EB304`. Their argument
order `(font, color, flags, x, y, format)` is confirmed against the decoded
`0x4048E0` right-aligned draw at `0x404D6C`
(x=`[0x509A4C]-width-10`, y=`0x43`) and against `[0x509A2C]` acting as the
surface height in the `0x404920` centered-resource block
(`y = [0x509A2C]/2 - 0x82`). The heading plan now emits both draws with that
geometry.

`raster_scene_shared_results_heading_text` takes the two `0x44DC70` text
operations from that heading plan in call order and projects their glyph and
pixel writes through the loaded-font, RLE-atlas, clipping, and palette pipeline
used for result rows. It validates the title/prompt format handles and requires
one renderer observation for each draw. The localized title/prompt strings and
renderer observations remain inputs; resource lookup and surface acquisition
are outside this helper.

`scene_shared_results_branch_plan` composes the direct results flag with the
heading gate/resource work, selected row records, optional time values, and
ordered draw arguments. The record span represents the accessor-bounded row
set, and time bytes/count and formatter marker stay explicit inputs; row and
heading geometry derives from the surface dimensions. A false results flag
returns the gate and unchanged heading
masks without traversing or drawing rows.
`raster_scene_shared_results_rows` now connects those row calls to the existing
`0x44DC70` font layout and `0x44D0C0` atlas rasterizer. It consumes the loaded
font tables and RLE atlas plus one renderer/palette observation per call, and
emits pixel writes in the same order as the three calls per selected row.
The `0x4CFF38` ordinal format is `%d`; its signed `integer_argument` is now
formatted as decimal before glyph layout, while the `0x4CFF3C` record text and
`0x4CFF40` time calls use their supplied string arguments. This ensures the
ordinal column contributes glyphs to the raster output instead of passing an
empty string to the layout stage.
Incomplete palette or glyph data remains reported rather than guessed.
`apply_scene_shared_results_raster` refuses incomplete plans, then delegates
to the existing all-ranges-validated text-write applier so the row batch is
applied atomically to a supplied framebuffer.

After results, `0x404970` calls the `0x404950` thunk to read the direct
`0x401DF0` splash-loading flag. When set, it registers `Splash:Loading`
(`0x4CFF44`) once under bit 1 of `0x4EB37C`, refreshes object `0x4EB258` when
its cached generation `0x4EB260` differs from `0x4D3868`, and draws handle
`0x4EB25C` through `0x40C930`. `scene_shared_loading_banner_plan` models this
mask/refresh/draw sequence. Its ordered operations now record registration
(`0x418C90` with cleanup `0x405240`) when needed, generation refresh
(`0x418CC0`) when stale, and the final resource draw (`0x40C930`).

`scene_shared_overlay_frame_plan` composes the shared post-dashboard callback
in the recovered predicate order: pause, cheater, `0x4048E0`, `0x404900`,
`0x404920`, race results, then splash loading. It feeds one shared raw-global
observation to each branch while keeping branch-specific measurements, data
buffers, and resource generations explicit. The optional dashboard body
before these predicates is not part of this shared-tail plan. Evidence for the
branch order and loading lifecycle is the `0x404970` instruction stream
`ev_76ecfc938780e22fc3861f3c54d3e690ccdd6d00a0cced2adf06bb05ec45a292` and
the loading-flag evidence `ev_ab094b5171d133fe86ce13f0add682eed6ee00de9f12a4e79a74c0541609edca`.

Evidence: `ev_e9c42d15820c303e581b1788df1d4e7e9db0a6c77cec72ece67cea39c4ff7816`
(`0x40C930` paused-banner resource wrapper),
`ev_446d548abb2cba168ffdeabb4111c6192dd2e8149b811792d6984c0291245bdd`
(`0x40C790` resource/fallback layout),
`ev_09f7b129c2801b51a9e11c7d8851fee08efd7a59ecd1b3f35d7fced94dc5e318`
(`0x44D460` resource height), `ev_02d7f31e3392c72464a90b91176191d24fec056a5c9e48b61b6ec23768c97a5a`
(`0x44D450` resource width),
`ev_87e78984503d0e8b83d062a59c5a4fbacecead9a1f77edb091e5cd5750f256d7`
(`0x404960` wrapper bytes), `ev_ca260757f720e3c0ff9c2abc12db97068c2470e44c8709aac9e856d753979e7d`
(`0x404970` pseudocode), `ev_76ecfc938780e22fc3861f3c54d3e690ccdd6d00a0cced2adf06bb05ec45a292`
(`0x404970` instruction stream through the nested result-row calls),
`ev_daf92979a6d79329dd74be1b6bc4dc28b53c1d13fc15381fd9f4711954e1bbe0`
(`0x45C230` returns global `0x4DC19C`),
`ev_2d409f9f2cca7c8c137ba2687dc95a0170ec2fcdc3faa45f95cd0f5c3d465b94`
(`0x404890` common results/loading gate), `ev_5a287a4e91b3a5f94a8a759a58876682a3d84d73c3ebdca9aea715361c55b951`
(`0x404860` pause gate), `ev_6c94c664a37d430f6971b409708dab44df6e18abdd54940f268b90d24d2551ed`
(`0x4048B0` cheater gate), `ev_269a5ac39029a3557fa7b0d9466c7709fb45b6765cba61e7a80557d24164f597`
(`0x4048E0` flag gate), `ev_aa5e142979b28027773fa22ed4c39dfc3295e6e486888ed728fa5ce7ad6a52ae`
(`0x404900` flag gate), `ev_15b5604518b5f97dd7ebda713765a3559838335a356b12b983a5ffffb6b44c33`
(`0x404920` flag gate), `ev_09f7b129c2801b51a9e11c7d8851fee08efd7a59ecd1b3f35d7fced94dc5e318`
(`0x44D460` resource height helper), `ev_02d7f31e3392c72464a90b91176191d24fec056a5c9e48b61b6ec23768c97a5a`
(`0x44D450` resource width helper), `ev_1c065c92998a4cd49e818f980a1d6b5ddaba78b56125ce8b9747963977b4210b`
(`0x404940` results flag), `ev_42d4d2ab151d94b72fb2ed397466809faa0a8fc480bb5b69707da013d37521e8`
(`0x425830` paused state), `ev_dba5f70016c4e8822f0abbb64cea48b8691d8b4c33065b4693020098647c9326`
(`0x40C2F0` suppression predicate), `ev_0085224d6d475c3fa543ba1d038de42796a71dc177c8408484426206cd08e261`
(`0x45BB80` world index), and `ev_ab094b5171d133fe86ce13f0add682eed6ee00de9f12a4e79a74c0541609edca`
(`0x401DF0` loading flag), `ev_d758f4038a0311ca3c6f5590867064800246e8f5bde871d0dda178ad15a8d10b`
(`0x45DFA0` results count accessor), and
`ev_6ea3c9fe70c8fed0675dfdf14fad61ac91096fb24ecf69a173535ca780dab073`
(`0x45C230` opaque row helper).

### State-5 numeric overlay callback

State 5's render callback `0x422A70` reads the row-major 3x3 matrix at
`0x507DC8` and passes it to `0x422FD0`, which converts it to a four-component
quaternion. `matrix3x3_to_quaternion` reconstructs the positive-trace formula
and all three largest-diagonal branches, preserving the chosen axis and the
`0x4E9754` compatibility-division gate. `state5_numeric_overlay_plan` models
the callback's background rectangle and two glyph-text calls. It formats
`TV Camera: %d/%d` at `(15,15)` from raw globals `0x4D488C` and `0x5081DC`,
then displays position values from `0x507DEC`/`0x507DF0`/`0x507DF4` and a
rotation vector at `(15,30)` using the observed formats at `0x4D4B04` and
`0x4D4B18`. The angle is `acos(q.w)`; the callback scales `q.xyz` by
`2*angle/abs(sin(angle))`, zeroing the vector when the sine magnitude is
strictly below float bits `0x34000000` or unordered, then converts radians to
degrees with the float at `0x4C770C`. The input quaternion/matrix and helper
intermediates follow the x87 path approximately; host `acos`/`sin`, division,
formatting, and the exact Pentium FDIV correction remain limitations. The
rectangle helper `0x44A1B0` dispatches by renderer mode `*0x4D7A48`: mode 5
calls `0x44A550`, mode 1 calls `0x44A640`, and other values call `0x44A490`;
the plan records that selected backend helper. The plan
expands both strings through `0x44D6A0`'s glyph dispatch: it replaces bytes
below `0x20` or above `0x7F` with `#`, emits one `0x44D780` call per remaining
character, and advances x by 8. `scene_font_8x8.hpp` contains the recovered 96
glyphs for bytes `0x20` through `0x7F`; `state5_numeric_overlay_plan` also
projects a `0x449E90` call for every marked pixel in each 8x8 glyph. It applies
the helper's half-open clip bounds. Mode 5 writes the packed 32-bit color at
`framebuffer + pitch*y + x*4`; other modes write 16 bits at
`framebuffer + pitch*y + x*2`, using the low color half for mode 3 and the high
half otherwise. The mode-5 packed color conversion is preserved as raw bit
operations. The table can be regenerated with `tools/extract_scene_font.py`
from the extracted `race.bin`. `apply_scene_text_pixel_writes` applies the
already clipped little-endian glyph writes to a caller-supplied framebuffer
span only after every destination range validates; it does not reproduce
runtime surface acquisition or backend effects.

Evidence: `ev_d1302d6070561e9af261fa64f57d21bb8e1f38b9450d57e098fbac520d0f4560`
(`0x422A70` callback bytes) and
`ev_e2025a079697b77f5659ad1f6a83af953287c958c73f5c984b9f2c96b1f2f1e5`
(`0x422FD0` matrix-to-quaternion pseudocode),
`ev_97aadc541d53cedc69473b543b452c21d6b137ac37ac10639ccff432bc1358f5`
(`0x422A70` callback instruction bytes),
`ev_2a26e6d107f0b9c0d9ded59941dc402ee80fced89f4221c0668c9a5b59d27030`
(`0x4BDFBC` acos dispatcher),
`ev_092f675db87201244a1925ccc3c66acb3f06bf5f9ba8d76516103eca6336980a`
(`0x4BD190` `_sprintf`),
`ev_fac3ce6a51f63e7491f9206e7df94552cac18bfd772c339be1a2c5853505e1ed`
(`0x44D6A0` glyph-text helper),
`ev_6e1559c698437961095f7e96979855145b53c582720b4a5e5dc8801d71e83ac4`
(`0x44D780` glyph bitmap walk),
`ev_49b29e8c32f44ae4536d89a5339efb24f4046920af5ddcd219e9061db4133950`
(`0x44A1B0` renderer-mode dispatch),
`ev_5bf7b00b4f16d8a6fbf9ae67a1fd00fa550e240305e21eed5c59c58c0056d5d1`
(`0x449E90` pixel-format dispatch),
`ev_ffbb719fa1a4e28f0826409e27e500095a55f79bedcac0b86b83cefebc8cd4b2`
(`0x4499B0` clip gate),
`ev_3af39f12a015ba69b073d9de3a19ea6f3a9f2c3ed2a4b3cfe863e39edb419e75`
(`0x449F30` 16-bit pixel write),
`ev_52a6355700fc4b4eeddb773cb0d04e767683d5659a32e8fc9bb815c4668f88cd`
(`0x449F80` 32-bit pixel conversion/write),
`ev_d5abdaae0471d22b891f4093882e0e86a5bc59c1e6787091a55ba87f0b1351f2`
(`0x449C20` 32-bit address calculation), and
`ev_1fb600e7e4d300bb14c54f8f84358471a05c34f0a572ea29746fd16ebd1de66b`
(`0x449D70` 16-bit address calculation),
`ev_8363c632acba66c5d18aecd508e813618c18833cdb92deb9fb9c657bdea23abf`
(`0x4D7848` through `0x4D9A48` font pointer/bitmap data),
`ev_b1e959ec02960ae44985ca3f03fa5925ac6f8bfcc0cb97a3b3c4b2b77f6ccce5`
(`0x4D4B04` format strings), and
`ev_5f6a3b7a8221ecdf741cc551368993ac4daa985e98b8a27ca88452126e3c6be4`
(`0x4C770C` through `0x4C7790` constants).

### State-0 dashboard callbacks

State 0's slot `+0x04` target `0x4052D0` forwards the render context to
`0x404970` with its second argument set to 1. The recovered body uses the
selected-car record at `+0x134`, installs the context, and enters the main
dashboard branch. That branch calls `0x405100`, which applies the dashboard's
rectangle table at `0x4CF9F0` and conditionally draws nine more rectangles from
`0x4CFA20` according to bits in `0x4EB40C`, then restores the full viewport.
For these calls, `0x405100` computes a horizontal offset as
`(surface_width - 640) / 2`, except widths 512 and 320 use `-192`. It adds this
offset to each rectangle's left edge and aligns left down to a multiple of 4;
it adds the offset plus 3 to the right edge and aligns right down to a multiple
of 4. The three rows at `0x4CF9F0` are unconditional. The nine rows beginning
at `0x4CFA20` are selected by mask bits `1` through `0x100`. The
`scene_dashboard_viewport_plan` emits just the resulting `0x4498A0` calls,
preserving row addresses and required bits. `scene_dashboard_render_plan`
composes the observed order around them: on a full redraw with nonzero
horizontal offset it emits the `0x44A1B0` full-surface call using raw arguments
from `0x4EB3DC` and `0x4EB27C`, then the `0x44AFD0` background bitmap call;
after each viewport call it emits another background bitmap call, and it ends
with `0x4498E0` restoring the full viewport. The bitmap wrapper uses object
`0x4EB3D0`, dimensions read from its `+8/+0xC`, renderer mode `0x4D7A48`, the
horizontal offset, and zero. The wrapper's `0x44A730` renderer dispatch and
clipped source/destination rectangle are projected separately by
`scene_dashboard_background_bitmap_blit_plan`. It selects the observed
copy/conversion helper from source and destination mode bytes, follows
`0x4499E0`'s half-open destination clipping, and reports adjusted source and
destination starts, transfer size, and starting addresses from the two pitches.
`scene_dashboard_bitmap_pixel_conversion` applies the recovered value
transforms for one source pixel on every supported route: same-mode raw copy,
mode 4 to mode 3 masking, 16-bit modes 3/4 to mode 5 byte expansion, and mode
5 to 16-bit modes 3/4 packing. `scene_dashboard_background_bitmap_pixel_writes`
walks the clipped rectangle over a caller-provided packed little-endian source
buffer and emits the converted destination value and address for each pixel.
It rejects incomplete source data without returning a partial write list. It
does not read destination memory. `apply_scene_dashboard_background_bitmap_blit`
applies a fully validated write list to a caller-provided linear destination
buffer, leaving it unchanged if either buffer is too small. The wrapper calls
`0x415090` and `0x4150A0` around the helper; both are `ret` stubs in
`disasm/engine_.text.asm`, so this path shows no lock/unlock work at those
addresses. The actual runtime pointer and DirectDraw integration remain
outside the portable model.
This path is grounded in `ev_bf174d2974c6824e088d742d89366c996c535a58e0e32c65859cffa7e4352794`
(`0x44AFD0`), `ev_f1489e5dba972a665d6c590810af0231be270ca328d2d981588a3af4f9cf5454`
(`0x44A730`), `ev_caa100f887cef378071707f8081ce39fb7cbb55a0d1898a05f950c6593ad2a74`
(`0x4499E0`), and the mode-specific helpers `ev_5f29400965839c8b80ff23b7d20c7aea14627bb5f62355213cde89dcaa703e67`,
`ev_b6c69ff55731f1a513fdd70f167312f6a97de20d4040dbc57be862b32874a693`,
`ev_0bbd8911b69a5f6ade34f00d54144874da5c47eec3048af0e2315af02eb897e3`, and
`ev_f76d514c0fcf54606e684508dfaef6ec0ec62e7b1c3fe571cc2211e83d6a1c22`.
The 16-to-32 conversion transforms are from `0x44B210` and `0x44B250`
(`ev_509c8c323f55d2cc351a48540e6bd3ddccd7004dc71a3793784e9c658d1c5d4e`
and `ev_ec1e4f2d9412b4d1cf77deededc39a67a2551dc3388c0c1d89011a960e69caed`):
both output the low byte of the shifted source word in the low result byte;
the high result byte uses shift 3 for mode 3 and shift 2 for mode 4. The
mode-5 to mode-3/4 packing follows `0x44ADC0`'s component masks and shifts in
the helper evidence above. These value-level equations are implemented by the
single-pixel converter; the full clipped source traversal now emits projected
writes, while destination memory mutation and device effects remain open.
`scene_dashboard_full_surface_fill_projection` additionally projects the
redraw-all fill from `0x405100` when its horizontal offset is nonzero. It clips
the rectangle through the context viewport and reports its encoded pixel,
write mask, bytes per pixel, first destination, and clipped bounds. Mode 1's
high-nibble preservation is a masked update; producing the final pixel still
requires the previous destination word. The rectangle backend and clip
evidence cited below also apply to this full-surface call. Both it and the gear
triangle scanlines now use the same `project_rectangle_fill` implementation,
so the `0x449940` clipping and `0x44A490`/`0x44A550`/`0x44A640` mode handling
stay aligned across the two dashboard draw paths.
The dashboard branch includes the resource keys `Dashboard:RaceTimeFinalBanner`
and `Dashboard:GearDisplay:ReverseGearCharacter` /
`Dashboard:GearDisplay:NeutralGearCharacter`; the gear display path is gated by
the surface width being greater than `0x27F`.

The one-time setup in `0x4031F0` is now represented by
`scene_dashboard_initialization_plan`. Its ordered operations preserve the
zeroed race/dashboard flags, text-resource construction for elapsed time, best
lap, and best-lap car, the six `stp` resource registrations, five font
registrations, two status-font measurements/context initializations, and the
colored resource creation and color writes. It also projects the summary
resource registration and the final redraw-cache sentinels (`-1` and float
`-1.0`) plus the initialized byte. Resource handles and helper effects remain
runtime outputs. Evidence: `0x4031F0`
(`ev_f63a1c54ee77e2b07363968872ef6738bfdb9961d2540c41da139ef5d11fa6e8`).
`plan_race_setup` exposes this operation plan at its `0x4031F0` call site.
The `0x44CFD0` registration helper requests the `STMP` type through shared
loader `0x417CB0`; for a present resource it requires the returned metadata
word to equal 3 and, when the loader marks data as relocatable, adjusts a
signed 32-bit product of the payload dwords at `+0x04` and `+0x10` pointers
from the embedded-data base. Shared lookup, cache, reference-count, and return
behavior is already represented by `plan_resource_get`; offline STMP parsing
and pointer-fixup projection are in `parse_stamp_payload`. Evidence:
`0x44CFD0` (`ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`),
`0x417CB0` (`ev_eee9a9b91823a05d7a00bff123f17ea332442f4170fab6d9047fb1708a1855ea`).

`scene_dashboard_gear_display_plan` now projects the branch after that width
gate from `0x404970` (`ev_ca260757f720e3c0ff9c2abc12db97068c2470e44c8709aac9e856d753979e7d`).
It records one-time registration of the final-time banner and, at widths above
639, the reverse/neutral resources and gear-geometry constants. Mode 0 emits
the rectangle `(20, height-41)-(40, height-15)` with the value loaded from
`0x4EB27C`, followed by a flush. Modes other than 0 and 12 draw the value
loaded from `0x4EB420` at `(16, height-144)`, flush, then
request the rotating geometry at `(80, height-80)` with size 50 and the
selected record's `+0x34` value clamped to at least 500. The observed geometry
parameters are phase `-0.27925268` and rate `0.00066322513` from `0x405510`
(`ev_3f3261dd623758f3ab6ca1b038a14bcb6995ca3bdf4cd68d464c3e7051f84eae`).
The portable plan now also projects the filled triangle vertices and its two
colored tip-to-base edges: center `(80, height-80)`, tip offset
`(-sin(angle)*50, cos(angle)*50)`, and base corners offset by
`(+/-3*cos(angle), +/-3*sin(angle))`. Colors are supplied as raw values from
`0x4EB29C`, `0x4EB27C`, and `0x4EB33C`. Vertex coordinates use host
long-double trigonometry and nearest rounding to approximate x87 `FSIN`/`FCOS`
and the target `__ftol`; exact x87 results still need original-runtime comparison.
`0x44A230`'s nonempty horizontal fill calls are projected as half-open scanline
spans, clipped to nonnegative rows below the supplied renderer height. The
binary uses two 1024-DWORD stack edge arrays; the portable plan caps projected
rows to that capacity rather than reproducing out-of-bounds stack writes for
larger heights. Edge intersections use host long-double division and nearest
rounding, so Pentium FDIV emulation and exact x87 edge values remain open.
The spans preserve the fill-helper calls and also produce per-pixel write
projections below; these plans do not mutate framebuffer memory. The pointer's
two `0x44C260` outline edges now emit ordered `0x449E90` pixel
calls in `rotating_gear_edge_pixels`. The line routine chooses its major axis
by comparing absolute deltas, steps from the first endpoint toward but
excluding the second, and advances the minor coordinate with a stored
single-precision slope before `__ftol` conversion. Vertical and horizontal
lines use integer steps. `rotating_gear_pixel_writes` now applies the existing
`0x4499B0` half-open viewport gate and `0x449E90` format conversion, then
records the framebuffer-relative offset and destination address. It projects
the word/dword values and write locations but does not mutate a surface or
DirectDraw device. Slope values use host division, so the Pentium FDIV
compatibility path and x87 rounding still need original-runtime comparison.
The triangle and line
helper evidence is `ev_1d43c2225e64306098fb53c1ac317dd3e59921736f45cddc1232433787d10182`
and `ev_7d92dc62df3260f95e999802b4e9bedbe018f6eda30558d4238644a47a739f74`.
Triangle spans now also project writes through the `0x44A1B0` rectangle-fill
dispatch. Mode 5 writes the converted four-byte value; mode 1 emits a two-byte
masked value with write mask `0x0FFF`, preserving the destination high nibble;
other modes use the two-byte selector from `0x44A490` (low source word only
for tag 3, high word otherwise). Projection applies the same context viewport
gate and row-pitch address calculation. Mode-1 output still needs the prior
destination word to produce a final stored value, so it is represented as a
masked read-modify-write. DirectDraw lock/surface effects remain unmodeled.
The dispatch and backend evidence is `ev_4988beb2055f09386ab3b29aaaa97f35b6e765f133302f38bd1ce7e6e0aef5a8`,
`ev_b915d79cb1d664e8eda90ce971acdcd0ae887b2cdfdc636d6633d6c9864461a2`,
`ev_580e2fd6b298faf8d1cba99b55a6495bc2bcc735301a0c3d0556bc80dcf518e4`,
and `ev_d9c34c7f72c21017bedf1e4546363b176d6e7848479b62e535bf3798f9be3964`;
the rectangle clip gate is `ev_b10b7e4eff88389675c2963f58a85d9c1ee95e8892e12be3ecdf0be8737637f5`.

The same renderer also has conditional shared overlays keyed by resource names
`Dashboard:PausedBanner`, `Dashboard:CheaterBanner`,
`Dashboard:WrongWayBanner`, `Dashboard:RaceResults`, and `Dashboard:PressESC`,
as well as a `Splash:Loading` branch. Their predicates are separate helper
calls (`0x404860`, `0x4048B0`, `0x4048E0`, `0x404940`, and thunk `0x404950`);
where the corresponding flags or mode reads are visible, they are retained as
raw state in the binary and are not renamed as higher-level game concepts.
The result-screen path iterates records, checks each record's type and ordinal
at `+0x140`, then makes virtual `+0x40` calls and conditionally draws a value
through `0x424370`/`0x425A40`. This establishes a results presentation path,
but not the meaning of those fields or virtual methods.

The state-0 slot `+0x08` callback at `0x4052E0` copies six bytes from
`0x4EB318` to `0x4EB2F8`, then conditionally draws rectangles and line geometry
using global surface dimensions and `0x4EB2F8` through `0x4EB2FA`. The raw
callback body runs from `0x4052E0` through `0x405506`; it is not a Ghidra
FunctionManager procedure. Likewise `0x4035E0` is a large disassembly callback
without a Ghidra procedure entry; its visible body includes context acquisition,
selected-record reads, and text rows keyed by `Dashboard:Position` with `%d/%d`,
`Dashboard:RaceTime`, `Dashboard:Lap` with `%d/%d`, `Dashboard:LapTime`, the
three `Dashboard:TestTrack:*` resources, and `Dashboard:Best`/`Dashboard:Last`
with associated numeric and record-derived format strings. It also branches on
the selected record's `+0x18` type field and uses record/subrecord fields for
some conditional rows. The callback remains only partially classified: format
arguments, several record fields, and most display predicates have not been
assigned semantic names. Direct draw-call arguments recovered from
`0x44DC70` are summarized below. Resource keys are initialized separately;
the `%s` and numeric format strings come from the data region directly after
those keys. The table records semantic x, y, and flags. The helper reads flags, x, and y from its stack frame, so the caller pushes them in reverse order: y, x, flags.

`scene_dashboard_callback_geometry_plan` applies the six-byte state copy and
projects the geometry calls before the callback's text paths. The first
branch gates on source byte `0x4EB31B`, uses the `0x44D450`/`0x44D460` results
for resource `0x4EB46C`, and submits the calculated four-coordinate array plus
color `0x7F000000` to `0x4484C0`. The second branch gates on copied byte
`0x4EB2F8`, centers the `0x4EB448` metric pair against the same raw surface
dimensions, and emits another `0x4484C0` operation. It derives the `0x404890`
suppression from copied results byte `0x4EB2F8` and splash flag `0x4EB1E8`.
When neither is set, copied bytes `0x4EB2F9` and `0x4EB2FA` select two more
line-shaped rectangle calls. Their resource-generation mismatches emit
`0x418CC0` refresh calls for objects `0x4EB290` and `0x4EB428`; their helper
calls are retained in order with the raw font and text-handle values. When
bounded string bytes and the font's 256 signed glyph advances are supplied,
`scene_dashboard_measure_text_width` reconstructs `0x44DB10`: it sums the
`+0x204 + unsigned_byte*2` advances through the NUL, or uses the helper's
eight-pixels-per-character fallback when the font pointer is null. Missing
NUL termination reports an incomplete measurement and preserves the caller's
observed width. `scene_dashboard_measure_font_extent` reconstructs
`0x44DC50`: for a null font object it returns the default extent 8; otherwise
it follows the object's first pointer and returns that payload's DWORD at
`+0x04` (or zero for a null payload pointer). Missing object fields preserve
the observed extent. The model starts the
shared batch at `0x448460` only once and finishes with `0x448490`. Coordinate
argument order is preserved as passed; no x/y interpretation is added where the
original names are unknown. Integer wraparound and signed division match the
observed 32-bit operations. Evidence: `ev_7faeade0d0ba5c93cbf5b7b4c55ba3a685a1d08e53d61c480824af8ef34485ed`
(`0x4052E0` bytes), `ev_91922500710b23f18621ab91f0c33e44f5e07487cbd09559fe30d08f945f031f`
(`0x44DB10`), and `ev_a43fa8b01e191080db9a371ae31bdca6cbd255859bb5e51dfc1cf76f82043b0e`
(`0x44D750`). The extent decomposition is supported by `0x44DBC0`
`ev_947d49214fc6bddd790a2e03d3ee10833a4d09b52b117f2819183691de9d95ee`,
`0x44DC00` `ev_bbe7ca7f77770f08e51c0607b7ebb9f941637cea3390180e164640571192f23d`,
`0x44D480` `ev_152cd44e150e3593386b10501cc0c8a33ba25538747adcf51456fb9bfc998666`,
and null defaults in `0x44D770` `ev_525ccb9c1707aae6df6092ae6a6cdea85647cf0e76015f797f00c0549009e44f`.
Renderer initialization now identifies the submission interface. `0x451FE0`
allocates an eight-byte wrapper; `0x4588B0` stores the `IDirect3D2` interface
at wrapper `+0`, and `0x4588F0` creates and stores the `IDirect3DDevice2` at
`+4`. Its `0x458990` helper queries the supplied surface, then invokes
`IDirect3D2::CreateDevice` at vtable `+0x20` with the device GUID selected by
the existing mode gate. Each state-0 rectangle call is expanded through
`0x4484C0` into four `D3DTLVERTEX` records and six indices
`{0,1,3,0,3,2}`. `0x452730` reaches
`IDirect3DDevice2::DrawIndexedPrimitive` at vtable `+0x78` with
`(primitive=D3DPT_TRIANGLELIST(4), vertex_type=D3DVT_TLVERTEX(3), vertex_count=4,
index_count=6, flags=8)`; its HRESULT is ignored. This identifies the API
boundary and exact submitted payload, while the driver-side rendering effect
remains external. Evidence:
`0x4484C0` `ev_2907d66994b17fff6cc1d0590a34c77e00d5a7bee3bca0fb84c8875726166a6`,
`0x452730` `ev_281aadb3ba99e83351908edcc9e32fd92cfe1828af04f7aa7036d083b31d0f0d`,
wrapper initialization `0x451FE0` `ev_4d03f10be057a55ba3900ca8373e9c9468c90caf428b5c2d04bc1d6e097bfd4e`,
`0x4588B0` `ev_35b36627000bf1aebc9abf313793369c85c7a58026ddbc09e56b97b7ff5de88`,
device creation `0x4588F0` `ev_0c3d18680d44c617bb5f4ffb7fcde88a6fb706a5f9a6017ef50b63db4af4e6a9`,
`0x458990` `ev_55e85dcc1688acacf93aba7d357531a5713097b8eeadc74fbaeab33bc228cef5`.
`plan_race_loop_shared_overlay` now recognizes this callback-table address in
either state 0's slot `+0x08` or state 4's slot `+0x04`, and includes the
geometry plan only when the active-frame call list reaches it. The enclosing
call list retains the distinct order: state 0 runs `0x4052E0` before
`0x4052D0`; state 4 runs `0x404960` before `0x4052E0`.

| Plan bit | Call site | Resource / format source | x | y | Flags | Color source | Font source |
|---:|---:|---|---:|---:|---:|---|---|
| 0 | `0x4036EA` | Position resource, `%s` at `0x4CFC68` | 348 | 18 | `0x21` | `0x4EB314` | `0x4EB2A8` |
| 1 | `0x40372E` | `%d/%d` at `0x4CFC6C` | 410 | 18 | `0x21` | `0x4EB314` | `0x4EB2A8` |
| 2 | `0x403820` | RaceTime resource, `%s` at `0x4CFC88` | 230 | 35 | `0x21` | `0x4EB3F4` | `0x4EB304` |
| 3 | `0x40391C` | Lap resource, `%s` at `0x4CFCA0` | 266 | 18 | `0x21` | `0x4EB314` | `0x4EB2A8` |
| 4 | `0x403972` | `%d/%d` at `0x4CFCA4` | 298 | 18 | `0x21` | `0x4EB314` | `0x4EB2A8` |
| 5 | `0x403A3F` | `%s` at `0x4CFCC0` | 336 | 35 | `0x22` | computed register value | `0x4EB304` |
| 6 | `0x403A66` | `%s` at `0x4CFCC4` | 342 | 35 | `0x21` | computed register value | `0x4EB304` |
| 7 | `0x403848` | `%s` at `0x4CFC8C` | 342 | 35 | `0x21` | `0x4EB3F4` | `0x4EB304` |

The Position and Lap values are compared against cached globals before their
rows are redrawn; changes set bits in `0x4EB40C`. Lap-time and later test-track
rows also compare raw selected-record floats and type bytes before formatting.
The Best/Last path formats values through `-%03.1f`, `+%03.1f`, `%1.0f`, and
`%1.1f @ %1.0f %s`, then passes the intermediate strings to separate draw
calls. The numeric and string fields feeding those formats remain unnamed.
`scene_dashboard_text_plan` records the eight call sites in the table when the
caller marks their gates as reached. It preserves call-site address, coordinates,
source address, flags, color source, and font source. Bits 0 and 1 are computed
by `update_scene_dashboard_position` from the context
change result at `0x44E3D0`, reset byte `0x4EB300`, selected-ordinal cache
`0x4CF9C8`, position cache `0x4CF9CC`, and the selected record's `+0x140`
value. It clamps that value to at least 1, subtracts one from the entity count
when `0x40E6D0` is nonzero, and reproduces the `0x4EB40C` full-redraw and bit-3
updates. The initial cache DWORDs at `0x4CF9C8` and `0x4CF9CC` are both
`0xFFFFFFFF`. The same common prefix initializes the Position and RaceTime
resources using bits `0x01` and `0x02` of `0x4EB484`;
`update_scene_dashboard_position` reports newly set bits while projecting row
caches.

The layout helper `layout_scene_dashboard_text` models the font-backed portion
of `0x44DC70` after formatting has produced a byte string. It sums signed
per-byte advances from font offset `+0x204`, applies right alignment when flag
`0x02` is set (otherwise horizontal centering for `0x04`), then applies vertical
metric adjustment in precedence order `0x08`, `0x10`, `0x40`. It emits a glyph
call only when the signed glyph index from `+0x04` is nonnegative, uses the
character-specific x offset from `+0x404`, and advances every byte whether or
not it has a drawable glyph. The top/bottom metric helper results remain
explicit font observations. This models layout and glyph selection; raster
backend clipping and pixel writes are separate stages.
The wrapper/helper observations are preserved in
`metadata/scene_dashboard_text_layout.evidence.json` (wrapper `ev_ccbcc893f64ac589352c9f038c356fbad734e92ae8b8dc4986d8e80125a54a7e`; metrics/measurement/glyph helpers `ev_ed868931361229661f9d686b7d95446e52d5065cdf2d533c03c1a7b69da27d59`, `ev_da529b1ec24759b44308db5b91000ea6f559c7a68bd55ae1b3337a85171c9b3e`, `ev_02d5d15611804abf9ba9821da2cb33b034db857182a8baab5038059d6c172d86`, `ev_02f9dab1d0169e30401743a98c02128bc29b31803b918fd2e065a14facc41eb6`, `ev_adc039d2ad233275621fb813804d81f957ebc353b51fa529a90834b24eaa1d31`).

`raster_scene_dashboard_text` models the subsequent `0x44D0C0` atlas stage.
Font rows are byte-run encoded: a clear high bit advances transparently, while
a set high bit supplies that many palette-index bytes. Glyph origins subtract
the font's x/y bearings; the renderer clip rectangle bounds source columns and
rows before writes are emitted. Palette selection follows renderer mode 3's
embedded/explicit base and the non-3 base adjustment (`+0x80` in the native
`int*`, or 256 16-bit color entries). Writes use `pitch * y + 2 * x`, preserve
the framebuffer base, and record 16-bit output values. Missing rows, zero runs,
truncated rows, or unavailable palette entries set `input_complete` false.
`0x449D70` confirms the row address calculation as surface base plus pitch*y
plus x*2. These functions project raster writes; `apply_scene_text_pixel_writes`
remains the bounds-checked operation that can apply them to a supplied buffer.
The atlas/raster evidence is in `metadata/scene_dashboard_text_raster.evidence.json`:
`0x44D0C0` is `ev_02f9dab1d0169e30401743a98c02128bc29b31803b918fd2e065a14facc41eb6`,
`0x449D70` is `ev_c34b9dd0a9177947ec36a0c0cd7003ba8751e160e2b772d1a199d2b0bc395245`,
and the fallback glyph routine `0x44D780` is
`ev_f549108220d0c24479601748be1ae1ac527ec031047b02a5fe246e1cfb220dfc`.

`scene_dashboard_core_callback_plan` composes the modeled common prefix and
selected-type branch for `0x4035E0`: copy the dashboard cache state, apply
Position redraw/cache/resource updates, then apply either ordinary Lap or
final-time state updates using the observed selected type and virtual `+0x40`
result. Only the type-0/global-`+0x40`-false route continues into the modeled
LapTime block at `0x40399A`; the composer supplies its redraw-all result and
derives its prior-state-change input from the incoming dashboard-kind cache.
The LapTime result carries its own ordered calls and state changes. The core
plan returns the updated state snapshot and combines the core reached text
site bits; the race-frame flow resolves those bits through
`scene_dashboard_text_plan` only when callback-table dispatch reaches `0x4035E0`.
For a nonzero selected-record type, the callback leaves the type-0 Lap/LapTime
routes and branches to `0x403CA8`. The race-frame flow feeds the common redraw
result and selected type into `update_scene_dashboard_test_track`, derives the
prior-state-change gate from the incoming dashboard-kind cache, and continues
the same state snapshot through its TestTrack cache/resource updates. Its
displayed-position input remains separate: the TestTrack branch reads record
`+0x00`, while the common Position row reads `+0x140`. The TestTrack plan's
emitted calls remain in branch order; resource text, generation matches,
distance scale, and suffix remain supplied observations.
On the type-0/global-`+0x40`-false branch, the flow also composes the
supplemental `+0x24`/`+0x1C` rows before the paths converge at `0x403F7E`.
Their position and count gates use the common `+0x140` Position result and
adjusted entity count; the composer overwrites both supplemental helper inputs
with those shared values before applying its `count > 1` gate. After branch
convergence, it applies both metric cache slots (`+0x10`, then `+0x14`) and
the `0x404313` auxiliary readout in callback
order for every selected type. Metric values, resource measurements, strings,
and the auxiliary glyph advance remain supplied observations.
The generic text plan's vector follows its descriptor-table order, not native
instruction order. Gear and other later callback operations remain modeled by
their separate helpers or caller inputs. The
callback dispatch is state-0 slot `+0x00`; evidence:
`ev_9aa46bc6450ec1572ea865ac9148625242d6d5ef645c945de5d2100e78982c22`.

`update_scene_dashboard_lap` models the ordinary Lap branch when the selected
object's raw type at `+0x18` is zero and the global `0x508A04` virtual `+0x40`
call returns false. The initial lap and dashboard-kind cache DWORDs at
`0x4CF9D0` and `0x4CF9EC` are `0xFFFFFFFF`. It changes the cached dashboard
kind at `0x4CF9EC` to 1, sets redraw bit `0x80`, and requests that rectangle
when a full redraw is not already active. It clamps the selected record's
`+0x138` value to at least 1; when that value differs from `0x4CF9D0` or a full
redraw is active, it updates the cache, ORs redraw mask `0x04`, and emits
call-site mask bit `0x10` for the `%d/%d` row. It also registers the Lap
resource using bit `0x04` of `0x4EB484`. The Lap resource row at `0x40391C` is
emitted on full redraw or dashboard-kind
change. The denominator comes from the global `0x508A04` virtual `+0x20` call
and remains an input. Bits 3 and 4 from this branch feed the text plan; later
Best/Last gates remain caller inputs to the generic text plan.

`update_scene_dashboard_lap_time` models the selected type-0 LapTime branch
at `0x40399A`. It registers `Dashboard:LapTime` in bit `0x08` of
`0x4EB484`. The positive-delta path requires signed raw `+0x14 > 0x3F800000`
and `+0x0C < 0x40A00000`; it sets dashboard-kind cache `0x4CF9EC` to 3,
sets redraw bit `0x80`, and requests that rectangle only outside a full redraw.
Its label row uses the resource text at `0x4EB2B4`; its second row formats
record `+0x14` through `0x425A40(..., 1)`. The text color is selected from
the low bit of the truncating conversion of `+0x0C * 8`. On fallback, the
first row is emitted only on full redraw or prior state change, while the
second row is formatted from record `+0x0C` through `0x425A40(..., 0)` and
drawn on every fallback visit. A drawn label row refreshes the resource
object when its generation differs from `0x4D3868`. The `-%03.1f` and
`+%03.1f` strings belong to the following supplemental `+0x24` and `+0x1C`
rows, not these LapTime calls. Their values and record-derived suffixes are
modeled separately by `scene_dashboard_supplemental_time_plan` below; suffix
resolution still depends on the runtime string-table observation.

`update_scene_dashboard_test_track` models the opening TestTrack path at
`0x403CA8` through the cached position row. It changes dashboard kind
`0x4CF9EC` to 2, sets redraw bit `0x80`, and requests that rectangle when the
kind changes outside a full redraw. It registers the `Try`, `Time`, and
`DistanceAbbreviated` resources using bits `0x10`, `0x20`, and `0x40` of
`0x4EB484`. The `Try` label call at `0x403DA8` is gated by full redraw,
dashboard-kind change, or the preceding state-change byte; its resource is
refreshed against generation `0x4D3868` only when drawn. The displayed value
at record `+0x00` is cached in `0x4CF9D0`; a full redraw or value change draws
the `%d` row at `0x403DF1`, sets redraw bit `0x04`, and requests that rectangle
when not already doing a full redraw. For types other than 5, the Time label
row at `0x403E53` follows the same redraw/state-change gate and refreshes its
resource when needed. Its value row is drawn every visit from `record +0x0C`
through `0x425A40(..., 0)`; type 3 selects format source `0x4CFD5C`, while
other types select `0x4CFD58`. Type 5 instead uses the Distance label at
`0x403F12` under the redraw/state-change gate, then multiplies `record +0x0C`
by the float at `global[0x4F04F4] + 0x0C`, formats `%1.0f`, replaces its
decimal marker through `0x4188F0`, and joins it with `global[0x4F04F4] + 0x08`
using `%s %s` before drawing at `0x403F76`. The resource text, generation-match
outcomes remain observation inputs. `bind_scene_dashboard_locale` supplies the
marker and scale and resolves the preset's null `+0x08` suffix to `"(null)"`;
host CRT and x87 numeric-formatting edges are not guaranteed bit-identical.
`scene_dashboard_core_callback_plan` now dispatches this branch when the
selected object's observed `+0x18` field is nonzero, threading the same updated
dashboard state into it and sourcing its cached position from callback record
`+0x138`. The inner type-5 distance choice remains separate from this nonzero
branch gate; labels, resource generations, scale, and suffix remain inputs.
The composer also reaches the type-0 supplemental time rows only when the
selected-type/global-result gates pass and the displayed record count exceeds
one, then runs the `+0x10` and `+0x14` metric caches in order for either branch.
It reaches the final auxiliary `+0x4C` readout after both metric rows on every
callback path, overriding its redraw flag with the shared full-redraw result.
Supplemental record indices/text, metric field bits, resource generations,
locale globals, the auxiliary glyph advance, and external strings remain
supplied observations.
The result also exposes `ordered_text_calls`, concatenating the recovered
Position/Lap/final-time sites, the reached selected branch, supplemental rows,
both metrics, and the auxiliary rows in callback order. Formatted arguments
remain separate because several values come from runtime resources or raw
stack state.

The null-font fallback in `0x44DC70` has a distinct renderer path: it invokes
`0x44D6A0` twice at `(x+1,y+1)` and `(x,y)` with separate global colors.
`0x44D6A0` brackets each string with `0x415090`/`0x4150A0` (both are `ret`
stubs), replaces bytes below space with `#`, and draws each character through
the built-in 8x8 bitmap routine `0x44D780`. Marked pixels go through
`0x449E90`, which selects the `0x449F80` mode-5 writer or `0x449F30` writer;
`0x449D70` computes the destination as `surface_base + pitch*y + 2*x`.
The glyph-pointer table at `0x4D7848` is decoded into the 96-entry
`include/viper/scene_font_8x8.hpp` dataset by `tools/extract_scene_font.py`.
Color-mode packing and pixel addressing are projected below; actual device
surface ownership remains a runtime input.
`raster_scene_dashboard_builtin_font` now projects one of those 8x8 passes
from the recovered table, applies the signed-byte `#` replacement, clips to the
renderer rectangle, and emits mode-specific 16-bit or 32-bit pixel writes.
Call it twice in order with the target's shadow and foreground colors to
represent the two `0x44DC70` passes. Evidence:
`ev_ccbcc893f64ac589352c9f038c356fbad734e92ae8b8dc4986d8e80125a54a7e`,
`ev_530a782028b247007aab203e8ab52d18b606f9b47ea2ac4fffd1c8904092d1ba`,
`ev_6e1559c698437961095f7e96979855145b53c582720b4a5e5dc8801d71e83ac4`,
`ev_53609ca437c9fbc09290b9b318bc48491c39867f51ecade530a793b1c2128e48`, and
`ev_fbc394d4ee7c793a2f5d41a8a3cb69e18f4f8ce02173603c4ed062669dccde85`,
`ev_52a6355700fc4b4eeddb773cb0d04e767683d5659a32e8fc9bb815c4668f88cd`,
`ev_72b883caf50dce68e3681752d8daec12677322290498a3b2a0999ac1b683abb7`, and
`ev_403930b259723ca4ddf0f8c3acfa85ffd5ccf5eb624e30cf2abdd4f3ade55245`.

`update_scene_dashboard_final_time` models the other type-0 branch when the
global `+0x40` result is true. It changes dashboard kind `0x4CF9EC` to zero,
sets redraw bit `0x80`, and requests that rectangle on a non-full redraw. The
RaceTime resource call at `0x403820` is gated by full redraw or kind change;
the formatted `%s` value call at `0x403848` runs on every visit to this branch.
`scene_dashboard_time_for_ordinal` models `0x424370`: it scans the supplied
`DAT_005086A0` count of `0x48`-byte result rows and matches ordinal byte
`+0x41`. For each matching ordinal it updates the candidate from `+0x0C` before
checking flag byte `+0x40` bit `0x20`; it stops on the first matching row with
that bit clear. If all matching rows have bit `0x20` set, the last matching
row's float is returned; no ordinal match returns `-1.0`.
`format_scene_dashboard_time` models the true second-argument
branch of `0x425A40`, including integer-minute/second conversion, hundredths
from the fractional second, the marker byte supplied from `0x4F04F4`, and the
`99:99%c99` cap at 60 minutes. The binary's `0x4BD1F8` conversion helper sets
x87 rounding control to truncate before each `FISTP`; extended-precision edge
cases can still differ in the portable formatter. Other selected-object types
remain unmodeled.

`update_scene_dashboard_metric` models the subsequent record values at `+0x10`
and `+0x14`, with float-bit caches at `0x4CF9D8` and `0x4CF9DC`. Both cache
DWORDs start at `0xBF800000` (`-1.0F`). The callback uses `FCOMP` then tests
only status bit C3, so equal and unordered comparisons both follow its cache-hit
path; the model retains that NaN behavior. On redraw or cache miss it updates
the cache and ORs dirty bit `0x10` or `0x100`, requesting that rectangle only
when the caller is not doing a full redraw. Display requires a signed raw-DWORD
comparison greater than `0x3DCCCCCD`. It selects a type-3 format, type-5
format, or default time format and projects the corresponding draw at x=222
for `+0x10` or x=342 for `+0x14`, both at y=50 with draw flags `0x21`.
Cache-update visits also
register the Best resource with bit `0x80` in `0x4EB484` for `+0x10`, or the
Last resource with bit `1` in `0x4EB2BC` for `+0x14`. Above-threshold
non-type-3 paths refresh the matching resource when its generation differs
from `0x4D3868`; type 3 uses its global embedded string and skips that refresh.
Raw field meanings and external resource values remain inputs. String-pointer
arguments can be resolved with `scene_dashboard_crt_string_argument` when the
raw pointer bits are known. Type 3 formats
`%1.1f @ %1.0f %s`, using the metric and `record[+0x18] * global[+0x1C]`,
then passes the result through `0x4188F0` to replace decimal points with the
global marker. Type 5 formats `metric * global[+0x0C]` as `%1.0f`, applies the
same marker replacement, then joins the supplied prefix and `global[+0x08]`
through `%s %s %s`. Other types use `format_scene_dashboard_time` and join the
result with the supplied prefix through `%s %s`. The source format literals
are at `0x4CFD98`/`0x4CFDDC` for type 3 and `0x4CFD84`/`0x4CFDC8` for type 5
(`+0x10`/`+0x14`, respectively). Their outer draw formats are `0x4CFDAC` /
`0x4CFDF0` (`%s`) for type 3, `0x4CFD8C` / `0x4CFDD0` (`%s %s %s`) for type
5, and `0x4CFDB0` / `0x4CFDF4` (`%s %s`) for other types. Host CRT and locale
behavior can differ at formatting edges.

`initialize_scene_dashboard_locale` models the data writes in `0x418500`.
It selects the first 13-DWORD preset when the first byte returned by
`GetLocaleInfoA(0x400, 0x0D, ...)` is ASCII `'0'`; otherwise it selects the
second preset. It copies the first byte returned for locale item `0x0E` as the
decimal marker and preserves the two bytes returned for item `0x16`. The
selected table is copied to `0x4F0500`, eight bytes after the object base
`0x4F04F8`. The two raw preset tables are at `0x4D3870` and `0x4D38A8` in the
verified `race.bin` target. Table word 1 supplies object `+0x0C` (the metric scale) and
table word 5 supplies `+0x1C` (the TestTrack/type-3 multiplier). Words 0 and 4
land at `+0x08` and `+0x18`; both presets contain zero there. They are raw
pointer fields passed to `%s` paths, so this model preserves their bits. The
target VS 1998 formatter substitutes the literal `"(null)"` for a null `%s`
pointer; `scene_dashboard_crt_string_argument` exposes that behavior. The
separate DWORD at object `+0x04` is initialized to `0x3F800000`. Evidence:
Ghidra pseudocode for `0x418500`,
`ev_f70d5341b32b053c7f8aa67013bccb1e5bd82a99a69c25313ec156d794e58e96`, and
formatter `0x4BFA90`,
`ev_fb5adb4b0c8c3e7e9334d26b214b6a877b2fdb3cc9f29fef9d7eb701c0e05559`, and
the `0x44DC70` formatted-text wrapper, which calls `_vsprintf`,
`ev_1e6c19c0c8c3f2593cccef553464796bed40edaebbd0698a3cb35be6c0bc88`.
`bind_scene_dashboard_locale` projects table word 1 (`+0x0C`), table word 5
(`+0x1C`), and the decimal marker into the matching LapTime, TestTrack,
supplemental-time, metric, and auxiliary-readout observation fields. It also
resolves the zero `+0x08` TestTrack/type-5 string and zero `+0x08`/`+0x18`
metric strings, plus the auxiliary `+0x18` heading string, to the target
formatter's `"(null)"` output. The auxiliary `[esp+0x18]+5` source remains
unknown; locale state keeps the global pointer words raw.

`scene_dashboard_auxiliary_readout_plan` models the record-derived block at
`0x404313` without assigning semantic names to its `+0x4C` field or global
multiplier. The callback multiplies the two raw floats, stores the result as
binary32, converts it with `0x4BD1F8`, and formats the integer with `%d`. If
the resulting text is longer than three characters, it substitutes the fixed
string `999`. The draw's x coordinate is
`0x22A - (zero-glyph advance * displayed character count)` and y is 45; the advance is
the signed 16-bit entry for byte `0x30` at font `0x4EB304 + 0x204 + 0x30*2`,
as measured by `0x44DB10`, and remains an input. The numeric row at
`0x4043E8` is emitted every visit. On a full redraw, the callback draws the
string at runtime global object `[0x4F04F4] + 0x18` at `(560,45)`; instruction
`0x418370` initializes the global pointer at `0x4F04F4` to `0x4F04F8`. Its second
string draw at `(513,22)` reads `[esp+0x18] + 5`; no write to that stack slot is
observed in the callback body, so its source remains unresolved and is kept as
a raw input rather than attributed to a caller. Both strings' meanings remain
unknown.

`scene_dashboard_supplemental_time_plan` models the later type-0 rows from the
same raw callback. It emits the `+0x24` pair when the displayed count exceeds 1,
the position is not 1, and the signed raw DWORD at record `+0x24` exceeds
`0x3DCCCCCD`; it emits the `+0x1C` pair when the count exceeds 1, position does
not equal count, and that raw DWORD exceeds the same threshold. These are signed
integer comparisons on the stored float bit patterns, so the model accepts raw
DWORDs and preserves that behavior. The value formatter compares the record
float against `30.0f` (the DWORD at `0x4C7070` is `0x41F00000`), keeps the
larger value, formats `+0x24` with `-%03.1f`
and `+0x1C` with `+%03.1f`, then applies the global decimal marker through
`0x4188F0`. The companion text comes from `0x45DFB0` using record indices at
`+0x28` and `+0x20`, respectively, and the draw receives the returned string
pointer plus five bytes using `%s`. Each pair draws at x=83/87 and y=25 for
`+0x24`, then y=37 for `+0x1C`; draw flags are `0x0A` and `0x09`. Color
selection compares the same raw DWORD against `0x3F800000` and `0x41F00000`,
selecting globals `0x4EB488`, `0x4EB314`, or `0x4EB3F4`. The model computes the
formatted values and can resolve the suffixes from a supplied model of the
runtime string table; callers may still provide explicit strings when the
table is unavailable.

`SceneDashboardStringTable` models the shared table used by `0x45DFB0` and
`0x45DEB0`. `0x45DE40` clears 16 entries of `0x170` bytes and resets the
exclusive entry count. The setter stores the raw mode at entry `+0x00`, the
mode-dependent byte at `+0x04`, its first string at `+0x05`, its second string
at `+0x12`, and a raw pointer/value at `+0x134`; it also marks the index
initialized and raises `DAT_004DC718` to at least `index+1`. The getter returns
the entry base for an index and the dashboard reads its `+0x05` string. The
supplemental-row model now accepts this table plus the record's `+0x20` and
`+0x28` indices. The portable setter rejects indices outside `[0,16)` because
the binary's setter itself has no bounds guard. The constructor at `0x463920`
feeds the setter its `param_1[0x65]` index, raw mode `*param_2`, and strings at
`param_2+1` and `param_2+0x11`. Its caller `0x45CAE0` passes a 200-byte car
record as `param_2`; the strings therefore begin at car-record offsets `+0x04`
and `+0x11`. The loader also copies the `+0x11` string into a 64-byte local
buffer before parsing the record. Their runtime contents remain observation
inputs; the mode-2 callback is now traced to vtable slot `+0x84` target
`0x4A1B70`. That method returns the byte at ChatLine object offset
`0x1C8 + index*0xD9`; `scene_chatline_virtual_row_flag` reads it from bounded
object bytes, and `scene_dashboard_string_table_mode_2_predicate` composes it
with the global-presence and object-state (`+0x04 > 4`) gates. Constructor
helper `0x4A1E80` sets that state to 6 when its `0x49FA00` lookup result is not
`-1`, otherwise 3; the predicate model can derive the state from that result.
The same virtual method is called by `0x40A230` for each index below the
multiplayer object's record count; a false byte routes that row through
`0x41C9F0`. The callback also requests the state label from `0x4A1680` (which
performs one-time localized string registration) and calls `0x4A1AF0`, whose
observed body is a no-op; those label-registration effects are not projected.
The `0x45DEB0` string copies are unbounded `strcpy` calls. The portable entry
now uses bytes up to the first NUL, counts the copied terminator, and flags
when the first copy overlaps the second string/raw value or passes the entry
end, and when the second copy overwrites the raw value or passes the entry end.
It reports those extents without writing past the typed entry. The accessor's
index scale is `0x5C` DWORDs, or `0x170` bytes per row.
`make_scene_dashboard_string_table_set_observation` now derives the setter
arguments directly from the observed records: a signed table index at runtime
`+0x194`, mode at configured-row `+0x00`, C strings at `+0x04`/`+0x11`, and the
car object's `this+0x3C` address. It requires the runtime span to reach
`+0x197` and a complete 0xC8-byte configured row, and rejects either string if
no NUL is present in the supplied row span.
The setter index is read from runtime `+0x194`. `0x45CAE0` writes the
zero-based configured-car ordinal to that slot before calling `0x45EC20`; the
`0x463920` constructor then uses the same value as the `0x45DEB0` table index.
This ordinal is separate from the CARF field mapper, which ends at `+0x190`, and
the model-name copy at `+0x198`.
Evidence: `0x45DEB0`,
`ev_4a089eafa32a1c572287085b7859506c0a2696827c6cbb363dd7a58acd1e4c73`;
initializer `0x45DE40`,
`ev_e9c13dcf5015049a08660565fbfb071c8bc007cc89c3b40e78d9e40b123bd194`;
`0x45DFB0`,
`ev_cb07271a162f3e082fba45cdbd660a21b95265614694da6ecba4d4b2a80db3ed`;
and constructor `0x463920`,
`ev_b49bd77798d2eb9f50d5c282a91ae0e40234bd80899de1f9d2925f4270c57795`;
car-record caller `0x45CAE0`,
`ev_0c21c172a16c58e01c47ed385c88c6018c7190ca4bd9f9ecdbedda281414dedd`, and
mode-2 helper `0x49C5C0`,
`ev_5fa48d7a2fb33c6cb799bdac5006978c1713ba54d875c72b8010bcf535dc5beb`, and
multiplayer caller `0x40A230`,
`ev_828668e5f39f3cf2624071bd6ab258f6b1ebab1b936305c1ac5365b69905c7d0`.
The `0x4A1B70` method bytes are `ev_8bab27b53f2a85675fe522c535884a3b680ece57e806e58de08c6cff8ac91d64`;
its `+0x84` vtable slot is confirmed by table bytes
`ev_7dc0242135890d7d129ba556455e7b86e3e4d84058429c839e4354a7e65ae2c2`.
The state assignment comes from `0x4A1E80` (`ev_f9da3d0a0e89aa7d741623719f0dbf0c27651881cce21c6bc3e069723b6817f7`);
the status-string helper `0x4A1680` is `ev_c63c6b92c1d8de5b9b3807364e549757e75855b1d24e4b25a70589e19038b253`,
and no-op helper `0x4A1AF0` is `ev_c3a1675ce0583c27c7db17d93fdf96d8dcf71a1a80514439627eb46161593ba2`.
Fresh focused analyses of `0x45DE40`, `0x45DEB0`, `0x45DF90`, and `0x45DFB0`
are preserved in `metadata/car_string_table.evidence.json`.

Evidence: `ev_9aa46bc6450ec1572ea865ac9148625242d6d5ef645c945de5d2100e78982c22`
(`0x4035E0` through `0x404427` bytes), `ev_5f60fd107734e816e45359b06c10c71e1300a8c15309a630ac4de08ed745912e`
(`0x4052D0` wrapper), `ev_7faeade0d0ba5c93cbf5b7b4c55ba3a685a1d08e53d61c480824af8ef34485ed`
(`0x4052E0` bytes), `ev_a058b9f23799a14959f9a4c014c5676ed3461968e94bbb8ac13c1c3cb3687e65`
(`0x404970` pseudocode), `ev_855176c7a0ba0d4c1d302a40d88e7f25684b0756ea29bb586df919457638acef`
(`0x405100` pseudocode; `ev_631679a7035edbc1fd38ef434b58b907ee55519fc391e5cb8dbe36498264eb5b`),
`0x44AFD0` (`ev_bf174d2974c6824e088d742d89366c996c535a58e0e32c65859cffa7e4352794`)
and `0x44A730` (`ev_f1489e5dba972a665d6c590810af0231be270ca328d2d981588a3af4f9cf5454`),
`ev_ad201f203863318288134c8b51aa52799fe701e8685d0eb9325ecf3ed6318ced`
(`0x404860` predicate),
`ev_7bdc139b426c6735e0f9ea713d2db0343c769b79c35abae2b7dfde12d0945912`
(`0x4CFC54` through `0x4CFE23` resource and format strings),
`ev_c239bd9600ba6ec7e141b2513e6cc655c94447e69be32b37cf391b7efbdef212`
(`0x4CF9F0` through `0x4CFAAF` rectangle table),
`ev_ccbcc893f64ac589352c9f038c356fbad734e92ae8b8dc4986d8e80125a54a7e`
(`0x44DC70` text formatting/draw helper pseudocode),
`ev_e084d5a389130370e1942eb73e1d21f8e748a24b63e5f6cbddb16578eacd81ec`
(`0x44DC70` confirmed flag/x/y interpretation and alignment behavior),
`ev_78c8e4847480384f186f62b3fcb3a2d174f3f355bb757e317833f1c2f98a3dd2`
(`0x4CFC68` through `0x4CFCE7` format bytes),
`ev_86ae6fca3678684d15403f3d8239222558c65d001daa28e9b0b11664d8e87f20`
(`0x4CF9C8` initial cache DWORDs),
`ev_292026c78d436479e120eac65d17251132c4cf0061f50cd408b70ab7de93919f`
(`0x4CF9D0` through `0x4CF9EF` cache initial values),
`ev_e29e2785cd6a60ba242fe990c5f9e0c503c83142aaa7c4ffc96de5e47a5ae5cf`
(`0x4EB300` initial bytes), and
`ev_7a5319f4992aea57e374abb47f8f67584a0fa991506ab172c9b4dc4698f79c39`
(`0x4EB40C` initial redraw-mask DWORD),
`ev_b0c36c02d2bce51a4858ad708f5a44434fdddac1760ab1c4273ec69bac9955a5`
(`0x424370` result-time lookup pseudocode),
`ev_533b5a14e4bea364640758dc5bedac975145c7f208ffbb0ed11a076fb66b89eb`
(`0x425A40` formatter pseudocode),
`ev_c2d4034f91d9143a48ae59ae4d3ddcc3fe9e95836e98af1865688ccdcfdee38e`
(`0x40399A` LapTime and supplemental-row instructions),
`ev_7c209de09110610d553217a7870af459c8924c8668cfd5ab10486b4f2bc1e01c`
(`0x404313` auxiliary readout instructions),
`ev_de46144a1f36500f47669f3bfd9172932dbae0a2ea5bca5ea280a828596981f8`
(`0x418370` initializes the `0x4F04F4` pointer),
`ev_91922500710b23f18621ab91f0c33e44f5e07487cbd09559fe30d08f945f031f`
(`0x44DB10` font-character advance lookup),
`ev_9c7c344373555507e820d06a1b821145f5e949553f218ebac870593ada64e8f2`
(`0x403CA8` onward TestTrack and dashboard instructions),
`ev_23ff2240386e6afd3ce12e5d14c67f89c7f5338c187a39ef57b14302ee1279ec`
(`0x4BD1F8` truncating x87 conversion bytes), and
`ev_62996ba3c3bceb85f7d438be906a4dbfd79e216095611e00a06f602060b3052e`
(`0x4C77F0` binary32 multiplier), and
`ev_9aa46bc6450ec1572ea865ac9148625242d6d5ef645c945de5d2100e78982c22`
(`0x4035E0` raw callback bytes for the supplemental rows and metric gates),
`ev_6c80c040596899843887752a41bafee166212fd3cdd5c4edddaade1d82d33848`
(`0x4CF9D8` and `0x4CF9DC` initial cache bytes),
`ev_3ae77e8b46df7121a744ae17f063fdf5e7cc7c5eda71fc011092e3e9f86dde8f`
(`0x4CFD80` through `0x4CFE1F` format strings),
`ev_092f675db87201244a1925ccc3c66acb3f06bf5f9ba8d76516103eca6336980a`
(`0x4BD190` `_sprintf`), and
`ev_7eb7c9b1a0ae2ddbf372ede4cfd1cc9fd4c1f0ff91eb8b0dbe11746977e5c09d`
(`0x4188F0` decimal-marker replacement), `0x45DFB0`
(`ev_cb07271a162f3e082fba45cdbd660a21b95265614694da6ecba4d4b2a80db3ed`,
record-string table indexing). The procedure inventory is
`ev_5a636f87d48bbf75ceaa1ab190853255604f1b305c99ddfa2bce44ab6045caf5`; it
contains no entries for `0x4035E0` or `0x4052E0`, consistent with treating
those callback blocks as raw disassembly rather than complete function
boundaries.

### State-1 overlay callback

The state-1 render slot `0x41A690` indexes the pointer table at `0x4F0AC0`
using `0x45C1F0`; a null entry returns without drawing. Otherwise it installs
the render context and calls `0x41A9B0`, `0x4146B0`, `0x41A790`, and
`0x41AA10`. The callback cycles a masked counter at `0x4F0B64`, chooses one of
two color globals using byte `0x4D3DD4`, and, when that byte and the event
predicate permit, updates the raw coordinates at `0x4F0B58`/`0x4F0B60` from
the event relative to `(320,240)`. Their semantic meaning is not established.

`0x41A790` switches on numeric page `0x4D3DD0`:

| Page | Text calls observed |
|---:|---|
| 0 | `Help` at `(0,240)` |
| 1 | `Info` at `(0,0)`, then eight row pointers from `0x508B38` with `0x50` stride at `(1,20..90)`; the selected car ordinal chooses the highlighted color |
| 2 | `n %3.3f m   %3.3f mph` at `(240,80)`; a separate line/geometry path has additional gates |
| 3 | `Save?  Hit any key to abort, or:` at `(100,100)` and `G : Save line as global default` at `(100,110)` |

`scene_overlay_text_plan` records those text addresses, coordinates, row
sources, and raw color globals. It does not reconstruct the page-2 geometry,
the renderer/font behavior, or the state mutation behind the event tables.
These functions are decoded in `disasm/engine_.text.asm`; Ghidra does not
assign them procedure bodies, so the evidence is byte-level rather than a
decompiler result. Evidence: `ev_1c945d51e491bc36d909a8c3b19dd106df27581b047d8dbd6a3ac4859c2e9704`
(`0x41A690` through the page switch), `ev_216bea0932599a297eaa4be7a8f989b9b4e0c5f6b3eeed93a2a48bb59fa37cf1`
(`0x4D3EE0` text bytes), `ev_6d64ddab9d769214d782d8537b536965f57e61507fb0969742f927cb5c078ca7`
(`0x4D3EA0` prompt), `ev_f08669474b099fac51dbd49f0dc88c334557534d77d7dffe24d9bed79428d936`
(`0x4D3EC4` prompt), and `ev_1668f3cde5d0347c83a94718620edcd8717f93df954ee741fdcbd8b8f50d87d7`
(`0x4D3EFC` `Info` string).

`handle_state1_input_event` applies the state-1 input callback's observed
two-stage routing. Events `0x3F` and `0x68` select page 0, `0x69` selects page
1, and `0x6C` selects page 2. Event `0x73` selects page 3 only when the object
at `0x45C2C0` has byte
`+0x25 == 0`. On page 0, `0x48` selects page 1. On page 2, the second table
maps `0x30` to the `0x4D3DD4` toggle; `0x42` snapshots the two free-camera
coordinates; `0x51` increments raw field `+0x4D3DF8` and `0x57` increments
raw field `+0x4D3DF4`; `0x71` and `0x77` subtract the same step from those
respective fields; `0x76` clears the controller scalar and reloads the saved
coordinate pair from the object at `0x4F0B24`; `0x56` performs
the temporary camera-controller increment and reports its indirect `+0x40`,
`0x419E80`, and `0x41D1D0` calls; `0x62` restores the snapshot. On page 3, the
external `0x4BDE40` key translation is supplied as an observation, and the
page returns to 1 regardless of whether it yields `0x67`. The model preserves
the callback's return value and its conditional clear of byte `0x4F0B44`.
Evidence: table bytes `ev_13323fa7894fe0cec8fa88d33bca8c9f57886dbf32629ec69623bbf0a23d4fdd`
(`0x41A580..0x41A638`), full second-table map `ev_38d8ae7e966699efa20b713a60f9918a280323fb199b8d8cf27b21d3b134068c`
(`0x41A5FC..0x41A643`), and callback bytes
`ev_09f7712f6e55467366932195f79c6279b9bbd38e6678175b3a586814da8fca63`
(`0x41A370`).

### State-2 physics overlay callback

State 2 selects render callback `0x423380` and key callback `0x4236B0`. The
render callback installs its context, draws `Physics` at `(10,10)`, then draws
`COLLIDE` and `WALLS` at `(160,10)` and `(160,20)`. Their colors are selected
by bytes `0x4D4B48` and `0x4D4B4C`. It reads the selected car's record through
`0x45DFB0(0x45C1F0())` and the subrecord at `+0x134`, draws
`R: %1.0fF  E: %1.0f` at `(320,100)`, `s: %5.2f t: %5.2f b: %5.2f c: %5.2f`
at `(320,120)`, `%3.0f MPH  V:%3.0f MPH %5.0f RPM (%c) DIST %4.0f'` at
`(320,140)`, and `LAT %5.2f G  LONG %5.2f G  TOT %5.2f G AERO %5.2f G` at
`(320,150)`. The names `s`, `t`, `b`, `c`, `R`, and `E` are preserved as
format literals; their field meanings are not inferred. It then formats global
coordinates from `0x53A4A4`, `0x53A4A8`, and `0x53A4AC` at `(320,160)` using
`(%6.1f %6.1f %6.1f)`. The callback also invokes `0x4236F0` four times while
walking four `0x38`-byte records beginning at subrecord `+0xA8`; the
bar/geometry helper remains opaque.

The key callback recognizes `0x63` (`c`) and `0x77` (`w`) to update the two
flag bytes. For either key it writes 1 when the previous byte was below 1 and
otherwise writes 0; other key values return unhandled. The portable
`handle_scene_physics_overlay_key` and `scene_physics_overlay_text_plan`
preserve these flag and text effects, leaving the wheel visualization open.
`scene_physics_wheel_bar_plans` now projects the four calls to `0x4236F0`.
Their positions are `(215,295)`, `(215,345)`, `(240,295)`, and `(240,345)`.
Each fill width is truncation of `+0xA4 * 40`; positive widths emit a 20-pixel
high fill rectangle, and all bars call `0x44C950` with radius 20 for an
eight-way midpoint-circle outline. The two raw components
at `+0x9C/+0xA0` are scaled to magnitude 1.2 only when the caller's reference
vector exceeds 1.2, then produce the indicator endpoint with the observed
`-20/+20` factors. The helper also projects its format argument as
`(input - 272) * 1.8 + 32`. The projection also preserves the observed
per-helper call order and arguments: conditional fill via `0x44A1B0`, circle
outline via `0x44C950`, indicator line via `0x44C260`, then formatting at `0x4D4C54` and
text submission via `0x479580` at `(x+5,y)` with size `0x11`; its operation
record keeps the `0x4D4C54` format source, computed numeric argument, and
formatted string. The format bytes at `0x4D4C54` are `%1.0f`; the call uses the
VS 1998 `_sprintf` wrapper at `0x4BD190` before passing the resulting string to
`0x479580`. The portable model formats the computed double with `%1.0f`; exact
legacy CRT rounding remains approximate, and the text backend call is not
rasterized here. Color values, caller stack reference components, and format
input remain observations. Pentium FDIV and x87 rounding remain approximate.
Evidence:
helper bytes `ev_250b08ea538ca564c22f50a26e8dd6ede4a6e1abf4e02bef9ca807ef65b72b75`,
constants `ev_c4830cfd2343199b1ac5a38f43632cf39c5e73bebe8b1d5f31b6391ed6580dfb`
(`272`), `ev_221695e4f65c53a01436ecb21628470afd2e93b888df006a940273beb56d2f6b`
(`1.8`), `ev_113edba29abda596f357360c8f1154a57745773067114d5c8de2034b73abe66f`
(`32`), `ev_f5cf488084b8334bbca9afb72fa2ceb17bce626fe7ee4539de9a034ec860dbb3`
(`40`), `ev_559997cb7591eb4668dfe8bffd2ab90177d9847c7a0cf544759aedcf2c2cb257`
(`1.2`), `ev_9bb6fd3e7bc1b1b1db1df32c2d6d5b8d00904ecf9db2a071bfb85631d50d2ec3`
(`-20`), and `ev_8737a67012c86c59b7d0d7c2eb4a1c7a2ca83f90258b63531f827e54a800a7f8`
(`20`).
The format bytes are `ev_df8f7585aecb66ed48b18aeb711c44a9c3ea21feaa77661a3660fe1c5b941138`; `0x479580` text submission is
`ev_8ea0d058927243f23b31fa69d01a4bcf40c15e14d826b47e3affabc9e68dfe54`, and
the VS 1998 `_sprintf` wrapper is `ev_d511af6851b2d17d5a5a65129cda2a02ca874f07aa7a7bb722bc0df633f5698c`.
Evidence: `ev_ba8387a20a6ac6b8e8e8614c4dcc5efcd8688e97e5dfc168aac016944cebb009`
(`0x423380` bytes), `ev_5ae98a77cb54cf090270cf928c9325b9e81003bfb575e3b0879a5968fb5aec9e`
(`0x4236B0` bytes), `ev_6e1b05d0565d20149d1d5471c5aeb33e787515af323ecb43aa6ed465a4201282`
(`Physics`), `ev_e601e682184f629b72f96697cb28318acde88c6cac6595eeb4610cde40d74d75`
(`COLLIDE`), `ev_5e1d20548e0716c0ae8e7587d0fbdf19d144a757d5819a717c8f812bb3c7e9de`
(`WALLS`), `ev_457534c6cdcd4ec197b90a9844484387ff3862693933f1b72248ed27a1732f78`
(`R`/`E` format), and `ev_f7d23d10ae87cfa45650b1190078e54feebde347e1d698afeba4fcbeba1d3c3c`
(`0x4D4C40` coordinate format), `ev_bd2902caf666727aebbc575cdde20e3ef1b958bdde0bcdcae3684e2631f29c76`
(`0x4D4BB4`), `ev_a0f97ed72c595677677e2096276367df10d4538fe747b7a513c1ceaf8a4ce9a0`
(`0x4D4BD8`), and `ev_380744216287c9278bd7d172e4ff7148c7c727e2900168eee4e4ad0f56ebfe67`
(`0x4D4C0C`). The `0x44C950` body bytes are recorded in
`ev_9bd15fa02ca8378379152537bc3764b404783eed4cff656f2bd6f3203dded73c`;
its loop updates the midpoint decision term and emits the eight symmetric
`0x449E90` points until x passes y.

### State-3 graphics overlay callback

State 3 selects renderer `0x40D570` and key callback `0x40D650`. The renderer
installs the supplied context, fills the rectangle from `(1,1)` to
`(0xE6,0x22)` through `0x44A1B0` using color source `0x4ECCC8`, formats the
float at `0x4D11D4` with `0x4D11D8`, and draws it at
`(10,10)` with color source `0x4ECCD8`. It draws the instruction string at
`0x4D11EC` at `(10,20)` with color source `0x4ECCC4`. It queries action masks
`0x40` and `1` through `0x451890`, then draws the strings `FOG` and `LIGHT`
at `(160,10)` and `(160,20)`. Each row selects color source `0x4ECCFC` when
its queried bit is set and `0x4ECCF0` otherwise. The strings and initial
float `1.0` are present in the data block beginning at `0x4D11D4`.

The input callback maps `0x27` to reset the LOD float to `1.0`, `0x5B` (`[`) to
subtract the step at `0x4C72EC`, and `0x5D` (`]`) to add it. The step's stored
binary32 value is approximately `0.2`. Keys `0x66` (`f`) and `0x6C` (`l`)
toggle masks `0x40` and `1` through the action-state helpers `0x451610` and
`0x4513A0`; wrapper `0x40D740` checks the current action state and calls the
opposite helper to toggle it. Other keys take the clamp-only path. After key
dispatch, the callback clamps the LOD value to `[0.2, 20.0]` using signed
comparisons of its raw float bit pattern, and always returns zero. The portable
model records the two action bits and overlay colors. Its
`scene_graphics_overlay_render_plan` retains the ordered fill and four text
operations, while leaving backend side effects to the renderer. Ordinary float
additions also do not reproduce every x87 extended-precision rounding case.
`scene_graphics_overlay_background_pixel_writes` now sends the observed fill
rectangle through the shared `0x44A1B0` clip, renderer-mode pixel conversion,
and address projection. `apply_scene_rectangle_fill_pixel_writes` can apply a
fully range-validated write list to a supplied framebuffer, including the
mode-1 preserved-high-nibble update. This reconstructs CPU-side fill writes;
surface acquisition and lock lifetime remain external.
`raster_scene_graphics_overlay` also routes the formatted LOD string, the
observed `[=down ]=up '=1.0` instruction, and `FOG`/`LIGHT` through the
recovered 8x8 glyph table and pixel encoding in draw order. The formatted LOD
text, colors, viewport, and framebuffer address are runtime inputs.
`apply_scene_graphics_overlay_raster` applies the background and glyph writes
in draw order only after validating every destination range; incomplete text
plans or out-of-range offsets leave the framebuffer unchanged.

The state-2 wheel-bar fill, circle outline, and indicator line now project
clipped per-pixel writes through that shared path. The indicator follows the
observed major-axis stepping and single-precision slope update from `0x44C260`,
with the final endpoint excluded. The `0x44C950` outline uses the observed
eight-way midpoint-circle stepping and preserves its symmetric pixel-call
order, including duplicate calls where the octants meet.
`apply_scene_physics_wheel_bar_pixel_writes` validates the fill, circle, and
indicator destinations together, then applies their writes in that exact
order. `raster_scene_physics_wheel_bar_labels` now takes each bar's formatted
`%1.0f` string through the shared `0x44DC70` layout and `0x44D0C0` atlas
rasterizer, preserving pixel writes in bar order. The style object's color,
font and flags, font tables, atlas bytes, and each bar's renderer/framebuffer
observations remain supplied inputs; this helper does not acquire DirectDraw
surfaces or infer those runtime resources.

Evidence: `ev_8bd7df33daf7008cf7470c1009769d94a5bb726432e62f1801e8eb6ec676fbf1`
(`0x40D570` bytes), `ev_e3c9bfbe5709843f76c437f4c75ec574f1058ea421f72bb5d2ee3a6f2c153063`
(`0x40D650` bytes), `ev_c3721d523bc21a5951fcb6a491fe58a1861bbed1a647b182a5f1ef50e8c20c3a`
(`0x40D740` bytes), `ev_c16f1d036b871cc2998eae183e8577c8dc4027e0969aef10f21525b1ec58e054`
(`0x4D11D4` data and adjacent strings), and
`ev_99ba07db3bf9457ce7a22b5a5067095a5178c5dac79aeca2e12cd8434d390a52`
(`0x4C72EC` step). Action helper pseudocode evidence:
`ev_8d59b239d712f9572921b40d017b3ae3dc58d38b00221468f2ee995db47c65d6`
(`0x4513A0`), `ev_b0d76e3659286234af3b07259af880fd103e6d5d7562e726ab67de748afe770c`
(`0x451610`), and `ev_cc1809c021c0736874a4793d5dcff0f748853ab6d7dfa2cd17fec047f79c8c50`
(`0x451890`).

For the scene-car vtable at `0x4C9420`, virtual `+0x18` points to `0x4648A0`
(the same target appears in the related table at `0x4C9450`). The `0x45C090`
submitter invokes each registry object's `+0x18(context)` method. The raw
instruction listing for `0x4648A0` shows an early return when the object's
ordinal at `+0x1C8` equals the selected-car index. Otherwise it calculates
squared distance against the camera-position globals, applies a stored
threshold and a further `0x448CD0` predicate, then obtains the object's
record through `0x45DFB0(+0x1C8)`. The passing path formats and draws text
using record offsets `+5` and `+0x140`; a second gated path also reads the
record at `+0x32` and object field `+0x7B4`. These are recovered draw inputs,
not identified labels or confirmed UI semantics. The body is present in
`disasm/engine_.text.asm`, but Ghidra does not define `0x4648A0` as a procedure,
so the available REA instruction observation verifies its entry instruction
and vtable reference rather than a decompiled function body.

Evidence: `ev_6cc8f1588a35e8baa2c53cb95e792f633255f28611b22c08ce40955fd8fecdd2`
(`0x4C9420` vtable bytes), `ev_96437c4a148c208eaa1a386e26296e1a897925082a9fb98eade10bc491c4eba4`
(`0x45C090`), and `ev_88e681bc916c6c91577973a83de771c96c24016dc10d84f76d831d0b8d5f7d94`
(`0x4648A0` entry instruction).

The base car constructor initially installs vtable `0x4C7F20`, whose `+8`
target is `0x431150`. That callback updates the four wheel objects with
`0x442530` and then consumes the body accumulators in `0x43ECE0` (documented
in `vehicle_setup.md`). Derived `NCAR` and `PCAR` objects replace that vtable;
their class-specific `+8` methods can call `0x431150`, as detailed below.
Thus the wheel-force and body-integration sequence occurs within the entity's
first virtual callback on the observed paths, before the dispatcher evaluates
that object's `+0x0C` result.

`0x45BD20` samples the clock helper `0x413BD0` near entry and exit and updates
global frame and elapsed-clock counters. The inspected call chain does not
show those measured deltas being passed to `0x43ECE0`. Constructor `0x43E490`,
called from `0x42FF90`, writes a scalar to body index `0x7C` and its reciprocal
to index `0x80`. The body routine uses the first value in its gravity
contribution and multiplies accumulated linear force by the reciprocal and
`0.016` when advancing velocity. The constant is applied once per body-update
call, so this is a fixed per-call integration factor. A separate physics-task
path does contain elapsed-time scheduling, documented below. The portable
helper `advance_body_linear_velocity` models the recovered linear velocity
step.

## Physics-task substep scheduler

World initialization at `0x45B6B0` calls `0x425420`, which initializes the
physics state and sets `DAT_00508774` to zero. It later calls `0x425540`.
`0x425540` sets the task state, registers callback address `0x425B60` with
`0x416730`, then waits in 10 ms intervals for the state to change. The
registered callback is a state dispatch block in the disassembly rather than
a Ghidra-defined function. Its normal active states call `0x4207C0`.

`0x4207C0` obtains a step count from `0x422740(&DAT_00508190)`. For each
returned step it calls `0x420D10` and then `0x421050`; when
`DAT_00508760 == 0`, it increments `DAT_00508774` once per step. The time
accessor `0x425790` returns `DAT_00508774 * 0.016`. The elapsed-time estimate
in `0x422860` is:

```text
((clock_now - previous_clock) * 0.001)
    + previous_simulation_time
    - (DAT_00508774 * 0.016)
```

On the elapsed-time branch, `0x422740` adds the binary32 constant `0.016` to
that estimate, multiplies by the binary32 constant `62.499996`, and converts
through CRT `__ftol`. Its observed helper uses x87 `ROUND`, so this is rounding
under the active x87 control word, not truncation. When `0x49BE40` is zero,
negative or greater than 2 is capped to 2, the anchor-recapture byte is set,
and the per-call catch-up flag is raised. When `0x49BE40` is nonzero, only
values greater than 4 are capped to 4. The mode-0 branch instead returns
`24 / DAT_004DC718` and can sleep 20 ms on its byte-wrap guard; mode 2 returns
the byte at accumulator `+8` and toggles it between zero and one. Other paths
default to one step. When the mode-1 anchor-recapture byte at `+9` is set, the
function stores current task simulation time and the integer clock tick at
accumulator `+4`, clears the `+9` byte, and still returns one step.
`include/viper/physics_scheduler.hpp` and
`src/viper/physics_scheduler.cpp` model these branches, both caps, anchor update,
and the mode-0 sleep guard. The portable estimate uses double intermediates
around explicit binary32 constants; exact x87 extended precision remains a
fidelity limit.

The factory and update path connects this scheduler to car-body updates.
`0x420480` allocates the `DAT_00507D0C` object array, then repeatedly calls
`0x4224D0` to deserialize a record, construct its class through `0x422580`,
and store the returned object in the next array slot. The factory recognizes
both `NCAR` (`0x438E40`) and `PCAR` (`0x4381E0`) records. The task's `0x421050`
callback iterates this same array and invokes the object's virtual `+8`
callback when virtual `+0x2C` reports it active.

`physics_task_fixed_step_dispatch_order` records the complete outer order of
`0x421050`. When `DAT_00508A40` is zero, each non-null object gets virtual
`+0x2C` and active ones then get `+8`; the dispatcher calls `0x427080`, then
visits the 16 auxiliary slots at `0x507D70` in nested row-major order, calling
`0x432300(first, second)` for every ordered pair of distinct non-null slots.
When the global is nonzero, the dispatcher calls `0x426A10` instead and skips
the normal object-update and auxiliary-pair passes. Both paths finish by
calling virtual `+0x0C` on every non-null array object. The model treats
`0x427080` as a helper event and `0x426A10` as a helper with a partial
alternate-time projection: slot selection, record interpolation, object dispatch,
and bounded command-parser observations are modeled below. Evidence:
`0x421050`, `ev_e91e243d2a75bd1f17a1d1dc64841e3b5122e94779806c54ebe0bc4421566982`.

`plan_physics_auxiliary_pair` projects each ordered `0x432300(receiver,
target)` call. It calls the target's virtual `+0x30` gate, computes squared
distance from receiver `+0x5C` to target `+0xE08`, and only when that is
strictly less than target radius `+0xE14` squared, adds the target's force
vector at `+0x234` multiplied by `(radius_squared-distance_squared)/radius_squared`
to receiver accumulators at `+0xDFC`. The target's three force components
therefore feed the receiver, in dispatcher argument order. The distance square
is spilled to binary32 before the radius comparison; radius squared remains
extended for that comparison, then spills to binary32 for the proximity
factor, which uses the legacy FDIV helper. Remaining x87 extended intermediates
are approximate. Evidence:
`0x432300` pseudocode `ev_093a726660d0859203c394c4b6a10b413bfe3b6bc47d73582e1b7837d83f1bf4`
and assembly `ev_04cfbeb623acd512909a581ee47905e7788fe86c1597666f2a7076b924c0d6d4`.

`plan_physics_auxiliary_pass` now composes those calls across the 16-entry
`0x507D70` table. It walks receiver and target indices in the binary's nested
ascending order, skips null entries and self-pairs, and applies each observed
pair through `plan_physics_auxiliary_pair`. The receiver's updated `+0xDFC`
vector becomes the input to its next target call. Pair inputs are keyed by
receiver/target indices; missing or duplicate entries invalidate that
receiver's accumulator while preserving later call ordering, and an observed
target dereference fault terminates the pass. With alternate time enabled,
the pair pass is delegated to the already modeled `0x426A10` path. This
composes the auxiliary portion of `0x421050`; its surrounding object callbacks
and global side effects remain in `physics_task_fixed_step_dispatch_order`.
Evidence for the loop and branch is `0x421050`,
`ev_e91e243d2a75bd1f17a1d1dc64841e3b5122e94779806c54ebe0bc4421566982`; the
pairwise target equation is supported by the helper pseudocode and assembly
cited above.

`plan_physics_time_slice` further projects the normal helper `0x427080`.
`0x426670` selects the signed slot `(simulation_counter / 6) % slot_count`;
zero slot count is the binary's divide fault. If the selected slot differs
from `DAT_00508A28`, `0x4272C0` updates ring state, and every array object is
then dereferenced without a null check: virtual `+0x2C` gates virtual `+0x20`
with argument `object[0x34] + selected_offset + 0x0B`. Afterward the helper
captures the selected record through `0x427150` and stores the selected slot.
When the slot is unchanged, it skips `0x4272C0` and the object callbacks but
still captures the current record. The post-transition slot offset accepted by
the model can be produced by `plan_physics_time_ring_transition`. Evidence:
`0x427080`,
`ev_6732cebdd4b39b1ea951510e30ba3ce6e56d0c30512aeb5defcba46dc3d7ed80`,
and signed division in `0x426670`,
`ev_8b70fb7e330efcb44e638651c35a76e28791191e1770bcf9fcf2a4d7c1c61a28`.

`plan_physics_time_capture` models the 11-byte record written by `0x427150`.
It copies the DWORD and byte at `0x507DB0`, clamps the next float to `[-300,300]`
and the following two to `[-4,4]`, maps each range to `[0,32768]`, and stores
the three low-word results at record offsets `+5`, `+7`, and `+9`. The x87
conversion helper changes the control word to truncate toward zero before
`FISTP`; this is reflected in the conversion. Division routes through the
existing Pentium-FDIV compatibility approximation when enabled. x87 extended
precision still depends on host `long double`. Evidence: `0x427150`,
`ev_f19a690178deb5d6c46c4981b20f796b1352c6316228f5b438448c6b89ecb7a9`,
constants at `0x4C7958..0x4C7980` (read evidence `ev_9da16e7bd1a981ed2ab1ea3bcad32f5b1c700e7a959cb550b5358c263e1355b6`,
`ev_a32ad6ea20b0822d4314eb2ef9eee5cd3ae6a75a8161163d4989c56535bbc8a3`,
`ev_29d3e19ef222183d5487c3827639a8401beafd872cbfab5279e032978cbea83`,
`ev_0f06f7093bfb3502fac93e97a202bc368c0632f20cdedf67ed364184856cd430`,
`ev_e1080640826757ac995dff6ec0695b4b9710a39a11c50541af3835a132819f52`),
and truncating `__ftol` assembly `ev_d537319812fd9d0a2da1c7a557df65490d4e37caf0006017ec0c6e5f0137b127`.

`plan_physics_alternate_time_path` projects the active `0x426A10` branch. It
selects `(simulation_counter / 6) % slot_count`, obtains the substep fraction
from the signed remainder times the binary32 `1/6` constant, and selects the
following ring slot only when the previous slot differs. Each non-null object
gets `+0x2C`; active objects then get `+0x1C` with current and next record
addresses adjusted by object `+0x34` and `+0x0B`, plus that fraction. The
current and next 11-byte records are interpolated into the global sample at
`0x507DB0`: the first DWORD/byte come from the current record, while three
unsigned 16-bit normalized values are mapped back to `[-300,300]` and
`[-4,4]` float ranges. On a slot change, the trailing parser reads command IDs
below 8 while a handler is present. The observed handlers for IDs 1, 3, 4, 5,
and 7 consume fixed payload lengths of 20, 28, 44, 14, and 2 bytes respectively;
unknown handler IDs still take bounded consumed-length observations. Handler
presence remains a bounded table-state observation. It then updates the
previous-slot global. When the slot is
unchanged, it skips parsing and sets `DAT_00508760`. x87 intermediates remain
host-approximated. Evidence: `0x426A10`,
`ev_c62985bb31746e21e9610af329debb7cc3081adcc7f6f468c5addf80fcb2f0e5`,
`0x426BC0` remainder/fraction assembly `ev_19e343c839ec1bb7045905ccfc4e9bd7c5674ed4fbe98dee29e33be5f03fe192`,
`0x426C10` sample interpolation `ev_c96e8668f387991b8e27677bbd50ff6fbfa984a0a76cc81a6d1793c9501391c1`, and the `1/6` constant `ev_cbf6642097c18046bb6b861a7850b8524db7e1db6998eb9789c00651cfdd9120`.

`plan_physics_time_command_payload` separately projects the known command
payload effects: ID 3's bounded event-buffer append and copied 28-byte payload,
ID 4's validation/panic gate, IDs 5 and 7's signed index lookup through the
registered-object table at `0x507D70` and reset/update calls, and ID 1's
record-ring update arguments. ID 1's cursor wraps by signed remainder after
increment. It reports unsafe index and null-object paths without dereferencing
them; queue/object memory writes are observations, not mutations by this
portable model.

That ring bookkeeping is now projected by `plan_physics_time_ring_transition`.
`0x4272C0` always sets `DAT_00508A34`. With old slot `-1`, it zeros the new
slot offset and resets the allocation cursor to the chunk size. Otherwise
`0x427050` writes a zero marker byte at `storage_base + old_offset + cursor`
and advances the cursor by one. The helper compares the signed wrapped sum
`old_offset + cursor + 0x600` against `0x1FFFFF`; ordinary nonzero-slot
transitions with no overflow advance the new slot to the resulting end offset
and reset the cursor. Slot zero or overflow enters the high-water/overflow
marker path, updates the generation when wrapping to slot zero, and may reset
the new slot offset to zero. The plan preserves the binary's 32-bit wrapping
arithmetic and signed comparisons. Inputs include the current slot-offset
table and prior ring globals. Evidence: `0x4272C0`,
`ev_035843b8b1092b7f7814d274836d5394dd4862d9389888999d8ae2769c45f047`, and
`0x427050`, `ev_699b74a5bf099f4cc59484e6ea3e7174a999ef165480917bba79771416b12f0b`.

The vtable bytes establish that `NCAR` virtual `+8` is `0x439200` and `PCAR`
virtual `+8` is `0x4384D0`. In both update methods, the static disassembly
contains a conditional call to `0x431150`; the PCAR call is at `0x43872F`, and
the NCAR method calls it at `0x4392BF` or `0x439336` on separate branches.
`0x431150` updates four wheels and then invokes `0x43ECE0`. Thus the fixed-step
physics-task loop does reach the car wheel/body update, conditional on each
class method's state branches. The PCAR update also calls `0x431150` before
writing its per-step car state record.

The main entity list does contain per-car scene objects created by a separate
world-record path: `0x45CAE0` selects `PCAR`, `ACAR`, `NCAR`, or `GCAR` for each
car record, constructs a scene object through `0x463920` (or subclass
`0x466840`), and appends it through `0x45C320`/`0x45C2E0` to
`DAT_00539430`. The scene object's vtable at `0x4C9420` has virtual `+8`
target `0x463EB0`; the `0x4C9450` variant uses the same target. That method
performs scene-object updates and calls its own virtual `+0x28` slot, but the
observed direct call graph does not include `0x431150`. The `0x463920` object
is a separate main-list instance built from the same serialized car record as
the physics object; the record-writing and loading chain is detailed in
`physics_task_objects.md`.

The fixed-step physics task is proven to reach car wheel/body updates through
its deserialized physics-object array. The world-record path separately
registers a corresponding per-car scene object in the frame dispatcher. The
scene and physics objects originate from the same serialized record; both
retain its ordinal from `+0x194` (scene object `+0x1C8`, physics object
`+0x510`). The scene object's `+8` method has no observed direct call to
`0x431150`, though indirect scene-update effects remain open. See
`physics_task_objects.md` for the traced writer/reader path.

The terrain query preceding wheel contact is modeled as a reusable sampler: quantized X/Z select a partition leaf, then the selected plane produces a height at the original X/Z. The wheel-contact wrapper runs this sampler only under the existing body-query gate and forwards its point, normal, and feature into the contact state machine. Track data loading is still an external input to this layer.

## Race input binding accessors

The central frame update repeatedly evaluates records using `0x446980`,
`0x446F40`, and `0x446F90`. The first reads a type/selector pair and resolves
device-button, controller-axis, keyboard-byte, device-selection, mouse-axis,
or mouse-button values. `0x446F40` uses a strict `> 0.2` activation test;
`0x446F90` tests whether the record type is one of 1, 3, 4, or 6. These
accessors are modeled as `evaluate_race_input_binding` and
`race_input_binding_is_active`; platform polling and higher-level binding
record semantics remain outside this layer. The direct value-copy and
difference fields, button flags, and debounced `0x5093BC` counter in the tail
of `0x43B290` are composed by `update_race_loop_control_state`; binding values
and type/active predicates remain observations. See `race_input_bindings.md`.

The primary steering accumulator at `0x509358` is reconstructed by
`update_race_loop_steering`: inactive bindings use the left/right value
difference, smoothing bias, and gain; the ramped path applies per-frame
decay plus active-input response and saturates to `[-1, 1]`. The two adjacent
analog channels at `0x5093F4` and `0x509320` share
`update_race_loop_analog_channel`. The first consumes binding `0x509328`,
scale `0x50934C`, bias `0x509370`, and rate `0x509350`; the second consumes
binding `0x509368`, scale `0x509330`, bias `0x509380`, and rate `0x509348`.
Their values and type/active results are caller observations. Both implementations retain
the signed or unsigned float-word comparisons observed in the x86 disassembly
instead of interpreting Ghidra's `(int)float` casts as numeric conversions.

`update_race_loop_43b290` composes cadence dispatch, steering, both analog
channels, and the modeled control-state tail while sharing the simulation
delta across all state updates. It returns the observed `0x444E10` / optional
`0x4170A0` call plan. Its 15 raw binding records are evaluated using the
post-refresh snapshot selected by the cadence result: the tick snapshot
includes the axis reset and `0x416D30` refresh. Platform input polling and
binding record contents remain caller-supplied observations.

The unconditional `0x444E10` call refreshes normalized mouse position and
three mouse-button bytes; on a periodic tick it first clears four controller
axis globals and calls `0x416D30`. The tick-only `0x4170A0` dispatch writes a
force-feedback effect parameter block with type `0x34`, clamps the converted
third argument to `[-10000, 10000]`, and submits it through available effect
interfaces. Opaque platform effects and input sampling remain outside the
portable model. Dossiers are in
`race_input_binding_accessors.evidence.json`.

## Outer race loop and frame submission

`plan_race_loop_outer_iteration` models one pass through `0x4020E0`. It keeps
the two local counters distinct: the startup/setup counter begins at 10 and
decrements every pass, while the other counter begins at 10 and decrements
only when `0x40A160` returns 3. The function routes through the timeout menu
or the one-time `0x49BEA0`/`0x425800`/`0x4035D0` setup sequence, models the
exit-to-page-12 path, the optional `0x4029C0` side branch and page-11 handler,
then dispatches the page before the active-frame/render plan. It also retains
the second active-state read after `0x49C110` and the `<1` gate for `0x447C50`.
When the timeout menu is reached, it composes
`plan_escape_menu_initialization` for the menu's one-time labels, resource
refresh gates, saved-page state, and four 13-DWORD button descriptors, then
optionally composes `plan_escape_menu_event`. The event model preserves input/render/state-callback/
global-key-handler ordering, the raw page selection writes, key-`0x31` callback
state transition, and the observed single-/multiplayer Escape callback routes.
The global handler's consumed events (`0x50` and `0x70`) are now calculated
directly. The `0x31` setter sets the startup byte and refreshes resources, but
does not set the `0x4EB20C` dirty-page flag; Escape action `1` sets the exit
latch. The key-`0x31` state change feeds the active-frame callback row in that
same iteration. Outputs from callback paths not modeled internally can be
supplied as page/state observations. Helper evidence is preserved in
`escape_menu_callback_helpers.evidence.json`.

Direct helper observations confirm `0x40A160` returns the event result at
`0x4D0C54`, `0x425850` returns active state `0x4D4DD8`, `0x4259B0` stores the
page at `0x508758`, `0x49BEA0` sets `0x4E3220`, `0x425800` clears `0x508760`,
`0x4035D0` sets `0x4EB300`, and `0x425710` sets `0x5087B4`. `0x4029C0` clears
`0x4EB20C` and selects `0x425660` or `0x49C2B0` based on `0x49BE40`. The
opaque timeout-menu result, page-helper mutations, second active-state read,
and graphics backend effects remain caller observations. Evidence is in
`race_loop_outer_iteration.evidence.json`.
