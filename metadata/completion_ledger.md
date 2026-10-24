# Playable reconstruction completion ledger

**Goal:** a complete, playable modern Windows reconstruction of Viper Racing.
Original-client interoperability is out of scope. Original game assets remain
external to this source repository; the reconstruction may load a user's
separately supplied installation. This ledger tracks observed implementation
and evidence, not reverse-engineering progress. The current `viper_game`
foundation validates five original bundles and 822 language entries, parses
Bemidji's TPPB terrain (10,431 planes and 34,712 partition nodes), normalizes
its 91-row ideal line, and draws that line as a D3D11 development view. A WARP
smoke run renders three frames and exits cleanly. The runtime still has no
playable gameplay. **Partial** means useful code or analysis
exists, but the acceptance condition is unmet; **unverified** means there is
not an integrated implementation with runtime evidence.

| Feature | Evidence / existing implementation | Runtime gap | Acceptance condition | Status |
|---|---|---|---|---|
| Build and application lifecycle | `viper_game` builds. `GameAssets` owns five original bundles, 822 language entries, parsed Bemidji terrain (10,431 planes, 34,712 partition nodes), and a normalized 91-row ideal line. The D3D11 development view draws that ideal line; WARP rendered three frames and exited cleanly. Startup evidence is in `metadata/engine_application_entry.md` and `metadata/graphics_startup.md`. | No gameplay scene or playable flow; repeated lifecycle and device/resource failure paths remain unverified. | Clean Windows checkout configures and builds `viper_game`; app starts, reaches gameplay, returns to frontend, and exits cleanly over repeat runs without stale state. | partial |
| Frontend and settings | `metadata/frontend_dispatch.md`, `metadata/race_input_bindings.md`; parsers and callback projections exist. | No integrated menus for starting a race, choosing options, controls, graphics, or audio; settings persistence unverified. | Navigate menus with keyboard/controller, change and retain supported settings, and reach race setup using only the new app. | partial |
| Shipped track inventory | `../game_install/GameData` contains `bemidji.trk`, `dundas.trk`, `hastings.trk`, `heaven.trk`, `kenyon.trk`, `limbo.trk`, `nfield.trk`, and `uptown.trk`. `metadata/world_track_objects.md` and `metadata/terrain_resource.evidence.json` document track and terrain analysis. | Inventory presence does not establish menu listing, loadability, race readiness, or selectable tracks. | Each listed track can be selected in the new frontend, loaded into a race, completed, and compared against the original using the capture protocol. | partial |
| Shipped vehicle inventory | `../game_install/GameData` contains `exotic.car`, `plane.car`, `sedan.car`, `sports.car`, and `viper.car`. `metadata/vehicle_setup.md` and `metadata/car_update_branches.md` document parsed setup and physics analysis. | Inventory presence does not establish menu listing, complete setup, drivability, or that every file represents a player-selectable car; some may be hidden or resource-only. | Verify the intended selectable roster from the original game's frontend/resources. Every supported player-selectable vehicle is selectable and drivable; classify hidden/resource-only files separately and document the evidence. | partial |
| Physics and race simulation | Wheel/body/contact models and callback compositions are documented in `metadata/simulation_loop.md`, `metadata/physics_task_objects.md`, and `metadata/vehicle_setup.md`. | Models are not a complete game simulation; integration, full race lifecycle, stable real-time behavior, and original comparison are absent. | Start a race, drive and collide, finish, and observe coherent vehicle/track state; compare controlled inputs and selected state outputs to the original. | partial |
| AI and opponents | `metadata/single_race_ai_config.evidence.json` and `metadata/single_race_entrant_plan.evidence.json` cover setup evidence. | No verified integrated opponent driving or race behavior. | A single-player race starts with configured opponents that drive to a finish; compare entrant count, order, timing, and sampled behavior. | partial |
| Race modes and results | Race-loop and setup evidence in `metadata/single_race_setup_screen.evidence.json` and `metadata/simulation_loop.md`. | No fully playable race, mode flow, results screen, or return-to-menu integration. | Inventory the modes supported by the original game; each supported mode can be configured, started, completed, show results, and return to the frontend without a crash. Record unsupported modes and evidence rather than treating implementation presence as support. | partial |
| Rendering and presentation | D3D11 runtime draws Bemidji's normalized ideal-line route as a development line strip; `metadata/scene_car_update.md`, `metadata/physics_interpolated_object_update.md`, and `metadata/graphics_startup.md` document existing renderer/overlay models. | Terrain surface, car models, HUD, frontend, and race results are not integrated in the runtime. | Playable scene renders track, cars, HUD, menus, and results at the supported display settings; compare captured frames under controlled conditions. | partial |
| Input and controls | `metadata/race_input_bindings.md` and `metadata/race_input_binding_accessors.evidence.json` contain binding evidence. | No complete, user-configurable input path wired into gameplay; controller support unverified. | Keyboard and supported controller inputs operate menus and vehicle controls; rebinding and saved settings survive relaunch. | partial |
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

## Ownership and CI boundary

Assign one owner per subsystem or ledger row at a time; coordinate interfaces
before parallel implementation and keep changes within the agreed ownership.
Public CI and checked-in fixtures must use synthetic or freely redistributable
data. Original/private game assets and captures stay outside the public source
repository unless their redistribution rights are established. A public build
must not require those assets; asset-dependent runtime acceptance is run
locally against a separately supplied installation.
