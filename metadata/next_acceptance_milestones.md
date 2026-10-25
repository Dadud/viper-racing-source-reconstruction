# Next acceptance milestones

This document prioritizes the next three integration milestones toward the
unchanged full objective: a faithful, fully playable modern Windows
reconstruction of Viper Racing. It is a sequencing aid, not a scope reduction,
acceptance substitute, or completion claim. The detailed feature-by-feature
requirements and final gates remain in [`completion_ledger.md`](completion_ledger.md).

## Current evidence snapshot

The Windows runtime owns parsed session assets and has a setup window, D3D11
scene/HUD, keyboard bindings, route-progress state, route-driven opponent
placeholders, standings, and a return-to-setup path. Trace-driven sessions
exercise a Viper/Bemidji lap and results lifecycle. `src/game_main.cpp`
still advances the player and opponents with `advance_route_motion`; its
four-wheel contact updates are observations and do not feed suspension forces,
body accumulators, collisions, or pose integration. This is explicitly
recorded as incomplete in the physics, AI, and race-mode rows of
[`completion_ledger.md`](completion_ledger.md). Existing portable physics
models are substantial but many still accept caller observations and are not
owned by the live session (`metadata/car_update_branches.md`,
`metadata/simulation_loop.md`, `metadata/vehicle_setup.md`).
The player now owns four byte-backed wheel objects containing resolved wheel
constructor defaults, all modeled `0x441C10` geometry/spring/steering outputs,
and persistent contact outputs. This closes object-state materialization only;
the live loop still does not consume those bytes as suspension inputs.
The Win32 loop now uses the recovered `0.016` physics-task schedule and
two-step catch-up cap to advance its current route/contact prototype, while
sampling controls once per frame and retaining the latest accepted simulation
pose for rendering. Trace rows include scheduled steps, task tick count, and
derived task time. This improves timing ownership only: the native task factory,
interpolation ring, PCAR callback, and body-step integrator are not connected,
and route kinematics still own player and opponent motion.

The original-client attempt currently stops during graphics initialization;
there is no paired gameplay baseline (`metadata/graphics_startup.md`,
`metadata/reference_capture_protocol.md`). Therefore neither route traces nor
smoke tests establish original-matched playability.

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
owns known base-constructor bytes for one PCAR body, but does not yet advance
that state through `0x431150`. The 3-slot NCAR `0x3E` sample
history is distinct from the `0x960`-record GCAR `0x3C` wheel-history ring;
the latter is tied to GhostBegin playback and is not a live-player ring.
See `metadata/physics_task_objects.md`, `metadata/car_update_branches.md`, and
the next-state dependencies below.

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
rendered pose. The runtime does
not yet dispatch complete native PCAR callback `0x4384D0` or shared integrator
`0x431150`; it lacks their runtime body, timer, table, force-collection, and
callback transitions. The PCAR `+0x4F4` constructor input is now sourced from
the configured entrant count: native setup copies the setup-block DWORD at
`+0xCA8` into global `0x508794`, and the race owner consumes that same field as
the participant-count loop bound. `+0xD74` remains unresolved; it stores the
result of a helper that samples gated control state at global `0x509328`, and
that state is not yet mapped to a configured binding. Wheel `+0xE8`
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

This is a byte-stream milestone, not native object construction. Current
startup passes selector zero based on the shipped zero-initialized global and
ordinary startup path; it assumes no indirect runtime mutation before track
parsing. Static parsing is enabled based on `0x40D2F0` returning false after
ordinary startup clears its second gate byte. On x64, terrain division uses
host arithmetic; original x87/FDIV compatibility is not implemented. Obstacle
model lookup currently uses an explicit active-track, active-car, Viper, then
shared-bundle precedence which still needs comparison against the native
resource-search order. The stream is not yet fed through the observed task
factory, and no resulting physics-task objects are retained for fixed-step
dispatch. Unsupported directives/classes, visual-only rows, invalid resources,
or incomplete lines still block stream acceptance rather than being skipped.

The next boundary is to compare the now-complete serialized bytes and object
creation sequence against a controlled original run, establish exact runtime
gate/global values and obstacle model lookup order, and feed the stream through
the observed task factory while retaining its objects for fixed-step dispatch.
Then reconstruct mutable
PCAR callback/force state, locate the live player's wheel-history producer
independently of the GCAR GhostBegin ring, and establish tick/fraction and
callback ordering. Only when the full per-frame inputs are sourced can the
session connect the shared integrator and feed evidence-backed `+0x50C` and
`+0xE8` values into the wheel/body composition. In parallel, the polarity
initializer still needs the actual common-startup timer backend and a single
shared stream whose call order includes non-wheel consumers. Keep these as
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
