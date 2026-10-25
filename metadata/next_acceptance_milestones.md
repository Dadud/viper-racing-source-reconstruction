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
still not called by the production race session. The next unresolved wheel
inputs are per-frame `+0x158` from body `+0x50C`, initialized-history `+0xE8`,
and the random-seeded table/call lifecycle for environment polarity. Static
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
Native order inside NCAR callback `0x439200` is known: its timed sample/queue
prefix and accepted-sample handling run before its conditional `0x431150`
integrator branch. The sample therefore captures the pre-integrator state for
that callback. `0x420930` only retrieves the registered object; the live race
session still lacks the mutable body state the capture requires, and the
relationship between the accepted sample window and the separate `+0xEC4`
callback record remains unresolved. The runtime must reproduce this ordering
once it owns that state. See `metadata/car_update_branches.md` and the NCAR
callback evidence listed there.

### State required before the live physics integration can be wired

The current audit does not support treating route pose, terrain contacts, or
the constructor's `+0x50C = 1.0` as substitutes for native frame state. The
`0x4329C0` wheel-context copy reads mutable body `+0x50C`; observed callback
and physics-record paths can change it. Wheel `+0xE8` is interpolated from
two adjacent five-byte samples in the separate `0x3C`-stride object ring,
using a fraction derived from global tick and object timing fields. This ring
is distinct from the timed NCAR sample pipeline's three-slot `0x3E` history.
The `0x4278F0` GCAR constructor allocates `0x960` records at this stride, and
the `0x427BF0` path can copy supplied records into it. The constructor's
identity-helper loop does not initialize an empty buffer, and the evidence does
not yet establish that this replay/sample buffer supplies the live race ring.
Neither mutable body-integrator state nor live ring contents, timing lifecycle,
producer ordering, and callback gates are currently owned by the race session.
The timed sample pipeline explicitly consumes body state from its caller; it
cannot create that state from route motion.

Therefore the next implementation boundary is the state producer and its
ownership, not a call to `advance_four_wheel_body_step`: reconstruct the
mutable body state and callback write paths, locate the live player's
wheel-history producer independently of the `0x960`-record GCAR ring (now
identified as ghost playback), and establish the tick/fraction and callback
ordering. Only then can the session feed evidence-backed `+0x50C` and `+0xE8`
values into the existing wheel/body composition. In parallel, the polarity
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
