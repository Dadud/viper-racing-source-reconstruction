# Next acceptance milestones

This document prioritizes the next three integration milestones toward the
unchanged full objective: a faithful, fully playable modern Windows
reconstruction of Viper Racing. It is a sequencing aid, not a scope reduction,
acceptance substitute, or completion claim. The detailed feature-by-feature
requirements and final gates remain in [`completion_ledger.md`](completion_ledger.md).

## Current evidence snapshot

The Windows runtime owns parsed session assets and has a setup window, D3D11
scene/HUD, keyboard bindings, route-progress state, route-driven opponent
safe-follow policy, standings, and a return-to-setup path. The trace-driven frontend
lifecycle now reaches results on all eight tracks through the route prototype;
Viper/Bemidji, Sports/Bemidji, and Sports/Dundas additionally exercise selector
changes and saved-selection reload. `src/game_main.cpp`
still advances the player and opponents with `advance_route_motion`; its
four-wheel contact updates are observations and do not feed suspension forces,
body accumulators, collisions, or pose integration. Their terrain projection
origin now comes from the initialized PCAR body's byte-backed basis and
position, rather than the route pose. The body remains static in the live
session, and the projection remains a terrain sample rather than the native
collision-world query. This is explicitly
recorded as incomplete in the physics, AI, and race-mode rows of
[`completion_ledger.md`](completion_ledger.md). Existing portable physics
models are substantial but many still accept caller observations and are not
owned by the live session (`metadata/car_update_branches.md`,
`metadata/simulation_loop.md`, `metadata/vehicle_setup.md`).
Each scheduled player tick now retains an immutable input/timing context
including live-keyboard versus logical-replay provenance, recovered controls,
callback globals, scheduler clock, and the pre-increment native tick time.
This is a provenance boundary feeding the existing projections; it does not
make their route-driven movement or terrain/contact samples physics-owned.
The opt-in `--record-tick-trace PATH` writes every scheduled
player tick, including the full 256-byte binding-input snapshot and its
per-tick controls/timing plus post-callback byte-backed body orientation basis,
position, native linear/angular velocity (`+0x234/+0x240`), force/moment
accumulators, and wheel contact-active/projected-depth state. It also exposes
initialized body timer sources `+0x50C` and `+0x510`, plus byte-backed wheel
`+0x158` and prior contact-depth `+0xE8` on each tick; unknown bytes remain
blank. The
trace replay regression checks contiguous tick ordering, pre-increment timer
values, logical-input provenance, input snapshot size, finite body/contact
samples, the new history columns, and byte-identical repeated playback. The
body and wheel values currently expose constructor/contact-observation state;
this is instrumentation, not physics acceptance.
Next wire a complete observed body update and preserve paired per-tick physical
state captures against the original.
Four wheel contact projections now transform constructor wheel anchors through
the initialized byte-backed PCAR body basis and position and project along its
up axis. `trace_replay_regression.ps1` confirms the supported terrain query
samples remain valid through a deterministic full Viper/Bemidji race. This
does not advance body pose, run the native collision query, produce native
forces, or transfer pose authority from the route prototype.
The opponents now use a deterministic route-gap controller with staged
braking hysteresis rather than unconditional full throttle; the trace records
each decision and repeated playback checks it. This authored policy improves
the current route prototype only. Native ACAR decisions, physical opponent
state, and original comparison remain open
([`route_opponent_ai.md`](route_opponent_ai.md)).
The player now owns four byte-backed wheel objects containing resolved wheel
constructor defaults, all modeled `0x441C10` geometry/spring/steering outputs,
and persistent contact outputs. This closes object-state materialization only;
the live loop still does not consume those bytes as suspension inputs.
The PCAR body owner now receives the exact initial pose copy performed by
`0x433C00`: the twelve runtime-record floats at `+0x18..+0x44` go to body
`+0x38..+0x64`. A focused regression checks exact bit ordering, and the
separate Release validation build passed a Viper/Bemidji software smoke against
the external asset installation. This initializes body bytes from the observed
constructor handoff; the body integrator still does not advance them and the
route prototype still owns player motion/render pose.
The body-kinematics reader now also requires gravity mass at `+0x1F0` (DWORD
index `0x7C`), and the four-wheel composition takes its gravity input from that
same byte-backed owner. The regression verifies exact extraction and rejects
an uninitialized source byte. This closes input provenance for the native
gravity term only; wheel forces, callback effects, and live body integration
remain disconnected.
The Win32 loop now uses the recovered `0.016` physics-task schedule and
two-step catch-up cap to advance its current route/contact prototype, while
sampling controls once per outer update and retaining the latest accepted
simulation pose for rendering. The modeled PCAR control prefix and selector
branch run once per scheduled fixed step; trace rows expose the callback step
count and the replay regression checks it against the physics tick count. The
`+0x4F0/+0x4EC` shared-global getter stores also run before the late tail. The
projection runs at the PCAR's bound index in the validated ordered task stream
(index 7 in the captured Viper/Bemidji order), with a per-entry count checked
against callback and physics tick counts during replay. Tail
effects apply only when every consumed byte is initialized in the session body;
otherwise those fields remain unknown without aborting the race update.
The timer phase is resolved: the original task dispatcher runs before its
global tick increment, so `0x425790` observes the current pre-increment tick.
The runtime now applies the false-gate `+0xECC = 0` store even when the prior
DWORD is unknown, since the native false branch does not read it. The enabled
branch still requires an initialized timestamp. A controlled original capture
observed the false gate and zero value at the callback join; the enabled
backend path remains unavailable. The direct `0x43B880` global stores now run
when their three byte-backed PCAR source DWORDs are initialized; the
`0x434A80` unconditional `+0xE28` cache-byte refresh is connected, while its
linked-target update remains unavailable. The native task factory,
interpolation ring, post-integrator callback work, and shared body-step
integrator are not connected. Route kinematics still own player and opponent
motion.
Three paired Frida snapshots at the original PCAR integrator entry/return prove
that `0x431150` changes position and linear/angular velocity for a grounded
near-stationary object (`metadata/car_update_branches.md`). The PCAR constructor
receives the serialized record through the `0x463510` → `0x425720` → `0x422580`
loader/factory path. After base constructor defaults, the PCAR constructor
helper `0x433C00` copies all twelve runtime transform floats from record
`+0x18` into body basis and position `+0x38..+0x64`; the session now applies
that exact mapping to its byte-backed body. A runtime-resolved manager slot
points to `0x43CA90`; its registrar stores the body pointer by ordinal and
initializes sidecar state but does not copy placement. The second registration dispatch was traced to a
`LiveMultiInfo`/`Multiplayer` service context and is deferred to the multiplayer
milestone; it does not explain single-player pose initialization. The later
per-step body integration and renderer consumption of body-owned pose remain
unresolved. This is evidence that native body state evolves and initial
placement now reaches session body bytes, not a runtime integration or an
input-driven baseline.
The session now also retains one ordered semantic task collection from the
validated serialized stream: finite CHKP geometry and supported inactive STAT
state are materialized, while other records remain explicit as unavailable
active callbacks. The known partial PCAR projection is dispatched at its bound
entry; native task allocation/registration and all other task callbacks remain
unimplemented.

The bounded video-memory override reached the original main menu, Quick Race
setup, and a rendered race-start scene (`metadata/graphics_startup.md`). No
input-driven vehicle movement, completed lap, or paired gameplay baseline has
been captured (`metadata/reference_capture_protocol.md`). Therefore neither
route traces nor smoke tests establish original-matched playability.

## Prioritized milestones

### 1. Make one live player car physics-owned

**Outcome:** A persistent player simulation object, initialized from the
selected track, CARF/setup, FNIM, wheel anchors, axle tires, and terrain, owns
position, orientation, linear/angular velocity, wheel state, and accumulated
forces for the duration of the race. A fixed-step session update consumes
actual keyboard binding samples, performs four wheel/contact/suspension/tire
updates, transfers the resulting forces/impulses into the body integrator,
updates body pose, and snapshots state for rendering. Remove route progression
as the authority for player pose; route data may remain for grid placement or
track queries. Do not fill unmodeled native inputs with guessed constants
without labeling and isolating them as unresolved evidence.

**Acceptance evidence:** A live Viper/Bemidji window with input playback moves
the physics-owned car across terrain; frame traces show controls entering the
same session-owned state, wheel forces reaching body integration, finite
changing position/orientation/velocity, and the renderer consuming that pose.
Include a reproducible throttle, steering, braking, coast, and terrain-contact
scenario. Add focused regressions for any new deterministic state transition.
No race-completion claim is made by this milestone.

**Immediate work:** Identify the narrowest missing state/argument seams between
`initialize_ncar_four_wheel_core`, `initialize_tire_response_object`, the
existing wheel/body update compositions, and `WindowState`. Introduce a
session-owned physics type and fixed-step update at the Win32 boundary; replace
the player's call to `advance_route_motion` only when that update produces the
rendered pose. Record every still-observed external callback/input in a
machine-readable trace. The live session now composes modeled `0x43B290` from
keyboard samples and configured `CONTROL` sensitivities/ranges/rates; its
outputs at `0x509358`, `0x5093F4`, and `0x509320` drive the current route
adapter. The PCAR callback reads the throttle/brake accessors, places
transformed values on the car through setter helpers, and reaches the
`0x431150` integrator, whose four `0x442530` wheel updates accumulate contact
forces through `0x43E6F0`. This establishes the control-to-wheel-update call
chain, but the live PCAR setter state, complete wheel/contact inputs, force
collector, and body-integrator state are still not owned by `WindowState`.
Build those persistent inputs and connect them to the existing wheel/body
compositions before replacing route motion as the rendered pose authority.
All currently modeled `0x441C10` outputs are now materialized at their wheel
offsets. The composed `advance_four_wheel_body_step` now derives the surface
motion projection from the prior contact point and step-start body kinematics,
before the current contact query can replace that point. This composition is
still not called by the production race session. The live `+0xE8` producer is
now identified: `0x442530` derives it from the wheel's native contact depth and
stores that depth after the suspension calculation. The session has the
constructor's initial `+0xE8` value, but its route-pose contact observation is
not the native fixed-step body/query state, so this store is not yet wired.
Remaining unresolved wheel inputs include per-step `+0x158` from mutable body
`+0x50C` and the random-seeded table/call lifecycle for environment polarity. Static
analysis now establishes common-startup initialization and the shared locked
stream: each car update consumes four sequential wheel draws, and at least 16
other resolved direct callers also advance it. The repo now models the seed
wrapper's low-EAX signed remainder modulo 30,000 and native push order, and
the recovered 97-entry table recurrence and associated initializer constants.
The table port has no native fixed-seed parity fixture; live timer sampling
and the shared stream are not implemented or connected. See
[`surface_polarity_initialization.md`](surface_polarity_initialization.md).
Do not promote the initial
constructor value of `+0x50C` to a per-frame value until the callback/sample
state that mutates it is connected.

The production per-tick body diagnostic now separately reports ownership of
the three body fields read by `0x433570` (`+0x50C`, `+0xE44`, and `+0x510`),
including missing offsets and finiteness of the two timer floats. This probe is
exercised by the existing `capture_player_tick_physics_state` call, but it
does not run or mutate the timer helper. Its ordinal-pair table at `0x4D45B0`
and the shared-integrator call ordering remain unowned, so the `+0x50C` ramp
must stay outside production suspension inputs until that source chain is
connected. Static analysis now shows `0x420750` rebuilds/sorts the registered
participant-pair table after its entity callbacks. Each `0x433570` call
therefore reads the prior completed update's table, and the live session must
preserve that one-update snapshot boundary. It still lacks current body-owned
position/velocity rows for all registered participants. Regression verifies
the timer gate independently from rigid-body kinematics: unavailable timer
fields do not invalidate an otherwise complete `0x43ECE0` snapshot.

A fresh hash-verified original-client race capture now samples all four wheels
for eight consecutive selected-PCAR integrator calls. It records body
`+0x50C/+0xE44/+0x510` alongside each wheel's `+0x158/+0xE8` before and after
the update. The values remain distinct and `+0xE8` responds during some wheel
updates. Static analysis identifies the `0x433570` timer helper, and a live
read-only follow-up observed 16 calls without changes to `+0x50C/+0xE44/+0x510`.
The wheel `+0x158` producer remains unknown, and the unchanged timer sample does
not establish why the helper's update branch is inactive. This narrows the
instrumentation gap without changing the production gate above. Repair and
capture provenance are in
[`original_physics_dispatch_capture.md`](original_physics_dispatch_capture.md).
A bounded checked-in Frida probe now waits through the 6.2-second timer gate,
retains a capped sample around that gate, and records the participant-pair
lookup if reached. A recent suspended-child retry created the engine child,
but its startup-memory override produced no query event and the child exited
before recording a timer row. The successful original-client launch path is
already established with the documented first-query video-memory override;
use that path for the next timer capture instead of treating the retry as a
startup blocker. Wheel `+0x158` write provenance remains unresolved.

The model `advance_four_wheel_body_step` currently has no production call site.
The local player is dispatched as PCAR: per-step task dispatch `0x421050`
checks the physics object's virtual `+0x2C` gate and calls virtual `+8`, which
resolves through PCAR vtable `0x4C8188` to callback `0x4384D0`. Once that
virtual dispatch gate passes, all observed PCAR callback branches join before
its call to shared wheel/body integrator `0x431150`; NCAR callback
`0x439200` is a separate sample/queue/interpolation path and must not be used
as the player callback. The live session now persists the numeric ACAR-derived
curve and selector configuration and runs the modeled PCAR embedded-selection
branch after the existing scalar prefix. The selector's `0x43F910` setup value
at child `+8` (parent `+0xCA0`) is distinct from the `0x43FDD0` selection
denominator at child `+0x18` (parent `+0xCB0`); the adapter reads the latter.
The callback's velocity/basis projection and `0x4330F0` smoother now execute
from initialized PCAR body velocity/basis and constructor-backed `+0x500`,
`+0xDCC`, and `+0xBE4` fields. Their selected output pair and the opening scalar
stores are persisted into the same body object. The shared integrator remains
unwired, so these values still do not advance body pose. The production session
owns known base-constructor bytes for one PCAR body, including the exact
`0x433AA0` `+0xE44 = -100.0` sentinel. It does not yet run the `0x433570`
ordinal-record timer update or advance body state through `0x431150`. The
3-slot NCAR `0x3E` sample
history is distinct from the `0x960`-record GCAR `0x3C` wheel-history ring;
the latter is tied to GhostBegin playback and is not a live-player ring.
See `metadata/physics_task_objects.md`, `metadata/car_update_branches.md`, and
the next-state dependencies below.

A controlled original capture hooks the selected PCAR at `0x43ECE0`, after the
four `0x442530` wheel updates. It records live `+0x24C/+0x258` aggregate
force/moment inputs before the body update and confirms those accumulators are
zero after the call; the same capture observes their values changing during a
synthetic throttle ramp. A subsequent wheel-scoped capture tracked each
`0x442530` entry/return around `0x43E6F0`. It identifies two nonzero writer
callsites per wheel (`0x4430E9` through wrapper `0x431130`, and direct
`0x443C09`), records all four wheel slots and the body center, and separates
later integrator-level writers at `0x431C2D`, `0x431D12`, and `0x431DBC`.
Eight captured wheel additions now pass the portable `0x43E6F0` accumulator
model's new regression fixture. A second live capture recorded 128 writer
events and wheel-object state across eight PCAR updates. All 32 sampled direct
`0x443C09` vectors match wheel `+0x100..+0x108` bit-for-bit; all 32 suspension
wrapper rows expose the clamped response magnitude, and the first four
captured X components match magnitude times the native caller's X direction
local. Static analysis also shows that the native direct tire vector is the
captured wheel `+0xF4` response projected off basis `+0x110`; the four captured
first-update object snapshots reproduce `+0x100` within `7.3e-6`, and the
asset-free regression now replays that transform before its four direct tire
accumulator calls. The regression also checks the four captured suspension X
compositions. The checked-in response probe has exact stage hooks for the
suspension and tire callsites, but those additional stack rows have not yet
been captured. The regression now replays all 32 ordered direct tire writes
from eight captured integrator updates, checking the known projection and
accumulator stages for each wheel. This closes another wheel-to-accumulator
stage; the pre-projection response inputs, suspension direction producers,
native collision/query state, and live session ownership remain unresolved. See
[`original_physics_dispatch_capture.md`](original_physics_dispatch_capture.md#corrected-wheel-scoped-force-callsites).

### State required before the live physics integration can be wired

The current audit does not support treating route pose, terrain contacts, or
the PCAR constructor's `+0x50C = 1.0` as substitutes for native frame state.
The `0x4329C0` wheel-context copy reads mutable body `+0x50C`. Its constructor
value is only initial state; `0x433570` can update it on the shared
`0x431150` integrator path, while other callback writers have their own gates.
The production session stores the shared body constructor's position, basis,
dimensions, inertia matrices, velocity/accumulator clears, derived PCAR
constructor vtable/`+0x50C`, and settings-derived shift/clutch/yaw plus wheel
traction/ABS fields in persistent byte-backed body/wheel objects with
initialized-byte masks. The session now also applies the constructor's known
embedded curve/selector defaults at `+0xBFC` and `+0xC98` to that same byte
owner. Runtime selector table entries and ACAR-derived curve/profile inputs
are now projected from the mapped runtime car record; the numeric embedded
selection branch is connected after the scalar prefix. Its denominator and
constructor setup fields are kept separate by native child-relative offsets.
On each race update, the callback opening now reads initialized body velocity
and basis, runs the velocity projection, global scale, and `0x4330F0` smoother,
and writes its accumulator, selected output pair, and
`+0xDE0/+0xDE4/+0xDE8/+0xDEC` scalar stores into that same body owner. The
constructor-backed smoother multiplier comes from the mapped runtime car
record; `0x40D270` now uses `GAME throttle_boost` and the human entrant-pointer
selector predicate. Gate `0x5093FC` now follows `CONTROL nonlinear_steering` and the recovered
`0x43BA30`/`0x446F90` binding-kind clearing rule. This
does not execute the shared integrator or make these outputs authoritative for
rendered pose. The gated late tail stores at `+0xC3C`, `+0xDE0`, `+0xDE4`, and
`+0x508` now use the existing offset model per fixed step. The immediately
preceding cache getters store `+0x509344` as a float at `+0x4F0` and
`+0x509420` as a byte at `+0x4EC`. The value passed to the `+0x508` helper is
still the current `+0x509340` projection because the earlier timestamp,
`0x434A80` target mutation, and other callback effects remain incomplete;
`0x43B880` is
applied conditionally from initialized object fields. The runtime does not yet
reproduce complete PCAR callback `0x4384D0` or shared integrator `0x431150`; it
lacks the runtime timer and earlier shared-cache callback producers, native
wheel/force collection, and
post-integrator record/event transitions. The PCAR `+0x4F4` constructor input is now sourced from
the configured entrant count: native setup copies the setup-block DWORD at
`+0xCA8` into global `0x508794`, and the race owner consumes that same field as
the participant-count loop bound. Constructor byte `+0xD74` and four wheel
bytes `+0x6B4` now come from the two recovered `0x43BA90` calls. The helper
uses the `CONTROL game_pad` gate and configured throttle binding at global
`0x509328`; the first result is persisted in the body and tick trace, while
the second is applied to each wheel object. Wheel `+0xE8`
is interpolated from adjacent five-byte samples by `0x444240`, using records
selected through `0x427630` and a fraction derived from global tick and object
timing fields. This particular `0x3C`-stride ring is owned by the
`0x4278F0`/`0x427A40` GCAR GhostBegin playback path; it is not evidence for a
live-player wheel-history producer. It is distinct from NCAR's timed three-slot
`0x3E` sample history.

Runtime startup builds a complete tag/cursor manifest for native world-load
order: recognized `track.obt` object records first, then configured entrant
rows not consumed by `obj car` directives. Each entrant's PCAR/ACAR tag comes
from its observed setup marker. The live race path now materializes the
track.obt CHKP/WOBL/STAT/OBST records, joins them with setup/placement-derived
PCAR/ACAR bytes by manifest identity, and serializes the complete ordered
framed stream. A one-frame WARP run confirms all entries and the final cursor
for all eight shipped tracks with Viper, and all five shipped cars on Bemidji.
The live stream remains separate from the constructor-only player stream.

This is a byte-stream milestone, not native object construction. The stream
readback is correlated with its factory inventory and a session-owned semantic
collection; the collection retains inert CHKP, supported inactive STAT, and
the cached pose for valid OBST records, with selector-2 geometry retained as
well. This still does not allocate native objects or dispatch active callbacks. Current
startup passes selector zero based on the shipped zero-initialized global and
ordinary startup path; it assumes no indirect runtime mutation before track
parsing. Static parsing is enabled based on `0x40D2F0` returning false after
ordinary startup clears its second gate byte. On x64, terrain division uses
host arithmetic; original x87/FDIV compatibility is not implemented. Obstacle
model lookup currently uses an explicit active-track, active-car, Viper, then
shared-bundle precedence which still needs comparison against the native
resource-search order. The stream is not yet fed through the observed native
constructors, and no allocated physics-task objects are retained for fixed-step
dispatch. Unsupported directives/classes, visual-only rows, invalid resources,
or incomplete lines still block stream acceptance rather than being skipped.

The next boundary is to compare the now-complete serialized bytes and object
creation sequence against a controlled original run, establish exact runtime
gate/global values and obstacle model lookup order, and feed the stream through
the observed task factory while retaining its objects for fixed-step dispatch.
Then reconstruct mutable
PCAR callback/force state and establish tick/fraction and callback ordering.
The transfer from body `+0x50C` to wheel `+0x158` is already identified at
`0x4329C0`; the remaining timer dependency is the live `+0x50C` value produced
by the gated `0x433570` branch. The GCAR GhostBegin ring is separate and must
not be used as a substitute for that PCAR value. Only when the full per-frame
inputs are sourced can the session connect the shared integrator and feed
evidence-backed `+0x50C` and `+0xE8` values into the wheel/body composition. In
parallel, the polarity initializer still needs the actual common-startup timer
backend and a single shared stream whose call order includes non-wheel
consumers. Keep these as
explicit dependencies; do not claim the existing models are live physics.

### 2. Close the physical race loop: terrain, collision, damage, and finish

**Outcome:** The player-owned simulation from milestone 1 runs stably for a
full Viper/Bemidji race. Terrain contact updates suspension and tire response;
chassis motion and collision response affect the same body state; race time,
lap/checkpoint progression, damage, finish freeze, and results derive from
that physical state and the original-supported rules. Rendering and HUD report
the integrated state rather than route-model speed or route crossings.

**Acceptance evidence:** Replay a timestamped schedule through keyboard
input, including acceleration, turn, braking, a controlled collision, and
finish. Preserve per-fixed-step controls, body/wheel state, contacts, collision
events, lap/checkpoint events, and results. Repeat from the same start and
compare deterministic state traces. Drive manually through start, finish,
results, return to setup, and a second race without stale state. The existing
trace-driven route-only scenario is not sufficient evidence.

**Immediate work:** Carry physical state through one complete update frame
without observation-only substitution; then harden stability and scenario
replay before extending mode behavior. Keep all outstanding native behaviors
visible in the ledger.

### 3. Complete and compare the first original-compatible single-player race

**Outcome:** One supported Viper/Bemidji single-player race has a usable
frontend/setup flow, physics-driven controls, configured opponent construction
and behavior, presentation/audio, mode-correct results, and a reliable return
to the frontend. Opponents must have persistent simulation state and actual
AI decisions; fixed-throttle route followers do not qualify. Establish the
first controlled original-versus-reconstruction baseline as soon as the
original can be launched in a usable graphics configuration.

**Acceptance evidence:** A paired capture following
[`reference_capture_protocol.md`](reference_capture_protocol.md) records the
same track, car, mode, opponent settings, input schedule, checkpoints,
frames, audio, and results for both programs. Report measured deltas and
repeatability; derive tolerances from repeated original runs. Independently
complete the same scenario in the reconstruction from frontend through
results and back, using both recorded input and manual keyboard play. If the
original remains unlaunchable on the current host, retain that as an explicit
blocker and pursue a compatible graphics/runtime configuration; do not treat
the failed startup capture as a gameplay comparison.

**Immediate work:** Resolve the original launch environment in parallel with
milestones 1 and 2 where practical. After player physics is stable, replace
opponent route followers with per-entrant physics/AI session objects and
integrate results rules, audio triggers, and scene presentation against the
captured baseline.

## Full-scope work that follows

Passing these three milestones would establish only the first validated race,
not the final objective. A lossless v6 replay container codec is now implemented
and byte-round-tripped against the shipped `bench.rpl`; compatible playback,
packet interpretation, and native replay UI remain open
([replay container notes](replay_container_codec.md)). Continue through all
original-supported tracks, vehicles, modes, frontend/settings,
career/upgrades/saves, compatible replay, modern multiplayer between
reconstructed clients, error/resource lifecycle, clean-checkout asset
discovery, and a requirement-by-requirement completion audit. Keep original
assets and private captures outside the public repo.
