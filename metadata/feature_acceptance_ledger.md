# Feature, evidence, and acceptance ledger

Updated: 2026-10-29. This is a compact acceptance index for the stated goal:
a complete, playable modern Windows reconstruction of Viper Racing using
separately supplied original assets. Original-client interoperability is out
of scope. It summarizes, and does not replace, the detailed evidence and
limitations in [`completion_ledger.md`](completion_ledger.md), nor does it
change the sequencing in [`next_acceptance_milestones.md`](next_acceptance_milestones.md).

## Status rule

**Partial** means implementation or analysis exists but the listed acceptance
condition is not met. **Unverified** means there is no integrated implementation
with runtime evidence. A parser, static model, smoke test, route trace, or
prototype lifecycle by itself does not establish playable or original-matched
behavior. The current runtime is a route-kinematic prototype; the route adapter
still owns player and opponent pose. No required system below is accepted as
complete.

**Test oracle rule:** ideal-line-derived pose, route distance/speed, lap
completion, standings, and results are not driving evidence. Runtime tests must
keep route motion disabled. Until the session owns a physics-integrated pose,
tests may verify parsing, input delivery, and deterministic diagnostics only.
Once that pose exists, driving tests must replay a timestamped recorded input
sequence and compare body state, contacts/forces, and surface observations at
matched tick ordinals against an original-client capture. Existing synthetic
DirectInput captures are field-selection and probe-validation references; their
uncontrolled initial motion and instrumented input prevent them serving as a
numeric golden trace.

## Required systems

| Scope | Authoritative evidence | Acceptance condition | Current status |
|---|---|---|---|
| Build, startup, and application lifecycle | [`completion_ledger.md`](completion_ledger.md) (build/application row); [`engine_application_entry.md`](engine_application_entry.md); [`graphics_startup.md`](graphics_startup.md); [`resource_loading.md`](resource_loading.md); Windows CI workflow and lifecycle smoke described in README | Clean Windows checkout builds and runs with separately supplied assets; setup reaches a race, results return to setup, repeated runs and shutdown are clean, and missing/corrupt assets produce actionable errors. Validate resource/device failure paths. | Partial. Build and route-prototype lifecycle evidence exists; public clean-checkout and complete failure-path acceptance are outstanding. |
| Asset loading and supported inventory | [`completion_ledger.md`](completion_ledger.md) (track and vehicle inventory rows); [`world_track_objects.md`](world_track_objects.md); [`model_resource.md`](model_resource.md); [`vehicle_setup.md`](vehicle_setup.md); README | Determine original-client selectable track and vehicle rosters from resource/frontend evidence. Every supported selection loads and is playable in the accepted race; classify resource-only/hidden entries explicitly. | Partial. Eight track bundles and five car bundles load in local checks; this does not establish the intended selectable roster or drivability. |
| Frontend, settings, and input | [`completion_ledger.md`](completion_ledger.md) (frontend/settings and input rows); [`frontend_dispatch.md`](frontend_dispatch.md); [`race_input_bindings.md`](race_input_bindings.md); [`runtime_frontend.md`](runtime_frontend.md) | Navigate supported menus using keyboard and supported controller inputs; configure and persist supported settings and bindings; reach setup and start the selected mode. Demonstrate held-key delivery and compare behavior where input parity is claimed. | Partial. Basic setup, keyboard control/remapping, persistence and route-session transitions exist. Native menu/mode parity, controller support and foreground held-key evidence remain open. |
| Physical vehicle simulation and collision | [`completion_ledger.md`](completion_ledger.md) (physics row); [`next_acceptance_milestones.md`](next_acceptance_milestones.md), milestone 1; [`original_physics_dispatch_capture.md`](original_physics_dispatch_capture.md); [`simulation_loop.md`](simulation_loop.md); [`vehicle_setup.md`](vehicle_setup.md) | A fixed-step, session-owned player state must consume controls, wheel/contact/suspension/tire inputs, forces and body integration; renderer must use the resulting pose. Reproduce throttle, steering, braking, coast and terrain-contact traces, then complete controlled original comparisons. Integrate collision response and retain coherent state through finish. | Partial; first-race gate blocker. Detailed byte-backed models, constructor/contact observations, a bounded synthetic-input original capture, and a read-only 8-integrator/32-wheel follow-up with force-accumulator and stage rows exist. Static analysis traces `0x442530` tire-response inputs through body point velocity, curve lookup and wheel/contact history, and confirms `0x45F760` selects from the `track.bpp` TPPB v2 plane/tree data already parsed by the runtime (Evidence `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`, `ev_7031c5908694232f0bcb6e0042e737b8f91c507c42431b064e587de9795c5e95`, `ev_706de19e0e8166a8a361cfa4ec84a6ce1489f0fbb8f46cb37162bfe8e3d2bdbf`, `ev_5d1495f055f329bfb284cd32ca65e5e6c17f79802eb3dd7fba4ed92a49193409`, `ev_3b54b33fed139732b6d29cd5b02c95e11ebe5e2b455a9b2de89bc3e48b4e4ae5`). The live session now runs the sourced `0x431150/+0x4C -> 0x432170` wheel `+0xE0` callback after the PCAR prefix, persists wheel `+0xC8..+0xD0` application points from byte-backed wheel/body inputs, and runs the `+0x158` differential producer after contact-driven suspension/history updates. Tire-force collection, the remaining shared-integrator stages, body accumulators, collision response, and pose integration remain open. Controlled behavior is unaccepted; route motion remains pose authority. |
| Opponents and race AI | [`completion_ledger.md`](completion_ledger.md) (AI row); [`route_opponent_ai.md`](route_opponent_ai.md); [`vehicle_pair_registry.md`](vehicle_pair_registry.md); [`single_race_ai_runtime_selectors.evidence.json`](single_race_ai_runtime_selectors.evidence.json) | Construct and update configured opponents through evidence-backed runtime state. Compare entrant count/order, timing and sampled decisions/behavior in the same controlled race; cover collision and passing behavior before claiming native-like AI. | Partial. AI setup records and a deterministic route-gap policy exist; ACAR constructors/callbacks and physical/native AI behavior are not integrated. |
| Race modes, progression, and results | [`completion_ledger.md`](completion_ledger.md) (race modes/results row); [`next_acceptance_milestones.md`](next_acceptance_milestones.md), milestone 4; [`race_progression` implementation and regressions referenced by completion ledger] | Inventory modes supported by the original. Each advertised mode must configure, start, complete, show correct results, and return to frontend without crash; document unsupported modes and compare the first accepted race's lap/results behavior. | Partial. Route-prototype lap counting, standings, and return-to-setup work in trace-driven runs; mode rules, physical completion and original-matched results are unverified. |
| Rendering and presentation | [`completion_ledger.md`](completion_ledger.md) (rendering row); [`graphics_startup.md`](graphics_startup.md); [`model_resource.md`](model_resource.md); README | Render the accepted race's track, cars, HUD, menus and results at supported display settings. Compare controlled captures; ensure displayed pose follows accepted simulation state and validate orientation, lighting/textures and camera behavior needed for parity. | Partial. D3D11 top-down terrain/mesh/HUD and WARP smoke coverage exist; route state supplies car pose, heading is approximate, and visual parity/lighting/textures remain open. |
| Audio | [`completion_ledger.md`](completion_ledger.md) (audio row); [`sound_resource.md`](sound_resource.md); [`vehicle_setup.md`](vehicle_setup.md) | Exercise representative menu and race audio with correct synchronized triggers; record output and compare against original. Cover engine, environment, collision, menu/music paths and buffer recovery. | Partial. Car PCM/ESFX parsing and provisional DirectSound engine playback/buffer setup exist. Band/gain/pitch mapping, synchronized output comparison and other sound categories remain open. |
| Career and save data | [`completion_ledger.md`](completion_ledger.md) (career/save row); [`career_season_samples.md`](career_season_samples.md) | Create/load a career, complete an event, verify results and progression, save, quit, relaunch, and verify the same visible state. Validate upgrades and persistence against original behavior. | Partial. Slot browsing, validation, creation and deletion are present; entering events and integrating race results/progression are not. |
| Replay and trace | [`completion_ledger.md`](completion_ledger.md) (replay/trace row); [`replay_container_codec.md`](replay_container_codec.md); [`reference_capture_protocol.md`](reference_capture_protocol.md) | Record a race and replay it with reproducible event ordering and visible motion; validate semantic state, save/load, results and original comparison. Keep developer route traces distinct from native replay acceptance. | Partial. Deterministic route trace capture/playback and lossless native v6 container inspection exist; native event/payload semantics and actual replay playback/recording remain absent. |
| Modern multiplayer | [`completion_ledger.md`](completion_ledger.md) (modern multiplayer row); [`network_session.md`](network_session.md) | Two reconstructed clients sustain a race with agreed setup and visible matching progression/results, then recover from a disconnect on a documented Windows/network setup. | Partial. Host/join and route-state replication smoke coverage exist; the bounded network session CTest now disables route motion and does not claim playable-driving coverage. Sustained interactive behavior, recovery, interpolation and authoritative identity/setup agreement are unverified. Original protocol compatibility is out of scope. |
| Original comparison and first accepted race | [`next_acceptance_milestones.md`](next_acceptance_milestones.md), milestones 1–4; [`reference_capture_protocol.md`](reference_capture_protocol.md); [`original_physics_dispatch_capture.md`](original_physics_dispatch_capture.md); [`reconstruction_progress.md`](reconstruction_progress.md) | Complete a controlled Viper/Bemidji single-player race in both clients with matched mode, car, track, laps, controls and host setup. Compare setup, transforms, timed inputs/body state, lap crossings, standings, audio triggers and return-to-setup; repeat until reproducible. | Not accepted. Original startup/grid and bounded synthetic input/body response evidence exists, but there is no controlled completed original race or paired gameplay baseline. |
| Clean checkout and external-assets boundary | [`completion_ledger.md`](completion_ledger.md) (clean-checkout row); README; Windows CI workflow and asset-discovery tests described there | From a clean public checkout, build without private assets, then discover/use separately supplied assets with clear diagnostics. Verify unsupported/corrupt data handling and document the reproducible workflow. | Partial. CI/build and bounded asset discovery behavior are documented; a full clean-checkout plus installed-data acceptance run remains open. |

## Acceptance order

The active first gate is **one physics-owned Viper/Bemidji player race**. Follow
[`next_acceptance_milestones.md`](next_acceptance_milestones.md): resolve the
wheel update's upstream inputs and persistent state; connect the complete
player step and render from its body pose; integrate native-like opponents;
then run and compare the same complete race in both clients. Broaden to other
tracks, vehicles and modes only after that gate passes. Career, native replay,
audio parity and modern multiplayer remain separate acceptance tracks.

The native contact query is now statically traced through a coordinate
quantizer, binary plane selector, plane equation, and signed-short surface code.
The matching `track.bpp` TPPB v2 loader/parser and PCAR-based runtime query
already exist. The native wheel callback now has a trace-only model of its
surface polarity draw and `0 -> -1`, nonzero `-> +1` adjustment. The portable
four-wheel tire-factor composition now propagates its terrain-feature-derived
mode scale to both the low-speed cutoff and forward/lateral tire-curve
multipliers; regression covers a feature-code result of `0.9`. This does not
connect tire forces to the Windows runtime. The remaining data gap is
fixed-seed/shared-stream parity and sourcing every frame-dependent wheel input
into the runtime session. The immediate behavioral gap is a controlled
per-wheel/body capture
including `+0x24C/+0x258` accumulator deltas under coast, throttle, steering,
and braking. The bounded original capture documents synthetic key reads, not
normal keyboard delivery or a force law.
Keep unresolved fields fail-closed; do not promote constructor state, contact
observations, route traces, WARP smoke or startup captures into claims of
accepted gameplay.
