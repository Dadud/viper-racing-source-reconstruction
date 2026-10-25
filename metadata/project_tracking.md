# Reconstruction status and completion plan

Updated: 2026-10-25. This snapshot is based on the source tree and the
feature-by-feature evidence in [`completion_ledger.md`](completion_ledger.md).
The old numeric scores were based on a substantially earlier prototype and
are not carried forward as current percentages. The workstreams differ too
much in maturity for one precise completion number to be useful. The prior
dated score calculations and development notes are preserved in
[`project_tracking_history_2026-10-24.md`](project_tracking_history_2026-10-24.md).

## Current status

The repository contains an analyst-written C++ reconstruction and a Windows
runtime, not recovered original source code. `viper_game` loads separately
supplied assets, presents a basic setup screen, starts a D3D11 race scene, maps
keyboard controls, records/replays deterministic route traces, displays
prototype results, and returns to setup. Its player and AI still move using
route-based kinematics; this is a runnable race prototype, not a complete or
original-matched game.

| Area | Current evidence | Main gap before acceptance |
|---|---|---|
| Source and build | Windows x64 CMake build and regression suite are configured in CI; local asset-backed runs use the external installation. | Reconfirm a clean public checkout build and a reproducible installed-data workflow. |
| Assets and setup | Eight tracks, five vehicle bundles, track terrain/ideal lines, vehicle models, human setups, AI defaults, and driver profiles load into the runtime. | Validate the selectable roster and original setup behavior against the original client. |
| Frontend and race loop | Setup, selection persistence, keyboard control, pause, route-trace replay, standings, and return-to-setup paths exist. | Native race modes, settings/rebinding, and visual/menu parity are incomplete. |
| Vehicle physics | Detailed static models, persistent PCAR constructor state, fixed-step controls, and terrain-projected wheel observations exist. | No production wheel-force/body-integrator chain; route motion remains authoritative. Controlled original input-to-motion parity is missing. |
| Opponents | AI setup records and profiles are mapped and retained. | ACAR construction, native AI callbacks, collision/passing, and physical opponent movement are not integrated. |
| Rendering | D3D11 top-down scene renders terrain, ideal line, car meshes and HUD. | Full camera/orientation, textures/lighting, and original frame comparison remain. |
| Audio | Selected car ESFX drives sample resource ownership and DirectSound buffer creation; all five car bundles have startup smoke coverage. | Band selection, gain and pitch still use provisional inputs; no synchronized output capture or parity. |
| Career, replay, multiplayer | Career file validation, replay-container codec, and multiplayer analysis exist. | Career progression, native replay playback/recording, and modern multiplayer are not integrated. |
| Original comparison | Original launcher has been made to reach its main menu, quick-race setup, grid and 3D scene using a process-local VRAM workaround; bounded input and state captures exist. | No controlled complete original race has been compared with the reconstruction. |

The full acceptance criteria and exact supporting evidence remain in the
[completion ledger](completion_ledger.md). A bounded original-client probe
attempt on 2026-10-25 suspended `race.bin` and installed read-only hooks, but
the instrumentation provider could not resume the launcher-owned thread. The
child was recovered and resumed; this attempt produced no simulation samples
and is explicitly not counted as implementation or parity progress.

## Fastest correct path to the first accepted race

Treat one Viper/Bemidji single-player race as the release gate. Work in this
order, and keep later systems out of the gate until this race is reliable:

1. **Close the input-to-motion evidence loop.** Resume the original client
   through a capture flow that keeps the launcher-created child suspended,
   installs hooks, and resumes that same child thread. Timestamp the synthetic
   DIK_UP intervention together with throttle, force/moment accumulator,
   body position, velocity and integrator entry/return. Repeat with steering,
   brake and reset. Preserve the existing data working directory and compare
   hashes before/after. Use this to decide which existing physics models have
   complete inputs; keep unknown stores fail-closed.
2. **Implement the smallest complete physical player step.** Connect contact
   observations, per-wheel force contributors and accumulator collection to
   the byte-backed PCAR state only when every consumed field and callback
   result is owned. Advance the body with the existing integrator projection,
   then make the renderer consume that body pose. Keep route kinematics behind
   a clearly marked development fallback until the physical path passes
   controlled traces; do not blend the two pose authorities.
3. **Integrate native-like opponents.** Materialize all ordered task records,
   construct ACAR session objects, identify the runtime inputs for their
   callback/timer/curve paths, and use physical or evidence-backed AI state for
   the configured roster. Validate entrant order and behavior against the
   captured original grid and race.
4. **Complete and compare the first race.** Use the capture protocol to run
   Viper/Bemidji with the same mode, car, track, laps, controls, and host setup
   in both clients. Compare setup, initial transforms, timestamped controls,
   sampled body state, lap crossings, standings, audio triggers, and return to
   setup. Fix the differences found and repeat until the scenario is
   reproducible.
5. **Broaden only after that gate passes.** Verify selectable tracks/cars and
   each advertised mode. Then complete career/save, replay playback, audio
   parity, and modern multiplayer as separate acceptance tracks. Test missing
   or corrupt assets, repeated transitions, and clean shutdown before calling
   the reconstruction complete.

### Immediate next task

Make the suspended-child capture deterministic and resumable with REA. The
last attempt has a known failure: `resume_frida_session` only resumes a spawn
owned by that session, whereas the original launcher created the child. The
capture script should let the launcher-owned delayed `ResumeThread` fire after
the child hooks are installed, or invoke `ResumeThread` from the launcher
session itself, and must verify the suspend count reaches zero before waiting
for samples. Then use the capture to populate the remaining per-wheel force
inputs before touching live physics integration.

## Completion rule

Do not mark a subsystem complete because its parser or model exists. Require
runtime integration, repeatable scenarios, and an original comparison for
behavior-sensitive claims. Update the ledger with exact scenario, environment,
outcome, evidence path, and remaining limitation after each milestone. The
overall goal is complete only when every required workstream meets its ledger
acceptance condition and the first-race regression passes from a clean public
checkout with separately supplied original assets.
