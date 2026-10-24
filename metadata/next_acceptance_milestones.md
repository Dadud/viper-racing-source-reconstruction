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

The model `advance_four_wheel_body_step` currently has no production call site.
Its caller needs the registered object's body state at capture time for the
NCAR sample pipeline. `0x420930` is only the registered-object lookup, not a state
update; native ordering between integration and sampling must be established
before wiring that pipeline into the race session. See
`metadata/car_update_branches.md` for the corrected evidence reference.

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
not the final objective. Continue through all original-supported tracks,
vehicles, modes, frontend/settings, career/upgrades/saves, compatible replay,
modern multiplayer between reconstructed clients, error/resource lifecycle,
clean-checkout asset discovery, and a requirement-by-requirement completion
audit. Keep original assets and private captures outside the public repo.
