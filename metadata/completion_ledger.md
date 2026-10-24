# Playable reconstruction completion ledger

**Goal:** a complete, playable modern Windows reconstruction of Viper Racing.
Original-client interoperability is out of scope. Original game assets remain
external to this source repository; the reconstruction may load a user's
separately supplied installation. This ledger tracks observed implementation
and evidence, not reverse-engineering progress. The current `viper_game`
foundation validates six original bundles and 822 language entries, parses
Bemidji's TPPB terrain (10,431 planes and 34,712 partition nodes), normalizes
its 91-row ideal line, parses the Viper version-6 CARF payload (138 DWORDs),
loads the selected track's validated 0x8C-byte `aidef.ccs` AI base setup, and
draws each selected track's recovered terrain and ideal line with selected
vehicle meshes in a top-down D3D11 view. The D3D camera follows the human grid
car at a 1.6x overview zoom. Initial car transforms now come from the
reconstructed track-line placement planner and terrain queries; subsequent
route-kinematic motion still approximates orientation from the ideal line.
The WARP smoke renders three
frames and exits cleanly. The runtime remains a route-kinematic prototype,
not yet a complete playable game.
The Win32 scene now displays every valid `obj car` position found in the
selected `track.obt`, using the selected player model for the human row and the
Viper model for AI rows. The existing entrant planner selects the human row;
for the fresh eight-car setup, the human is slot 8. That mesh responds to
arrow/WASD input and advances along the ideal line with provisional
hand-authored speed and steering equations. Forward route crossings now count
laps and stop the race at the configured `GAME laps` value; elapsed finish time
appears in the title bar. Non-player cars now move and track provisional lap
state along the same ideal-line route. The extracted-font D3D HUD shows lap,
time, route-model speed, and finish state. After all entrants finish, an
in-scene standings overlay lists their route-model finish times. Native AI,
collision response, and original-matched results behavior are not active.
This is scene and input progress, not a playable race. **Partial** means useful
code or analysis exists, but the acceptance condition is unmet; **unverified**
means there is not an integrated implementation with runtime evidence.

| Feature | Evidence / existing implementation | Runtime gap | Acceptance condition | Status |
|---|---|---|---|---|
| Build and application lifecycle | `viper_game` builds. `GameAssets` owns shared bundles plus selected track/car bundles and parses terrain, ideal line, CARF, the common default tire, four axle-selected wheel tires (car-specific front/rear entries or default fallback), the selected level-0 FNIM model, per-user CSU or bundled CCS human setup, track-owned `aidef.ccs` AI base setup (CCS v2, 0x8C bytes), and `drivers.res` AIDR v2 driver groups. CSU selection is documented in `metadata/vehicle_setup.md`, supports `--config-dir`, and has a bounded parser regression for exact size and native version-warning behavior. ADNT v7 notes have a bounded raw parser and native filename-key formatter, but sidecar discovery, ILIN cross-checks, and runtime integration remain open. CLI selection is implemented for eight shipped tracks and five car bundles; local asset-validation and per-selection WARP smoke CTest cases cover each when external data is available. A private original-client startup capture is at the local manifest `../analysis/reference_captures/original_graphics_failure.json`; the client reaches graphics initialization but fails there, so it provides no gameplay comparison. The top-down D3D11 scene parses all track `obj car` positions, samples their terrain heights, renders selected-model copies, and lets the human row selected by `build_single_race_entrants` follow the ideal line via provisional keyboard-driven kinematics. A Win32 setup selector starts the selected session; the finished-results Enter path returns to setup. `metadata/runtime_frontend.md` records the controls, asset ownership, and limits. CTest `game_frontend_launch_smoke` verifies setup-to-race loading and clean early exit. Trace-driven interactive tests now run complete Viper/Bemidji and Sports/Bemidji prototype sessions, display standings, return to setup, change cars, reload assets, and exit. Manual gameplay, physical simulation, original comparison, and native-menu parity remain open. Startup evidence is in `metadata/engine_application_entry.md` and `metadata/graphics_startup.md`. | Original-game comparison, physical race simulation, and device/resource failure paths remain unverified; the trace-driven results-return lifecycle is verified for two selected cars. | Clean Windows checkout configures and builds `viper_game`; app starts, reaches gameplay, returns to frontend after results, and exits cleanly over repeat runs without stale state. | partial |
| Frontend and settings | `metadata/frontend_dispatch.md`, `metadata/race_input_bindings.md`; parsers and callback projections exist. The runtime parses and session-owns external `options.def`, including repeated sections and empty bindings. Saved `track_no` resolves through `common.res` `tracks.tab`; saved `car_no` uses the original `*.car` file-enumeration order. The basic Win32 race setup window cycles installed `tracks.tab` entries and root `.car` bundles using keyboard input; `game_frontend_launch_smoke` verifies Enter loads the selected default session. CLI selection bypasses the window for automation. | Native menu flow, settings edits/persistence, control rebinding, random-track selection, and race-mode/lap/difficulty controls remain unimplemented. The selector's full inventory and settings writeback remain unverified; Viper and Sports have end-to-end trace-driven lifecycle coverage. | Navigate supported menus with keyboard/controller, change and retain supported settings, and reach race setup using only the new app. | partial |
| Shipped track inventory | `../game_install/GameData` contains `bemidji.trk`, `dundas.trk`, `hastings.trk`, `heaven.trk`, `kenyon.trk`, `limbo.trk`, `nfield.trk`, and `uptown.trk`. CLI selection loaded all eight terrain and ideal-line resources locally. The basic setup window enumerates installed names from `common.res` `tracks.tab` and filters to present `.trk` bundles. `metadata/world_track_objects.md` and `metadata/terrain_resource.evidence.json` document further track analysis. | Only the configured Bemidji default has been launched through the interactive selector; complete scene-object composition, race readiness, and drive/finish flow remain incomplete. | Each listed track can be selected in the new frontend, loaded into a race, completed, and compared against the original using the capture protocol. | partial |
| Shipped vehicle inventory | `../game_install/GameData` contains `exotic.car`, `plane.car`, `sedan.car`, `sports.car`, and `viper.car`. CLI selection locally loaded all five CARF payloads and their level-0 FNIM models. The parser accepts an empty fixed-width material label observed in `plane1.mod`; see `metadata/model_resource.md`. | Inventory presence does not establish the intended player roster, complete setup, drivability, or whether all bundles are player-selectable. | Verify the intended selectable roster from the original game's frontend/resources. Every supported player-selectable vehicle is selectable and drivable; classify hidden/resource-only files separately and document the evidence. | partial |
| Physics and race simulation | Wheel/body/contact models and callback compositions are documented in `metadata/simulation_loop.md`, `metadata/physics_task_objects.md`, and `metadata/vehicle_setup.md`. The Windows race session now maps the player's active 0x8C setup through CARF, retains the 0x1E4 base runtime record, derives all four FNIM anchors, and initializes the axle-specific 0x40 tire response objects. The base setup path requests no UPGS overlay because no upgrade-selection UI exists; x64 host FDIV is ordinary. Native row setup dispatch is recorded: `0x40A5D0` reaches `0x40A620`, which calls the human/AI initializers with each row's `+0x34` block before CARF mapping; the regression covers this dispatch. The CARF mapper's `+0x58/+0x5C/+0x12C` outputs now feed three exact `0x433C00` constructor scalars. The mapper also copies CARF word `0x66` directly to runtime `+0x128`; this composes through parsed FNIM bounds into the existing twelve-sphere reset-proxy planner and is regression-covered. | Suspension/contact state, embedded callback effects, and a physical pose integrator still need evidence-backed mapping and live integration. The sphere proxy is not connected to live collision handling; runtime still uses route-driven motion and does not call native wheel/body update models. | Start a race, drive and collide, finish, and observe coherent vehicle/track state; compare controlled inputs and selected state outputs to the original. | partial |
| AI and opponents | `metadata/single_race_ai_config.evidence.json`, `metadata/single_race_entrant_plan.evidence.json`, and `metadata/vehicle_setup.md` document AI row dispatch and setup mutations. `GameAssets` loads the Viper AI CARF separately and resolves the selected track row. Startup composes setup candidates for every AIDR profile, each entrant's modulo-7 slot, and both tuning modes; a local Exotic/Bemidji launch produced 100 numerically complete candidate records using the base host-division setting. A one-lap Viper/Bemidji trace run reaches finish with the player and all seven provisional opponents. | Candidate setups are not connected to AI vehicle construction or motion. The moving opponents use fixed throttle and the player's provisional kinematic model, not native AI; exact FDIV selection, native curve/note inputs, collision, passing, personality, localized names, and original-behavior comparison remain unresolved. | A single-player race starts with configured opponents that drive to a finish; compare entrant count, order, timing, and sampled behavior. | partial |
| Race modes and results | `race_progression` owns the route speed/segment integrator used by `WindowState`, accumulates race time, counts forward crossings of its initialized lap-start segment, and finishes at the configured lap count. Runtime standings appear once the player and all AI route participants finish, sorted by their recorded finish times and entrant slots; the ordering helper has regression coverage. The results screen now offers Enter to return to setup. | Trace-driven interactive tests complete Viper/Bemidji and Sports/Bemidji prototype races, show standings, and return to setup twice in one process. This does not validate manual input or physical-race behavior; mode-specific rules and original-matched standings remain unverified. | Inventory the modes supported by the original game; each supported mode can be configured, started, completed, show results, and return to the frontend without a crash. Record unsupported modes and evidence rather than treating implementation presence as support. | partial |
| Rendering and presentation | D3D11 runtime renders the selected track terrain and ideal line plus selected FNIM meshes at parsed car placements. The camera centers on the player at 1.6x overview zoom; the extracted-font HUD renders in the swap chain. Release WARP smoke cases cover eight tracks and five player car selections; `metadata/model_resource.md`, `metadata/scene_car_update.md`, `metadata/physics_interpolated_object_update.md`, and `metadata/graphics_startup.md` document related evidence. The in-scene standings overlay is now based on completed entrant states. | Camera controls, lighting/actual textures, frontend, original visual and results comparison remain unimplemented or unverified; placement heading is approximated. | Playable scene renders track, cars, HUD, menus, and results at the supported display settings; compare captured frames under controlled conditions. | partial |
| Input and controls | `metadata/race_input_bindings.md` and `metadata/race_input_binding_accessors.evidence.json` contain original binding evidence. The Win32 runtime reads the four driving bindings from `options.def`, maps virtual keys to DirectInput scan-code selectors, samples key events into a digital snapshot, and applies the reconstructed type-1 evaluator and its `>0.2` active predicate to the human-row prototype. Missing options retain arrow-key defaults; empty bindings remain inactive. `race_progression_regression` covers active, released, and unbound snapshots; `game_keyboard_binding_smoke` verifies the live Up press/release path through the race trace. | Native DirectInput polling is not implemented. No controller support, settings editor, full original input-record integration, or original-validated motion model. | Keyboard and supported controller inputs operate menus and vehicle controls; rebinding and saved settings survive relaunch. | partial |
| Audio | `SFX0` PCM and two- or three-band `ESFX` setup parsing cover all five shipped car bundles; the Win32 runtime loads the selected car's idle and numbered PCM buffers through DirectSound and selects a band/pitch from speed and throttle. A 30-frame Viper/Bemidji `--smoke-audio` run initialized DirectSound and all four Viper buffers. Details and remaining limits are in `metadata/sound_resource.md` and `metadata/vehicle_setup.md`. | Parsed native band values are not connected to runtime sound mixing because reconstructed RPM/throttle units are not yet mapped; band selection is provisional. Speaker output capture/comparison and environment, collision, menu, and music playback are missing. | Play representative menu and race scenarios with correctly triggered audio; record synchronized output and compare to the original. | partial |
| Career and save data | `metadata/career_season_samples.md` documents sampled season data and progression analysis; career resource files are present externally. The Win32 setup now opens a five-slot browser with C and uses the native-format slot reader to display stored names and revalidate a selected slot. The screen explicitly labels this as validation only; `game_frontend_launch_smoke` now opens this screen and returns to setup before launching the race. | Career create, native career action/event loop, upgrades, race-result progression, payload-to-session integration, save UI, and durable progress verification remain unimplemented. A validated file does not continue into an ordinary quick race. | Create or load a career, complete an event, observe expected progression and results, save, quit, relaunch, and verify the same state and visible progression. | partial |
| Replay and trace capture | `metadata/ncar_replay_cursor.evidence.json` is static analysis evidence. The runtime accepts `--record-trace PATH` and `--play-trace PATH`; traces contain per-frame control levels and deltas plus player/opponent route and finish states. A Windows CTest replays edited throttle/steering/braking twice and compares every state row, then runs a one-lap Viper/Bemidji trace and asserts player finish/time freeze plus all opponent finishes. This remains a diagnostic route-kinematic replay, not the native NCAR format. | Seeking, fixed-step wheel/body simulation, original-compatible event/state layout, save/load, results UI, and original comparison remain absent. | Record a race and replay it with reproducible event ordering and visible motion; verify save/load and compare the captured replay state. | partial |
| Modern multiplayer | Multiplayer protocol and status evidence exists in `metadata/multiplayer_*.evidence.json`. | No integrated modern network session, lobby, matchmaking, or verified peer race. Legacy client interoperability is excluded. | Two new clients can discover/join a session, race, receive results, and recover from disconnects on a documented supported Windows/network setup. | partial |
| Error handling and resource lifecycle | Panic, startup, and resource models are documented in `metadata/engine_panic.evidence.json`, `metadata/engine_application_entry.md`, and `metadata/resource_loading.md`. | Native error paths and resource ownership have not been exercised across a playable session. | Missing/corrupt assets and failed initialization produce actionable errors; repeated menu/race transitions and shutdown release owned resources and permit relaunch. | partial |
| Clean checkout and assets boundary | `README.md` states original executables and payload are outside this source-only repository; CMake build produces analysis tooling. | No clean-checkout playable build or user-facing external asset discovery/validation flow. | From a clean public checkout, build succeeds without private assets; app clearly locates a separately supplied game data directory and reports missing/unsupported content. | partial |

## Milestone order

1. Complete one **Viper on Bemidji** single-player race: Windows launch,
   frontend/setup, controls, track and car loading, physics, AI, rendering,
   results, and clean return. Capture it against the original using
   [`reference_capture_protocol.md`](reference_capture_protocol.md).
2. Close failures found in that scenario and document repeatable acceptance.
3. Verify all original-supported selectable tracks and vehicles. Classify
   inventory files that are hidden, resource-only, or otherwise not selectable;
   do not assume every `.car` file belongs in the player roster.
4. Complete career/save and replay, then audio and modern multiplayer.
5. Verify error handling, resource lifecycle, and clean-checkout distribution.

Update a row only when implementation or evidence changes. Link the exact
source/evidence and record the scenario, environment, outcome, and remaining
gap; do not infer runtime support from static analysis or file presence.

## Recent runtime evidence

- **2026-10-24, Viper/Bemidji route-lap completion:** Extended race traces with
  every opponent's lap/finish fields and kept opponent simulation running after
  the player finishes. The Windows CTest feeds a 1,800-frame throttle trace to
  the external Bemidji track and Viper car: the player completes the configured
  one lap, the finish clock stays frozen, and all seven route-following
  opponents reach their finish state. Playback skips intermediate renders
  while still updating simulation and samples the scene every eight frames.
  Release build and all 42 configured CTest cases passed. This is a completed
  route-kinematic prototype race; it does not validate wheel/body physics,
  native AI, a results screen, or original-game equivalence.

- **2026-10-24, deterministic input-trace playback:** Added `--play-trace PATH`.
  The replay consumes saved frame deltas and the four input levels, ignoring
  live driving keys while still allowing Escape to close. A new Windows CTest
  captures 12 WARP Viper/Bemidji frames, injects throttle/steering/braking into
  the trace, replays it twice, checks positive speed and steering response,
  and compares every resulting CSV row byte-for-byte. The test passed; all 42
  configured Release CTest cases passed. This validates determinism of the
  current route-kinematic prototype, not original physics or native replay
  compatibility.

- **2026-10-24, race trace capture:** Added `--record-trace PATH` to write
  per-frame control levels, simulation time, player route/lap state, and each
  opponent's route state. A 12-frame WARP Viper/Bemidji launch wrote the header
  and 12 frame records to the external local artifact
  `../analysis/reference_captures/reconstruction_trace_smoke.csv`. It had no
  injected keyboard input and is only a trace-format/startup check, not a race
  replay or comparison with the original. Release build and all 41 configured
  CTest cases passed after fixing the unopened-stream guard.

- **2026-10-24, provisional opponent route motion:** `WindowState` now keeps a
  separate `RouteMotionState` for each non-player grid car, projects its spawn
  onto the ideal line, and advances it with the same provisional route
  kinematics as the player. All car transforms are uploaded in one batch per
  frame. The Windows Release build succeeded and all 41 configured CTest cases
  passed, including asset validation and WARP startup coverage. No interactive
  race was run for this change. These are placeholder route followers, not
  reconstructed native AI; passing, collision response, lane choice, and AI
  behavior comparison are absent. The Viper/Bemidji completed-race acceptance
  remains unmet.

- **2026-10-24, commit `44f221f`:** Release MSVC build of target
  `viper_game` succeeded on Windows. CTest `game_warp_render_smoke` passed
  using the external `../game_install/GameData` install and WARP for three
  frames. The runtime window loads the selected Viper/Bemidji assets, renders
  every parsed `obj car` grid position with copies of the player FNIM model,
  and accepts arrow/WASD events for the first mesh. This smoke run verifies
  initialization/rendering only; no input was replayed, opponents remained
  stationary, and no original-versus-reconstruction comparison or completed
  race was recorded.
- **2026-10-24, fresh entrant selection:** Added
  `initialize_single_race_driver_order` for the observed `0x41C8E0`
  identity initialization and wired `build_single_race_entrants` into the
  Win32 runtime. In the external install, `GAME/car_count=8` plus this fresh
  order selects human row index 7; the Viper/Bemidji WARP launch logged
  `Selected human grid slot: 8 of 8` and exited after three frames. The new
  `race_participants_regression` checks the identity order and eighth-row
  selection. Full configured Windows CTest passed 36/36. Static call evidence
  and limits are in `metadata/frontend_dispatch.md`; this still does not
  verify interactive driving, AI motion, race completion, or comparison with
  the original.
- **2026-10-24, PCAR constructor adapter:** Revisited the shipped `race.bin`
  constructor chain with Ghidra evidence and added a bounded adapter from the
  0x1E4-byte car runtime record into the existing shared body-constructor
  observation. The source word and four floats at `+0x08..+0x14` are now read
  directly; the second constructor argument stays explicit. This narrows the
  record-to-body gap but does not instantiate a simulation object or change
  the runtime's provisional kinematics. The evidence and limitations are in
  `metadata/vehicle_setup.md`. Windows Release target `viper_game` built
  successfully; CTest was not run.
- **2026-10-24, setup-to-CARF adapter:** Native call evidence from the garage
  path shows the active first 0x8C bytes loaded from `.csu`/CCS flow directly
  into `0x45E4B0` as its 35 setup words. Added a bounded resource-preparation
  entry point for that buffer and a regression case for all 35 words and
  short-input rejection. The single-race participant row source, UPGS flags,
  and FDIV mode remain open; this does not claim gameplay physics integration.
  Evidence is in `metadata/vehicle_setup.md`. Windows Release build succeeded
  and full configured CTest passed 37/37, including the new regression.
- **2026-10-24, saved keyboard bindings:** The runtime's four prototype driving
  actions now use supported names from the loaded `CONTROL` section; empty
  bindings are inactive and unknown names fail with a specific diagnostic.
  Windows Release target `viper_game` built successfully. This does not validate
  the motion model or complete the controls/menu acceptance condition.
- **2026-10-24, axle tire loading:** Recovered `0x441C10`'s front/rear `f`/`r`
  resource suffixes and `def_tire.tir` fallback, then added four session-owned
  wheel tire definitions to `GameAssets`. The five external car bundles were
  checked: four contain both axle tires and Viper takes the fallback. Windows
  Release build succeeded; all 37 configured CTest cases passed, including
  asset-load and WARP startup coverage for the five cars. This still does not
  initialize wheel geometry or integrate tire forces into gameplay.
- **2026-10-24, aggregate AI setup preparation:** Added
  `prepare_single_race_ai_setups` to connect entrant plans, AIDR slot lookup,
  track/mode tuning-row selection, and 35-word setup composition. Profile
  group and tuning mode remain explicit inputs: evidence supports the eight
  difficulty-profile resource names and modulo-7 driver slot, but does not
  establish the live profile/mode selector path. Windows Release build
  succeeded and all 38 configured CTest cases passed. This remains a setup
  planner; it is not wired to live opponent vehicle initialization or motion.
- **2026-10-24, runtime AI asset/setup preparation:** `GameAssets` now owns an
  independent `viper.car` AI bundle when the selected player car differs and
  resolves its track row. The runtime composes and retains candidate setups for
  all eight AIDR profiles and both modes for each AI entrant. Against the
  external install, an Exotic/Bemidji one-frame WARP launch prepared 100
  numerically complete setup records with the base host-division setting. The
  test confirms loading and setup composition only: profile/mode selection,
  exact FDIV helper behavior, AI construction, rendering as opponents, and AI
  movement remain incomplete. Release build and CTest passed 38/38.
- **2026-10-24, opponent mesh selection:** `GameAssets` now parses and owns
  `viper0.mod` from the AI Viper bundle when the selected player car is
  different. The D3D scene uses the selected car mesh only for the human row
  and the Viper mesh for other grid rows. All 38 release CTest cases passed,
  including the external-asset WARP smoke matrix for all five cars and eight
  tracks. This changes opponent appearance only; the cars remain stationary
  and the mesh/material rendering remains provisional.
- **2026-10-24, race progression foundation:** Added a tested race clock and
  lap-completion state, connected to forward transitions over the human car's
  ideal-line route. The runtime uses `GAME laps`, freezes movement at the
  configured finish, and shows elapsed time in its title bar. Release build
  succeeded; all 39 CTest cases passed, including the new progression
  regression. A full lap driven through keyboard playback and original timing
  comparison remain unverified; this is not a completed-race acceptance result.
- **2026-10-24, route-integrator finish regression:** Extracted the runtime's
  existing segment, speed, steering, and lap transition logic into the
  platform-neutral `advance_route_motion`. `WindowState` now calls this same
  function; the regression advances a synthetic closed course until two laps
  finish. Release build and all 39 CTest cases passed. This proves the shared
  route state machine, not the Windows input/window path or a game-asset lap.

- **2026-10-24, runtime track-line placement:** The Win32 startup now composes
  `plan_car_track_line` for every visible grid row, using spline nodes decoded
  from normalized `track.ild`, the inferred endpoint key, the selected player
  row, and terrain queries at the planned X/Z. Initial visible car transforms
  preserve the planner's yaw and height. Direct Ghidra evidence shows the
  native `-grid`/`/grid` switch sets the force-default global; this switch now
  selects the modeled track-basis placement and has its own WARP smoke test.
  External Viper/Bemidji smoke logged 8 spline placements normally and 8
  forced-default placements with `-grid`; all 44 configured Release CTest
  cases pass. This integration still assumes an identity track basis, zero
  GCAR count, and `obj car` row order matching entrant order; the model's host
  floating-point path approximates original x87 rounding. There is no original
  visual comparison yet, and live route movement remains provisional.
- **2026-10-24, interactive setup-to-race session:** Added a keyboard setup
  window populated from installed `tracks.tab` entries and root `.car` bundles,
  retaining the saved selection as the initial cursor. Enter launches the
  loaded session; when standings are ready, Enter returns to setup. Race assets
  are owned by the per-race `GameAssets` value and release when that call
  returns. A Windows CTest posts input to the actual setup window, waits for
  the race window, exits the race, and verifies process exit 0. Release build
  and all 45 configured CTest cases pass. A full race-to-results-to-setup run,
  cross-selection loop, option persistence, and native menu parity remain
  unverified. See `metadata/runtime_frontend.md`.

## Ownership and CI boundary

Assign one owner per subsystem or ledger row at a time; coordinate interfaces
before parallel implementation and keep changes within the agreed ownership.
Public CI and checked-in fixtures must use synthetic or freely redistributable
data. Original/private game assets and captures stay outside the public source
repository unless their redistribution rights are established. A public build
must not require those assets; asset-dependent runtime acceptance is run
locally against a separately supplied installation.

- **2026-10-24, race HUD:** Added a Win32 overlay for lap, elapsed time, route-model speed, and the finished state. The Release build and all 43 CTest cases pass. A live WARP window capture shows the HUD and Bemidji D3D scene in the same frame after removing a GDI/DXGI white-screen regression. This remains a first-pass HUD, not a standings/results screen or original-game visual match.
- **2026-10-24, following overview camera:** Added a D3D constant-buffer transform shared by terrain, ideal line, and car draws. It centers rendering on the current human car at 1.6x zoom; the HUD resets to screen space. Live WARP capture verified the centered scene and HUD. Full external-track WARP matrix verification is pending.
- **2026-10-24, NCAR wheel anchor selection:** Reconstructed the four candidate-point formulas and nearest-FNIM-vertex scan in constructor `0x433C00` as `select_ncar_wheel_vertex_anchors`, using the mapped runtime width, ratio, and offset fields. Documented the exact scan and its limits in `metadata/vehicle_setup.md`. The Release build succeeds; tests were not run. The selected anchors are not yet connected to wheel construction or live physics.
- **2026-10-24, mapped-record wheel anchors:** Added a bounded adapter from mapped runtime fields and parsed FNIM vertex rows to the NCAR anchor selector. Regression coverage checks the four candidate positions, strict tie ordering, empty meshes, and truncated runtime records. The Release build and full local CTest suite pass 43/43 with external game data. Participant runtime-record construction and live wheel integration remain open.
- **2026-10-24, NCAR wheel core setup:** Added `initialize_ncar_wheel_core` for the evidence-backed `0x441C10` axle/side selection, anchor-relative position, front/rear track scale, steering angles, and spring-height correction. Car mass-like state and FDIV compatibility are explicit inputs; helper-owned transforms and tire allocation remain open. The Release build and focused `car_runtime_setup_regression` pass.
- **2026-10-24, four-wheel core setup composition:** Added `initialize_ncar_four_wheel_core` to compose the mapped-record anchor scan with four ordered `0x441C10` core setup results. Mass-like state and FDIV mode remain explicit, and absent mesh anchors produce an incomplete result. Targeted Windows Release build and `car_runtime_setup_regression` pass. This is a constructor-modeling seam only; participant-record production and full wheel construction remain open.
- **2026-10-24, native tire response object:** Added `initialize_tire_response_object` for all sixteen float slots populated by `0x435BF0`, using the parsed TIRE's 22 values and an explicit caller-supplied `+0x2C` value. The external argument's upstream dimension division and FDIV selection are documented; physical meanings remain unnamed. Regression checks all slots. This model is not yet connected to persistent live wheel state or simulation.
- **2026-10-24, axle-specific tire object inputs:** Added `initialize_ncar_wheel_tire_setup`, which selects the front/rear runtime setup scale, derives the loader dimension argument, divides by TIRE float 0 through the explicit FDIV path, and initializes the 0x40-byte response object. Regression covers front/rear selection, the divided slot, invalid index, and truncated record. Still not connected to live wheel state or motion.
- **2026-10-24, participant setup row identity:** Corrected post-read setup planning to resolve each configured row by its native `record_index`, matching the track-line join, rather than assuming the caller's vector order is the row order. Reordered and missing-row regression cases cover correct dispatch and incomplete-input reporting. Runtime participant row serialization and vehicle construction remain open.
- **2026-10-24, player wheel constructor inputs in runtime:** `WindowState` now owns the active setup's base CARF-mapped `0x1E4` record, four FNIM anchors, and four axle-selected TIRE response objects for the session. The path uses no UPGS overlay because the quick-race runtime exposes no upgrade selection; x64 selects ordinary host division. External-asset Viper/Bemidji WARP smoke initialization succeeds. This does not replace route-driven pose updates or implement wheel/body integration.
- **2026-10-24, body and wheel constructor bridge:** Connected the mapped player record to `plan_ncar_body_constructor`, passed its observed `+0x1F0` scale into the four-wheel core initializers, and retained all four core outputs in `WindowState`. Reference evidence from `0x42FF90`, `0x43E490`, `0x438E40`, `0x422580`, and `0x4224D0` shows constructor argument 2 is propagated from a serialized object stream; its runtime value is not currently available in the quick-race setup path. The planner now represents that input as optional and omits inherited `+0x08` when unknown while still deriving record-based dimensions. A clean isolated Windows Release build, the complete 43-test CTest suite, and the external-asset Viper/Bemidji startup smoke pass. This remains constructor initialization only; it does not change route-driven motion.
- **2026-10-24, body inverse-mass runtime input:** Retained the constructor-derived reciprocal at body `+0x200`, the exact `inverse_mass` consumed by the reconstructed `0x43ECE0` kinematics step. Regression now checks the reciprocal relation against scaled `+0x1F0`. The value is prepared for integration but body motion is not invoked because force, orientation, callback, contact, and frame-cadence inputs remain unresolved.
- **2026-10-24, terrain-validated route-state commit:** Player and opponent route state is now staged and committed only after the actual parsed terrain query succeeds. A failed player query no longer advances lap time/progress while rendering an old pose; failed opponent queries no longer let hidden route state run ahead. Regression stages a lap crossing, rejects it, verifies the complete route/race state remains unchanged, then validates the successful commit. This repairs route runtime consistency but does not substitute for missing physical motion.
- **2026-10-24, completed-race standings overlay:** When the player and every opponent finish, the HUD switches to a results list built from each retained route-race finish time, with stable entrant-slot ordering for equal times. Regression covers sorted finishers, unfinished entrants, and the player rank. This completes the current route-prototype result presentation, not native standings parity or physical-race acceptance.

- **2026-10-24, trace-driven interactive race lifecycle:** Added game_frontend_race_lifecycle_smoke and a runtime path that holds results open after trace playback only when launched through setup. The test builds a 1,800-frame Viper/Bemidji full-throttle trace, selects Viper in the frontend, completes the trace, posts Enter to return from standings to setup, changes to Sports, runs a second full trace race, returns to setup, and exits cleanly. The Release build succeeded; both frontend smoke tests and the complete 46-test suite pass. This verifies repeated frontend/session/resource lifecycle for the route-kinematic prototype; physical simulation, manual gameplay, and original parity remain unproven.
- **2026-10-24, live keyboard binding evaluator bridge:** The Win32 race window now stores key events as a digital-button snapshot and evaluates the four configured driving bindings through the reconstructed type-1 binding predicate. Empty bindings remain inactive; regression covers pressed, released, and unbound snapshots. A Windows Release build succeeded. The adapter currently indexes by Win32 virtual-key values; native selector translation, DirectInput polling, and original-game input comparison remain open.
- **2026-10-24, NCAR serialized constructor offset:** Further REA analysis established that constructor argument 2 is the byte cursor offset preceding each serialized physics object, stored at the common base object's `+0x08`. The bounded CARF runtime cannot infer it without reproducing every preceding scene object and class size, so the runtime input remains explicitly optional. The constructor model now names the field by its stream-offset meaning and regression verifies both omission and exact passthrough. No gameplay physics changed.
- **2026-10-24, DirectInput keyboard selector mapping:** Ghidra evidence established that native type-1 selectors index the keyboard device's 256-byte DirectInput state returned by `GetDeviceState`; `0x413170` reads the indexed key byte's high bit. The Win32 adapter now maps virtual keys to DirectInput scan-code indexes, including E0-prefixed keys. Added a live keyboard smoke test that records throttle on Up press and release. Native polling and controller support remain open.
- **2026-10-24, durable career slot file I/O:** Added `load_career_slot` and `save_career_slot` around the reconstructed native career codec. The store reads `Config/career/careerN.dat`, applies native header/XOR/checksum validation, writes an encoded image to a sibling temporary file, then replaces the destination. A filesystem regression covers slot path, payload round-trip, missing slot, short payload, invalid header, checksum tampering, replacement, invalid payload length, and negative slots. Windows Release build and the full 48-test CTest suite passed before the final failure-path assertions; the updated focused career-store regression passes. The runtime UI does not yet call this store; career creation semantics, menu navigation, and post-race progression remain open, and the new-career raw settings at `0x10`/`0x14` remain intentionally uninterpreted.

- **2026-10-24, runtime career slot browser:** Added a C-key screen to setup that enumerates the five observed native career slots through `load_career_slot`, displays valid stored player names, and revalidates a selection on Enter. It is explicitly a slot validator and does not start a quick race under a false career label. The Windows Release runtime links to an alternate build output because the normal executable was locked by an already-running app; its asset-backed startup smoke, the focused career-store regression, and the extended frontend launch smoke all pass. The UI smoke enters the career screen, returns to setup, launches a race, and exits cleanly. Runtime interaction with a valid on-disk save and end-to-end career progression remain unverified and open.
- **2026-10-24, installer AI-sidecar inventory:** Reconstructed the raw Mode 1 ISO from the original installer image outside this repository and extracted all three InstallShield cabinets using a temporary upstream `unshield` build. The CAB listings contain no standalone `.ili` or `.dnt` files; extracted `drivers.res`, `bemidji.trk`, and `common.res` match the existing external install by SHA-256. This confirms the already-recorded absent-sidecar finding for this installer release, while preserving the distinction that track bundles do include ILIN ideal-line resources. No original binaries or game-data payloads were added to the source repository.
- **2026-10-24, isolated full Release verification:** Because the normal `build/Release/viper_game.exe` was locked by the user's running game process, configured a fresh Visual Studio build under the system temporary directory. All targets, including `viper_game`, compiled successfully and all 48 CTest cases passed. The only compile warning surfaced was the existing unused `twice_x` local in `matrix_quaternion.cpp`; the temporary build directory is outside the source repository.
