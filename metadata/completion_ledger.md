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
draws Bemidji's 10,431 recovered terrain triangles with the ideal line and
parsed Viper0 indexed mesh overlaid in a top-down D3D11 development view. The
mesh is anchored at the first `obj car` placement in the selected track's
`track.obt`; its Y coordinate comes from the parsed terrain query, while its
heading remains an ideal-line approximation. The WARP smoke renders three
frames and exits cleanly. The runtime still has no playable gameplay.
The Win32 scene now displays every valid `obj car` position found in the
selected `track.obt`, using copies of the selected player model. The existing
entrant planner selects the human row; for the fresh eight-car setup, the human
is slot 8. That mesh responds to arrow/WASD input and advances along the ideal
line with provisional hand-authored speed and steering equations. Other grid
meshes stay parked; AI, collision response, laps, race timing, and results are
not active.
This is scene and input progress, not a playable race. **Partial** means useful
code or analysis exists, but the acceptance condition is unmet; **unverified**
means there is not an integrated implementation with runtime evidence.

| Feature | Evidence / existing implementation | Runtime gap | Acceptance condition | Status |
|---|---|---|---|---|
| Build and application lifecycle | `viper_game` builds. `GameAssets` owns shared bundles plus selected track/car bundles and parses terrain, ideal line, CARF, the common default tire, four axle-selected wheel tires (car-specific front/rear entries or default fallback), the selected level-0 FNIM model, per-user CSU or bundled CCS human setup, track-owned `aidef.ccs` AI base setup (CCS v2, 0x8C bytes), and `drivers.res` AIDR v2 driver groups. CSU selection is documented in `metadata/vehicle_setup.md`, supports `--config-dir`, and has a bounded parser regression for exact size and native version-warning behavior. ADNT v7 notes have a bounded raw parser and native filename-key formatter, but sidecar discovery, ILIN cross-checks, and runtime integration remain open. CLI selection is implemented for eight shipped tracks and five car bundles; local asset-validation and per-selection WARP smoke CTest cases cover each when external data is available. A private original-client startup capture is at the local manifest `../analysis/reference_captures/original_graphics_failure.json`; the client reaches graphics initialization but fails there, so it provides no gameplay comparison. The top-down D3D11 scene parses all track `obj car` positions, samples their terrain heights, renders selected-model copies, and lets the human row selected by `build_single_race_entrants` follow the ideal line via provisional keyboard-driven kinematics. This is not a race simulation. Startup evidence is in `metadata/engine_application_entry.md` and `metadata/graphics_startup.md`. | No integrated frontend-to-race flow; original-game comparison, repeated lifecycle, and device/resource failure paths remain unverified. | Clean Windows checkout configures and builds `viper_game`; app starts, reaches gameplay, returns to frontend, and exits cleanly over repeat runs without stale state. | partial |
| Frontend and settings | `metadata/frontend_dispatch.md`, `metadata/race_input_bindings.md`; parsers and callback projections exist. The runtime parses and session-owns external `options.def`, including repeated sections and empty bindings. Saved `track_no` resolves through `common.res` `tracks.tab`; saved `car_no` uses the original `*.car` file-enumeration order. The current install's `track_no 7` selects Bemidji and `car_no 0` selects Exotic. Synthetic regressions cover option parsing and STAB row access. | Full menus, settings edits/persistence, control rebinding, random-track handling, and use of race-mode/lap/difficulty options remain unimplemented. | Navigate menus with keyboard/controller, change and retain supported settings, and reach race setup using only the new app. | partial |
| Shipped track inventory | `../game_install/GameData` contains `bemidji.trk`, `dundas.trk`, `hastings.trk`, `heaven.trk`, `kenyon.trk`, `limbo.trk`, `nfield.trk`, and `uptown.trk`. CLI selection loaded all eight terrain and ideal-line resources locally. `metadata/world_track_objects.md` and `metadata/terrain_resource.evidence.json` document further track analysis. | No frontend selection, complete scene-object composition, race readiness, or drive/finish flow. | Each listed track can be selected in the new frontend, loaded into a race, completed, and compared against the original using the capture protocol. | partial |
| Shipped vehicle inventory | `../game_install/GameData` contains `exotic.car`, `plane.car`, `sedan.car`, `sports.car`, and `viper.car`. CLI selection locally loaded all five CARF payloads and their level-0 FNIM models. The parser accepts an empty fixed-width material label observed in `plane1.mod`; see `metadata/model_resource.md`. | Inventory presence does not establish the intended player roster, complete setup, drivability, or whether all bundles are player-selectable. | Verify the intended selectable roster from the original game's frontend/resources. Every supported player-selectable vehicle is selectable and drivable; classify hidden/resource-only files separately and document the evidence. | partial |
| Physics and race simulation | Wheel/body/contact models and callback compositions are documented in `metadata/simulation_loop.md`, `metadata/physics_task_objects.md`, and `metadata/vehicle_setup.md`. `GameAssets` parses and owns `common.res`'s `def_tire.tir`, the four `0x441C10` axle-selected tire records (including fallback), and the selected car's validated CCS v2, 0x8C-byte human setup record. The runtime's current motion is a hand-authored route kinematic prototype and does not call these models. | Reconstructed body-step composition is not integrated. CARF-to-wheel geometry, CCS field semantics, suspension/contact initialization, and embedded callback effects need evidence-backed mapping before runtime physics can be claimed. | Start a race, drive and collide, finish, and observe coherent vehicle/track state; compare controlled inputs and selected state outputs to the original. | partial |
| AI and opponents | `metadata/single_race_ai_config.evidence.json` and `metadata/single_race_entrant_plan.evidence.json` cover setup evidence. | No verified integrated opponent driving or race behavior. | A single-player race starts with configured opponents that drive to a finish; compare entrant count, order, timing, and sampled behavior. | partial |
| Race modes and results | Race-loop and setup evidence in `metadata/single_race_setup_screen.evidence.json` and `metadata/simulation_loop.md`. | No fully playable race, mode flow, results screen, or return-to-menu integration. | Inventory the modes supported by the original game; each supported mode can be configured, started, completed, show results, and return to the frontend without a crash. Record unsupported modes and evidence rather than treating implementation presence as support. | partial |
| Rendering and presentation | D3D11 runtime draws all three corners from Bemidji's 10,431 TPPB terrain-plane records as a top-down shaded triangle mesh and overlays the normalized ideal-line route. It parses Viper0 FNIM indices/material ranges and renders one selected-model copy for each valid `obj car` placement in `track.obt`; positions and heights come from the placement rows and TPPB, while each heading is approximated from its nearest ideal-line segment. The entrant planner chooses the moving grid row; the fresh default eight-car order selects row index 7. Release WARP rendered three frames with the grid after commit `2f753b2`; `metadata/model_resource.md`, `metadata/scene_car_update.md`, `metadata/physics_interpolated_object_update.md`, and `metadata/graphics_startup.md` document related evidence. | No camera/lighting/actual textures, HUD, frontend, moving opponents, or race results; placement heading and visual match remain unverified. | Playable scene renders track, cars, HUD, menus, and results at the supported display settings; compare captured frames under controlled conditions. | partial |
| Input and controls | `metadata/race_input_bindings.md` and `metadata/race_input_binding_accessors.evidence.json` contain original binding evidence. The runtime now reads `CONTROL.steer_left`, `steer_right`, `throttle`, and `braking` from `options.def` and applies supported keyboard names to the human-row prototype; missing options retain arrow-key defaults and empty values unbind an action. | No menus, controller support, settings editor, full original input-record integration, or original-validated motion model. | Keyboard and supported controller inputs operate menus and vehicle controls; rebinding and saved settings survive relaunch. | partial |
| Audio | `trk2.sfx` is present in the external install inventory; no integrated audio implementation is documented in the source README. | No verified engine, environment, collision, menu, or music playback/mixing. | Play representative menu and race scenarios with correctly triggered audio; record synchronized output and compare to the original. | unverified |
| Career and save data | `metadata/career_season_samples.md` documents sampled season data and progression analysis; career resource files are present externally. | No integrated career UI, race-result progression, save/load, or durable progress verification. | Start a career, complete a race, observe expected progression, quit, relaunch, and load the same state; compare save artifacts and visible progression. | partial |
| Replay | `metadata/ncar_replay_cursor.evidence.json` is static analysis evidence. | No verified replay recording, playback, seeking, or result integration. | Record a race and replay it with reproducible event ordering and visible motion; verify save/load and compare the captured replay state. | partial |
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

## Ownership and CI boundary

Assign one owner per subsystem or ledger row at a time; coordinate interfaces
before parallel implementation and keep changes within the agreed ownership.
Public CI and checked-in fixtures must use synthetic or freely redistributable
data. Original/private game assets and captures stay outside the public source
repository unless their redistribution rights are established. A public build
must not require those assets; asset-dependent runtime acceptance is run
locally against a separately supplied installation.
