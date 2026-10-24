# Project tracking

Updated: 2026-10-24 (generic scene renderer dispatch reconstruction)

## Report two percentages

There is no single useful percentage for this project: reverse-engineering
knowledge can advance without making the game playable. Track these separately:

- **Reverse-engineering coverage** measures how much of the target's major
  behavior is understood and tied to evidence.
- **Playable reconstruction** measures how much of a playable game has been
  implemented, integrated, and verified against the original.

The second is the headline completion figure when someone asks how close the
remake is to done. Neither percentage means that every instruction or code path
in the original binary has been recovered.

## Scoring rules

Each row has a fixed weight. Assign a maturity from 0 to 4 in quarter-level
increments, then calculate `row points = weight * maturity / 4`. Sum the row
points for the percentage. Quarter levels make steady partial progress visible
without pretending a subsystem is complete. Use these anchors and interpolate
in 0.25 steps:

| Level | Meaning |
| --- | --- |
| 0 | No usable work in this area. |
| 1 | Evidence or a stub exists, but the area is mostly unknown or nonfunctional. |
| 2 | A meaningful partial analysis/model exists; major behavior is missing. |
| 3 | Most required behavior is implemented or understood; remaining gaps are bounded. |
| 4 | Integrated and verified against stated acceptance criteria and original-game evidence. |

Do not award level 4 for source code alone. For playable reconstruction, the
build must succeed and the feature must run; behavior-sensitive features also
need a comparison against the original. Record evidence and the next blocking
gap in each row. Re-score only when evidence or implementation changes, and
record meaningful partial progress in quarter-level increments, and keep each
dated score change in the history below.

## Baseline snapshot

These are coarse analyst ratings based on the repository state on 2026-10-23,
including recent compositions and successful builds. They are not claims of
exhaustive coverage. The point totals make the estimate reproducible; update
the maturity level when the evidence changes.

### Reverse-engineering coverage: 58% (57.81 / 100)

| Workstream | Weight | Level | Points | Evidence / remaining gap |
| --- | ---: | ---: | ---: | --- |
| Target and asset recovery | 10 | 4 | 10.00 | Extracted game PE, installer and assets documented. Confirm target scope and provenance in `install_layout.md`. |
| Resource and data formats | 10 | 3.25 | 8.13 | RST0/resource load paths, TIRE and track spline records, and shipped SEAS career records are decoded and connected to consumers; other payload semantics and runtime file outcomes remain open. See `resource_loading.md`, `vehicle_setup.md`, `world_track_objects.md`, `career_season_samples.md`. |
| Startup and frontend flow | 15 | 2.75 | 10.31 | Startup handoff composes command-line/config flow, initialization, task/lock setup and cleanup; frontend/race transitions include composed callback and overlay paths. The main-menu localization registrations, cache gates, full-screen controller/registry, auxiliary control, and selection callback flow are now modeled. OS/allocation results, visible meanings, and several called systems remain unknown. See `engine_application_entry.md`, `frontend_dispatch.md`. |
| Physics and simulation | 25 | 2.75 | 17.19 | Track placement, terrain/suspension/contact paths, tire response, PCAR/NCAR stages, physics-task record loading/population, and track service construction are modeled. Full race simulation, selector/helper effects, exact arithmetic, and runtime behavior remain open. See `simulation_loop.md`, `car_update_branches.md`, `physics_task_objects.md`, `world_track_objects.md`, `vehicle_setup.md`. |
| Scene and rendering | 15 | 2.25 | 8.44 | Dashboard/race-frame flow now connects state-0 callback text through layout and atlas rasterization in the race-loop composer; gear geometry and clipped pixel-write projections are also composed. DirectDraw submission/state and full scene rendering remain unknown or unimplemented. See `scene_car_update.md`, `simulation_loop.md`. |
| Input, audio, platform services, multiplayer | 10 | 1.5 | 3.75 | Menu/race input gates, startup services, and multiplayer initialization/status/packet-queue paths have partial models; the status supervisor also models its two one-time localized labels, generation refresh gates, and thread-identity cleanup deadline. Full controls, audio, transport/lobby behavior, and runtime event/timer semantics remain open. |
| Original-game behavioral validation | 15 | 0 | 0.00 | No systematic original-versus-model capture and comparison recorded. |
| **Total** | **100** |  | **57.81** | Rounded to 58%. |

### Baseline playable reconstruction: 16% (16.25 / 100, 2026-10-22)

| Workstream | Weight | Level | Points | Evidence / remaining gap |
| --- | ---: | ---: | ---: | --- |
| Buildable application and development loop | 10 | 0 | 0.00 | CMake target exists, but project notes say it has not been compiled; no runnable build verified. |
| Asset and resource integration | 10 | 2 | 5.00 | Asset catalog and bundle readers exist as source; integration into a running game is unverified. |
| Frontend and race flow | 15 | 1 | 3.75 | Numeric flow models exist; application screens and race setup are not implemented as a playable flow. |
| Simulation and vehicle behavior | 25 | 1 | 6.25 | Partial portable equations and state models exist; no complete integrated game simulation. |
| Rendering and presentation | 20 | 0 | 0.00 | No complete renderer or playable scene. |
| Input and audio | 10 | 0 | 0.00 | No integrated game controls or audio implementation. |
| Persistence, replay, networking | 5 | 1 | 1.25 | Career file structures/progression are partially analyzed, but no integrated playable services. |
| End-to-end integration and acceptance | 5 | 0 | 0.00 | No playable build or original-game comparison run recorded. |
| **Total** | **100** |  | **16.25** | Rounded to 16%. |

The 16% and 19% figures were earlier snapshots. This 28% score recognizes
additional source foundations in resource/season decoding, ranking, startup,
wheel simulation, and physics-task loading. It does not mean the game is 28%
playable: the end-to-end playable-game milestone remains at 0 until a game can
launch and complete a race.

### Current playable-reconstruction foundations: 28% (28.13 / 100)

| Workstream | Weight | Level | Points | Evidence / remaining gap |
| --- | ---: | ---: | ---: | --- |
| Buildable application and development loop | 10 | 1 | 2.50 | Full Visual Studio 18/CMake Release build succeeds and produces `build/Release/viper_reconstruction.exe`; it is a source-analysis tool, not a playable game. |
| Asset and resource integration | 10 | 2.25 | 5.63 | Asset catalog and bundle readers now include resource loading and season inspection flows; they are still not feeding a running game. |
| Frontend and race flow | 15 | 1.5 | 5.63 | Startup/race flow and season ranking inputs are composed; playable screens, complete race setup, and runtime results remain absent. |
| Simulation and vehicle behavior | 25 | 1.5 | 9.38 | Composed wheel/contact paths and physics-task record/track services deepen the model; a game-loop-integrated simulation is absent. |
| Rendering and presentation | 20 | 0.5 | 2.50 | Pixel and geometry output projections exist, but no renderer presents a playable scene. |
| Input and audio | 10 | 0.25 | 0.63 | Some input events are modeled; controls are not wired into a playable game and audio is absent. |
| Persistence, replay, networking | 5 | 1.5 | 1.88 | Career-file/progression models and decoded season records are connected to ranking; runtime race results, replay, and multiplayer remain unimplemented. |
| End-to-end integration and acceptance | 5 | 0 | 0.00 | No playable game or original-game scenario has been run; the successful executable is an analysis tool. |
| **Total** | **100** |  | **28.13** | Rounded to 28%; this is foundation work only. The playable-game acceptance milestone remains unmet. |

## Acceptance checks for level 4

- **Target/assets:** correct original executable and version are identified;
  shipped files are inventoried and supported formats are tested against real
  examples.
- **Analysis workstreams:** claims cite binary addresses or captured behavior;
  uncertainty and search boundaries are recorded; representative paths are
  reconciled with runtime evidence where available.
- **Playable workstreams:** implementation is wired into the application, the
  application builds and launches, and representative scenarios work. Compare
  timing, controls, visible output, and state changes with the original for
  behavior-sensitive systems.
- **End-to-end:** a documented scenario can start the game, enter and complete
  a race, show results, and preserve expected progress without crashes. Record
  environment, steps, and outcomes so another person can repeat it.

## Update procedure

After a meaningful change:

1. Review the acceptance check for the affected row.
2. Change its maturity level only when the evidence supports the next level.
3. Recalculate row points and totals; round headline percentages to whole
   numbers and retain the unrounded total here.
4. Add a dated history entry with the changed rows, evidence links, and any
   newly exposed blocker.

## History

- **2026-10-22:** Initial rubric and baseline from `reconstruction_progress.md`,
  `README.md`, and the current metadata/source inventory. Build and runtime
  verification are not recorded as complete.
- **2026-10-22 review:** Added the newer track-object/ILIN placement analysis
  and PCAR/NCAR wheel and callback work to the evidence notes. They deepen the
  existing partial categories without moving a workstream to a new maturity
  level; both scores remain 44% coverage and 16% playable reconstruction. No
  build toolchain or runnable build is recorded, so playable credit is unchanged.
- **2026-10-22 later review:** Added the composed race-scene/dashboard overlay
  flow and multiplayer resource lifecycle, TAPI setup/shutdown, connection
  allocation, and worker/status observations. These extend the existing scene
  and platform/network evidence, but renderer backend behavior, native
  transport/lobby behavior, and runtime validation remain open; no maturity
  level changes. Scores remain 44% coverage and 16% playable reconstruction.
  The progress notes explicitly report that no build or tests were run.
- **2026-10-22:** Expanded physics evidence for the per-wheel contact,
  suspension, collision-impulse, tire-response, smoothing, and `0x443F60`
  wheel-basis paths. The physics-analysis row remains maturity 2 because the
  full simulation and physical field meanings remain open; playable simulation
  remains maturity 1 because the model is not integrated into a running game.
  Also reconciled the state-0 LapTime note with the implemented supplemental
  row model. No percentage changed.
- **2026-10-22:** Added `scene_dashboard_gear_display_plan` for the `0x404970`
  width/mode branch: one-time resource setup, mode-specific draw calls, the
  rotating geometry parameters, and the selected-record `+0x34` clamp. Added
  binary evidence references and retained raster/device behavior as an open
  analysis item. Maturity and percentages remain unchanged; no compiler is
  available in the current environment, so this increment was not built.
- **2026-10-22:** Extended the gear display plan with the rotating pointer's
  triangle vertices and edge-color values using the `0x405510` instruction
  evidence. Host long-double trig/rounding remains an approximation to the
  original x87 path; no maturity or percentage change.
- **2026-10-22:** Added the `0x44A230` filled-triangle scanline plan to the
  gear display result, including half-open rows, renderer-height clipping, and
  the observed 1024-row scratch-array bound. Pentium FDIV/x87 precision and
  renderer-side pixel/device behavior remain open; maturity and percentages
  stay unchanged.
- **2026-10-22:** Added ordered `0x449E90` pixel-dispatch coordinates for the
  two `0x44C260` gear-outline lines. Major-axis stepping, endpoint exclusion,
  and the single-precision slope accumulator follow the observed instruction
  path; exact FDIV/x87 results and pixel writes remain open. No score change.
- **2026-10-22:** Connected the gear outline pixel calls to the existing
  viewport and pixel-format model. The plan now records clipped write
  coordinates, converted pixel values, offsets, and destination addresses;
  it does not mutate a framebuffer. The scene model now links the reusable
  render-context library. No score change.
- **2026-10-22:** Added mode-specific `0x44A1B0` triangle-fill write
  projections: direct 32-bit and 16-bit writes plus mode-1 masked writes that
  preserve destination bits. The caller supplies the existing render context;
  DirectDraw surface mutation remains unimplemented. No score change.
- **2026-10-22:** Added the state-0 redraw-all full-surface fill projection,
  including viewport-clipped bounds, mode-specific pixel encoding, mask, pitch
  offset, and first destination address. DirectDraw surface locking/writes
  remain open; no scoring change.
- **2026-10-22:** Centralized rectangle clipping and renderer-mode value
  projection, then routed both the full-surface clear and gear triangle spans
  through it. This removes duplicated mode-1 mask logic and uses the same
  half-open context clipping rule for both paths. No score change.
- **2026-10-22:** Added the background bitmap's `0x44A730` mode-pair dispatch
  and `0x4499E0` clipping projection, including adjusted source/destination
  origins, extent, pitch offsets, and helper route. Pixel conversion loops and
  DirectDraw effects remain open; no score change.
- **2026-10-22:** Implemented the recovered per-pixel bitmap conversions and
  a clipped source-buffer traversal that projects output values and destination
  addresses for the full rectangle. Incomplete source buffers return no partial
  writes; actual destination mutation and DirectDraw effects remain open.
- **2026-10-22:** Added an apply path that writes the fully validated bitmap
  transfer into caller-supplied linear destination bytes. The helper's
  surrounding `0x415090`/`0x4150A0` functions are `ret` stubs in the shipped
  assembly; runtime surface pointer integration remains open.
- **2026-10-22:** Modeled the state copy and two geometry branches from the raw
  `0x4052E0` callback. Rectangle calculations preserve the observed integer
  wrapping, resource metric inputs, call order, two suppression-gated line
  rectangles and their resource refreshes, text/font metric helper calls, and
  one begin/end batch around all emitted geometry. The text effects remain in
  their existing plans.
- **2026-10-22:** Added `apply_scene_text_pixel_writes` so the state-5 glyph
  plan can mutate a supplied framebuffer after all pixel ranges validate.
  Invalid ranges leave the buffer untouched; runtime surface acquisition and
  backend submission remain out of scope.
- Shared terrain-query reconstruction: TPPB resource loader and child fixup, quantizer, spatial partition traversal, 56-byte plane records, valid-normal guard, height solve, and wheel-contact adapter. Remaining work: prove camera/track-object callsite wiring and improve exact arithmetic fidelity. See `metadata/vehicle_setup.md` and `metadata/world_track_objects.md`.
- **2026-10-22:** Added `advance_ncar_sample_dynamics`, composing the ordinary-path `0x439520` state changes in binary order. This fills the gap between the existing derivative formulas and their actual history/ramp state mutation; the compatibility divider and x87 precision remain open. Reverse-engineering maturity and playable scores are unchanged because the callback remains a partial model without runtime comparison.
- **2026-10-22:** Fixed the escape-menu event projection after reviewing `0x4022F0`: `0x17B` is a guarded no-op, while `0x37B` writes state 11. This corrects the portable helper's prior conflation of those events. Coverage scores are unchanged; menu label-to-selection semantics and runtime UI behavior remain open.
- **2026-10-22:** Composed the active race-frame call order from `0x4020E0` in `plan_race_loop_active_frame`, including callback slots, context acquisition/submission, mode-0 viewport operations, and conditional state-4/post-submit gates. Renderer and helper results remain observations. Scores unchanged; no runtime comparison performed.
- **2026-10-22:** Recovered and modeled the state-1 input callback's two jump-table routes at `0x41A370`. The new state transition applies page and raw-coordinate stores, snapshot/restore/reset, input-byte clearing, and returns the unresolved key-translation and virtual-controller results as explicit observations. Scores remain unchanged; renderer and callback backend behavior are still incomplete.
- **2026-10-22:** Added the four state-2 physics overlay wheel-bar projections from `0x4236F0`: positions, proportional fill rectangle, frame, conditional vector normalization, indicator line endpoint, and format argument. Pixel/font/backend effects remain unimplemented. Scores unchanged because no rendered comparison was performed.
- **2026-10-22:** Added the `0x442530` contact-feature projection and applied its animation-byte stores and reset/inactive clears for fields `+0xEC`, `+0xF0`, and `+0x11C`. Evidence: `ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`. Coverage scores remain unchanged; reconstruction is still partial.
- **2026-10-22:** Composed the event-`0x31` overlay state change with the next active-frame callback/render path. This connects states 0 and 4 to their callback rows, state-4 gates, context acquisition, submission, and post-submit check; no render backend behavior was added. Scores unchanged.
- **2026-10-22:** Composed the shared state-4 paused-banner path from raw globals through generation refresh and the inner resource/fallback layout. Evidence: `0x404970`, `0x404860`, `0x40C930`, and `0x40C790` dossiers cited in `metadata/simulation_loop.md`. Coverage scores remain unchanged because the renderer is not validated.
- **2026-10-22:** Composed the state-4 race-results branch from its raw flag through heading/resource setup, row/time selection, and ordered draw arguments. Coverage scores remain unchanged because no rendered comparison has been made.
- **2026-10-22:** Connected the `0x4048E0` and `0x404900` text plans to their raw shared-overlay flags and suppression gate. Added `scene_shared_centered_resource_plan` for the separate `0x404920`/`0x4EB2FB` resource draw; coverage scores unchanged.
- **2026-10-22:** Composed the `0x404920` centered resource draw with its common suppression and `0x4EB2FB` gate. The render dimensions and resource width remain caller inputs; no pixel output was validated.
- **2026-10-22:** Composed the cheater-banner predicate from raw global flags through the centered draw call. Text dimensions remain helper inputs; renderer output remains unvalidated.
- **2026-10-22:** Composed the `0x404970` shared-overlay tail in instruction order, including the previously missing splash-loading resource lifecycle. The order follows the captured instruction stream; optional dashboard setup before the shared tail and renderer submission remain outside this plan.
- **2026-10-22:** Integrated race-loop dispatch with shared-overlay planning in `viper_race_scene_flow`; state-specific wrapper reachability now gates the `0x404970` tail composition. State-0 optional dashboard effects and backend rendering remain partial; no verification build was run.
- **2026-10-22:** Connected callback-table dispatch for `0x4052E0` in states 0 and 4 to the geometry plan; only a reached callback emits the projected rectangle/resource operations.
- **2026-10-22:** Composed the modeled `0x4035E0` core path from Position cache/resource updates through the mutually exclusive ordinary-Lap/final-time branch, and connected its reached text sites to active-frame state-0 callback dispatch. Remaining dashboard rows and draw/backend behavior stay partial; no build or tests were run.
- **2026-10-22:** Extended the `0x4035E0` composition into the following `0x40399A` LapTime block on the type-0/global-`+0x40`-false route. It derives redraw-all and prior kind-change inputs from core state and returns the existing LapTime helper's calls/state updates. TestTrack and later rows remain separate; no build or tests were run.
- **2026-10-22:** Connected the nonzero selected-record-type branch from the active `0x4035E0` callback to the existing `0x403CA8` TestTrack plan. The planner supplies the common redraw state, derives the dashboard-kind change gate, preserves the distinct record-`+0x00` displayed-position input, and carries the same state snapshot through TestTrack updates. Later supplemental and metric rows remain separate; no build or tests were run.
- **2026-10-22:** Composed the type-0 supplemental time pairs before the shared `0x403F7E` join, then the two metric cache slots and auxiliary readout after that join for all selected types. Cache/resource/redraw state now flows through these helpers in callback order; formatter inputs and measurements remain explicit observations. Gear and later operations remain outstanding; no build or tests were run.
- **2026-10-22:** Connected the existing `0x404970` gear-display projection to the state-0/state-4 race-frame wrapper path before the shared overlay tail. The wrapper's second argument now gates the optional dashboard path (state 0 enabled, state 4 disabled); dimensions, selected `+0x34`, mode, resources and render context remain inputs. No build or tests were run.
- **2026-10-22:** Connected `scene_dashboard_full_surface_fill_projection` to the reached state-0 `0x405100` dashboard-render path, alongside its operation plan. The clipped fill write projection is now available in the integrated race-frame result when redraw-all and nonzero horizontal offset select that operation; framebuffer mutation remains outside the planner. No build or tests were run.
- **2026-10-22:** Connected each reached state-0 dashboard bitmap draw operation to the existing clipped `0x44AFD0` blit plan and pixel-write projection. Results retain the source render-operation index, and pixel writes are derived only when source bytes are supplied; no framebuffer mutation or build/tests were performed.
- **2026-10-22:** Added `advance_ncar_callback_main_with_bookkeeping`, which gates the `0x431150` post-wheel bookkeeping on main-integrator reachability and places it between the known callback prefix/reset and outer `0x439200` timer tail. The four wheel updates and their produced bookkeeping values remain observations; no build or tests were run.
- **2026-10-22:** Added `advance_pcar_callback_post_integrator` to gate and order the `0x43872F` record write followed by the nearby-event scan. Backend callback values, sampled time, candidate filtering results, and record destination remain inputs; no build or tests were run.
- **2026-10-22:** Added `advance_ncar_frame_callback` to connect the `0x439200` timed-queue prefix with the input/mode callback and reached integrator bookkeeping. It reuses the frame's observed current time and event-processor gate across stages; queue record identity and wheel outputs remain supplied data. No build or tests were run.
- **2026-10-22:** Extended the PCAR post-integrator composition to apply the selected nearby-event target's three observed cue-byte stores after its record and scan stages. Target-object memory remains an explicit caller span; no build or tests were run.
- **2026-10-22:** Corrected and expanded the multiplayer post-step path: `0x40A400` delegates to `0x40A600`, which unloads entrant `.car`, difficulty, and track sets, unloads `race.res`, then loads `ui.res`. Added an ordered portable resource-action plan; the connection/lobby behavior remains unresolved, and coverage scores remain unchanged.
- **2026-10-22:** Composed `plan_multiplayer_post_step` into the active poll loop with `plan_multiplayer_frontend_detailed`, preserving one optional resource plan per observed active iteration. Coverage scores remain unchanged.
- **2026-10-22:** Added the `0x49AEA0` TAPI initialization gate model for event creation and line API negotiation. Network connection polling and lobby behavior remain open; coverage scores remain unchanged.
- **2026-10-22:** Modeled `0x49C620` connection-object allocation and the worker-thread-ID success/failure gate, including constructor publication and failed-object cleanup. Socket/line communication setup remains unresolved; coverage scores remain unchanged.

- **2026-10-22:** Extended multiplayer reconstruction through `0x485940` transport setup and `0x49B080` TAPI shutdown. Coverage and playable scores remain unchanged: native transport, connection polling, lobby behavior, and runtime validation remain incomplete. No build or tests were run.
- **2026-10-22:** Added an observation-driven model of the unowned `0x49BD40` multiplayer worker callback and documented the `0x49C2E0` status supervisor. Progress ratings remain unchanged because this does not resolve native helper semantics, lobby behavior, or end-to-end runtime validation. No build or tests were run.
- **2026-10-22:** Modeled the `0x49C2E0` status supervisor's numeric branches and status-`0x0F` unbounded string copy as explicit observations. Coverage and playable ratings remain unchanged because the state vocabulary, native callback effects, and runtime behavior are still unknown. No build or tests were run.
- **2026-10-22:** Added explicit status-label IDs and the `0x49C5C0`/vtable `+0x84` per-state flag lookup. Scoring remains unchanged because the network protocol and full state-transition effects are not reconstructed. No build or tests were run.
- **2026-10-23:** Connected RST0-backed CARF/UPGS resources through optional upgrade application into the runtime field map and composed subtype/size/placement writes on caller storage. Re-traced the dashboard car-string table, corrected its indexed stride to `0x170` bytes, and recorded native C-string overwrite boundaries. These changes deepen existing partial work without meeting level-4 integration/validation criteria; scores remain 44% reverse-engineering coverage and 16% playable reconstruction. No build or tests were run.
- **2026-10-23:** Connected the `0x463920` constructor's inputs to the modeled `0x45DEB0` table setter, deriving the index, mode, source strings, and stored object-member address from their observed records. The parser preserves safe span/NUL gates; coverage scores are unchanged because higher-level constructor effects and integration validation remain incomplete.
- **2026-10-23:** Recovered `0x45EC20`'s model-name normalization and integrated its `+0x198` runtime-record copy with CARF field mapping and placement composition. Cross-checked the string-table index source against `0x45CAE0` instruction evidence: it is the configured-car ordinal written at `+0x194`. Existing coverage scores remain unchanged; runtime validation is unavailable.
- **2026-10-23:** Connected the single-race entrant row projection to the world-car line planner using the traced `0x45B6B0`/`0x45C630`/`0x45CAB0` call chain. The adapter retains the live global cursor as an input while sourcing row marker and car stem from the 0xC8 participant row. This improves code-path integration without resolving global state or meeting runtime validation criteria; scores remain unchanged.
- **2026-10-23:** Added bounded decoding for the 35 CARF configuration words from configured row `+0x34..+0xBC`, joining the 0x45CAE0 → 0x45EC20 → 0x45E4B0 data path to the runtime mapping observation. Existing scores remain unchanged because integration and original-runtime validation are incomplete.
- **2026-10-23:** Reconstructed the `track.bpp` TPPB v2 terrain-resource envelope and child-link fixup from `0x4630B0`, `0x467060`, and `0x467130`; added a bounded parser and sampler adapter. Scoring stays unchanged because build/runtime acceptance and exact arithmetic fidelity remain open.
- **2026-10-23:** Added `--inspect-terrain BUNDLE` to expose parsed TPPB plane/node counts and the root index from an extracted track bundle; no build or runtime validation has been performed.
- **2026-10-23:** Traced terrain consumers from `0x45F600` into vehicle placement, checkpoint/obstacle setup, and camera transform `0x4282E0`; confirmed camera mode `0x421120` and wheel update `0x442530` use the full `0x45F760` point/normal/feature path. Added `sample_terrain_resource` and `terrain_height_or_zero` to connect parsed TPPB data to those call patterns. The caller-computed query coordinates remain explicit inputs.
- **2026-10-23:** Composed the four observed 0x43E7F0 moment-vector additions into the wheel/body step before per-wheel collision impulses; upstream vector derivation remains unresolved. Release build and git diff --check pass; maturity and scores unchanged.
- **2026-10-23:** Added `advance_body_integration_kinematics` to compose the recovered `0x43ECE0` linear velocity/position step, angular transform and cap, orientation update, basis synchronization, and accumulator/counter tail. Earlier force collection, virtual record callbacks, and two stack-dependent matrix words remain observations; score unchanged.
- **2026-10-23:** Added `consume_body_point_impulse_accumulator` to transfer the six duration-normalized `0x43E830` impulse/moment sums into the `0x43ECE0` running force/moment slots at scale `62.5`, with the observed epsilon gate. The six source sums clear only on the gated transfer; duration clears unconditionally. Evidence: `0x43ECE0` `ev_b86123034f29c694edd5431f554d5f1ae94e34dd0b6ed80c5280b1318bc24589`; `0x4473D0` `ev_b41be4f3332134bc2982af94ada2a60bd7d1d6240c10101ad3bdceb4728c0664`.
- **2026-10-23:** Added bounded decoding of the seven-float `0x43ECE0` records passed through the car's vtable `+0x38`, including shared duration gating and normalization of the first vector. The virtual target behavior and embedded `0x4473D0` effects remain unmodeled; no score change.
- **2026-10-23:** Added `transform_body_local_point` for the basis-plus-center mapping in `0x437BA0`, used by embedded record handler `0x4473D0`. The existing `transform_ncar_vector_at_point` models its point-velocity stage at `0x43E9B0`; terrain contact and the virtual `+0x34` response remain uncomposed. Evidence: `0x4473D0` `ev_fb5183b19769447fbe9542bad62c27b4ebc24c4fb6a73f5a6b8284a17f3ece9b`; `0x437BA0` `ev_a77d04151f0edaddec16334221ecb09d07312eacd6c6f674f349740476475d50`; `0x43E9B0` `ev_55e374cea3290f211e8f05c04418071e112cc7253a3051ed7ff43e7214c913b0`.
- **2026-10-23:** Added `prepare_embedded_record_contact` for the contact penetration, spring sign branch, feature-`0x0E` damping, and other-feature tangent friction cap prepared by `0x4473D0`. Terrain query orchestration and virtual `+0x34` response remain explicit; no score change. Evidence: `0x4473D0` `ev_fb5183b19769447fbe9542bad62c27b4ebc24c4fb6a73f5a6b8284a17f3ece9b`.
- **2026-10-23:** Extended the embedded-contact result with the capped-friction magnitude and the observed non-`0x0E`, strictly-over-`50000` gate for `0x4361E0`. Effect throttling/emission remains outside the model. Evidence: `0x4473D0` `ev_fb5183b19769447fbe9542bad62c27b4ebc24c4fb6a73f5a6b8284a17f3ece9b`.
- **2026-10-23:** Added `make_embedded_record_contact_input` to carry the successful terrain sampler's contact point, normal, and surface feature into the `0x4473D0` force model. Caller-computed query coordinates and record transforms remain inputs; failure yields no contact response.
- **2026-10-23:** Added `finish_embedded_record_update` for the contact/miss `+0x1C` store, `+0x0C` transformed-point cache write, and `+0x2C` body-center reset gate in `0x4473D0`. The virtual callback's updated force X stays a supplied value. Evidence: `ev_fb5183b19769447fbe9542bad62c27b4ebc24c4fb6a73f5a6b8284a17f3ece9b`.
- **2026-10-23:** Added `prepare_body_integration_force_collection` to compose gravity, supplied embedded-record accumulator deltas, gated wheel-impulse transfer, and normalized seven-float callback arguments in `0x43ECE0` order. It resets duration and queued-record count while leaving virtual callback effects explicit. Corrected the point-impulse helper so six source sums clear only when the duration gate passes. Evidence: `0x43ECE0` `ev_1f8bdd16767495ef26b07408d204d21f2578fe67db7c59a27a32cc583c5fb225`.
- **2026-10-23:** Resolved the embedded-record vtable `+0x34` effects: base/PCAR calls `0x430560`, NCAR thunk `0x43A280` forwards to it; the target conditionally updates `+0xE40` from force magnitude and speed, then accumulates force and moment through `0x43E6F0`. Added `apply_embedded_record_force_to_body`; force vectors are observed unmodified. Vtable and function evidence are listed in `vehicle_setup.md`.
- **2026-10-23:** Added `update_embedded_record_contact` to compose the known stages of `0x4473D0`, from local-point transform and supplied terrain result through point velocity, force preparation, body force/energy accumulation, and hit/miss state writes. Terrain-query stack-coordinate construction remains outside the composition.
- **2026-10-23:** Added `advance_four_wheel_contact_updates` to apply the recovered contact/suspension/feature-state projection across the four slots visited by `0x431150`, in `0x1A4`-byte stride order. Full wheel dynamics and body-accumulator effects remain partial; maturity and percentage estimates stay unchanged.
- **2026-10-23:** Added `apply_four_wheel_collision_impulses` to run each wheel's reconstructed collision gate in the same stride order and accumulate accepted impulses into the shared body point-impulse buffer. The body consumer is already modeled separately; the full wheel updater and runtime validation remain incomplete. Scores stay unchanged.
- **2026-10-23:** Added `advance_four_wheel_body_step` to compose the four collision gates, `0x43ECE0` force collection, supplied virtual-record accumulator effects, and kinematics/tail. This closes an implementation seam between existing physics models without resolving the remaining full-wheel callback or runtime-validation gaps; scores stay unchanged.
- **2026-10-23:** Added a bounded batch adapter for the `0x43ECE0` embedded-record loop, applying each terrain-backed record update in `0x30`-byte order to shared energy and force/moment accumulators. Missing rows are reported. The physical record count/data and runtime comparison remain inputs; scores stay unchanged.
- **2026-10-23:** Connected the terrain-backed embedded-record batch into `advance_four_wheel_body_step`, so the shared wheel/record force accumulator now feeds the body collection before kinematics. Maturity and scores remain unchanged; target runtime and callback-result validation are still absent.
- **2026-10-23:** Located Visual Studio 18 Build Tools outside `PATH`; configured and built the complete CMake `Release` target. Fixed the missing `PcarCallbackTailFields::value_0xde0` member and declared the vehicle library's NCAR-model link dependency exposed by the first build. Build succeeded; playable reconstruction advances from 16.25% to 18.75% solely in its build/development-loop row. No runtime or original-game validation was performed.
- **2026-10-23:** Added the `+0xD78` eighth-call animation gate to `advance_four_wheel_body_step`; when reached it advances four wheel timers and the observed vehicle channels via `0x4323E0`. Release build and `git diff --check` pass; reconstruction maturity and scores remain unchanged.
- **2026-10-23:** Composed the four observed 0x43E7F0 moment-vector additions into the wheel/body step before per-wheel collision impulses; upstream vector derivation remains unresolved. Release build and git diff --check pass; maturity and scores unchanged.
- **2026-10-23:** Recovered and modeled the post-0x43ECE0 direct/angular/speed-maximum tail of 0x431150, preserving helper and transform results as inputs. This narrows a significant gap in the full integrator; maturity scores unchanged. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Composed the observed mode-0/mode-1 integrator force calculation into the body step before the counted embedded records. Upstream force and point inputs remain observations; maturity scores unchanged. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Replaced the maximum-speed branch opaque helper boolean with the recovered 0x433A00 wear-threshold mask, fed by raw caller-supplied field bits. Release build and git diff --check pass; maturity scores unchanged.
- **2026-10-23:** Added the 0x431150 timeout gate and observable 0x433920 reset effects to the post-body composition, including wheel damage-byte clears. Resource payload and backend behavior remain outside the model. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Extended the 0x433920 timeout path with five bounded resource-group copies and the resolved 0x44F750/0x44F9C0 flag-4 stores. Backend virtual behavior remains unresolved. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Composed the 0x431150 paired wheel differential stores into the body step before embedded-record processing; contact observations are shared with the mode force projection. Release build and git diff --check pass; no tests were run.
# 2026-10-23 continuation: added the next 0x431150 common-force branch (contact sum <2 versus >=2) into the shared force/moment accumulator before embedded-record processing. Evidence comes from the active engine dossier and disassembly at 0x431A08..0x431C28; low-contact weighted projections and high-contact quadratic resistance are modeled, while transform outputs and field scales remain caller observations. Release build and git diff --check pass. No tests or original-game runtime comparison were performed; later force/energy instructions in this integrator are still unreconstructed.

- **2026-10-23:** Extended `0x431150` through the two following force calls and per-step energy accumulation (`0x431C2D..0x431DDD`). The two projections share the current velocity and body axis but use distinct quadratic/damping scales and application points. Release build and `git diff --check` pass; no tests or runtime comparison were performed. Later instructions through `0x43ECE0` remain only partially modeled.
- **2026-10-23:** Added the gated four-object `0x440B40` updates preceding `0x43ECE0`, including all four `+0x38` target formulas and its normalized-slip/quadratic-resistance force. The transformed per-object velocity/basis and application-point inputs remain explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected the first two `0x43E6F0` tire-force additions from each `0x442530` wheel update into the shared four-wheel body force/moment state after collision impulse accumulation. The conditional third tire force and derivation of wheel-local observations remain open. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the mode-zero, feature-gated third tire force in `0x442530` to the four-wheel body composition, including the bounded scalar weight and strict body-speed-squared gate. Raw local scale and upstream tire-vector inputs remain observations. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the subsequent wheel tail stores at `+0x120`, `+0x124`, and `+0x128`, preserving its selector-byte bit OR and supplied intermediate scalars. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Removed duplicate composition data for the wheel tail and connected the four-wheel step to `apply_wheel_tire_basis_update` and the existing `finalize_wheel_rate_state`. Corrected tire vector and response scalar now flow to the observed consumers, and collision results supply the damage flag. Maturity and scores remain unchanged.
- **2026-10-23:** Extended `derive_wheel_tire_response_vector` to combine the recovered response factors and construct the contact-normal lateral basis before normalization and the integrated basis update. Upstream factor generation and forward/contact basis derivation remain open. Maturity and scores remain unchanged.
- **2026-10-23:** Wired the existing wheel-dynamics calculation into the per-wheel body step and made its trigonometric response authoritative for the following tire-vector assembly. This removes a duplicate response input; the dynamics intermediates and other response factors remain observations. Maturity and scores remain unchanged.
- **2026-10-23:** Connected the existing TIRE curve and signed slip evaluators to response-factor construction for the quadratic normalization/blend/correction, linear curve terms, and forward slip adjustment. Runtime curve/slip inputs and wheel/mode scales remain observations. Maturity and scores remain unchanged.
- **2026-10-23:** Derived the two wheel-relative response ratios inside the per-wheel factor path using the existing wheel-kinematics helper. Raw projected speeds and the clamped reference speed remain call-site observations; factor meaning labels remain provisional. Maturity and scores remain unchanged.
- **2026-10-23:** Added the recovered ten-times-mode projection floor and low-speed weight to the tire-factor composition; the clamped projection now feeds both kinematic ratios. Mode and velocity observations remain explicit. Maturity and scores remain unchanged.
- **2026-10-23:** Derived tire curve input `+0xC4` from the observed contact-normal projection of suspension and stored wheel-response terms, its `1.0` floor, and wheel `+0x90` divisor. Component values and divisor remain explicit observations. Maturity and scores remain unchanged.
- **2026-10-23:** Derived the bounded tire-direction adjustment from curve and signed-slip terms when the divisor is nonzero. The Release build succeeds; semantic factor labels and exceptional division cases remain open. Maturity and scores remain unchanged.
- **2026-10-23:** Integrated per-wheel contact/suspension into the body-step loop in recovered order, connecting valid query normal/depth to collision data and finalized suspension force to tire curve projection. Release build succeeds; no tests were run. Remaining wheel query/tuning inputs are observations.
- **2026-10-23:** Composed `0x443F60` into the wheel body step after collision and connected its computed wheel basis to the tire response projection. Wheel orientation/body matrix observations remain. Maturity and scores remain unchanged.
- **2026-10-23:** Connected finalized suspension force to the shared force/moment accumulator at transformed wheel `+0xC8`, matching `0x431130` after `0x443F60`; the transformed point is reused for tire calls. Maturity and scores remain unchanged.
- **2026-10-23:** Removed duplicated body-center/linear-velocity observations from the composed wheel collision solve by sourcing them from the body-step kinematics, and sourced each contact point from the post-update wheel `+0x130` state when contact is composed. Matrix, mass, and response threshold stay explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reused body-step position and velocity across mode/common/velocity/attached-object force stages and the embedded-record terrain batch; propagated updated wheel contact bytes through downstream gates and post-body bookkeeping, and post-integration velocity into the post-body tail. Axes, transforms, and scale inputs remain observations. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Derived the integrated conditional tire-force gates from wheel `+0x168` and raw `+0x19C != 0`, its multiplier from the recovered constant `1.0`, and its tire scalar from the wheel tire object's first quadratic coefficient (`TIRE[13..16]` through `0x435580`). The separate response factor remains an observation. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added a four-wheel input path from parsed TIRE definitions to runtime response: it derives each wheel's curve set and trigonometric coefficients, exposes the selected curves in the result, and shares them with the conditional tire force. TIRE selection/loading remains external. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added the deterministic mouse portion of `0x444E10` to race input sampling: raw cursor globals and extents produce the normalized axes and three button bytes consumed by the selected `0x43B290` snapshot. OS sampling and `0x416D30` device refresh remain explicit. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added the successful `0x416D30` controller-state projection to periodic `0x43B290` input sampling: six normalized axes, POV direction, and 32 button bits now flow into the snapshot, with the caller's four-axis clear retained on read failure. Corrected the former keyboard/device labels to reflect their observed provenance. Release build and manual whitespace check pass; no tests were run.
- **2026-10-23:** Advanced the shared `0x431150` post-`0x43ECE0` path: derive the `+0x382` vector from the updated orientation matrix and `+0xD88` source vector, and record the unconditional final global-interface callback with its `+0x510` vehicle ordinal. The callback's native effects remain external. Release build succeeds; no tests were run.
- **2026-10-23:** Added the shared `+0x38` method from factory vtables `0x4C7810`/`0x4C7880` (`0x43BF00`): transformed position, vehicle adjustment steps, and the four-state timer/message logic are now modeled, with query and virtual-return values explicit. Evidence includes shipped method bytes `ev_6eaa26f360d1ef80114e74300fa6f1df68ec8b46da35e99d0006eff692b2544c`. Release build succeeds; no tests were run.
- **2026-10-23:** Reconstructed the `0x43A880` X/Z region predicate and wired it into the `0x43BF00` state transitions when region bounds are available. Invalid state reporting to `0x411000` is also exposed. Evidence: `ev_06979066b49eac1dc764d60124407aa440f208537c112e84a09fd1554cda86c6`. Release build succeeds; no tests were run.
- **2026-10-23:** Resolved the shared vtable's `+0x64` and `+0x68` calls in state 3: the former supplies a 60-second threshold, and the latter compares vehicle speed squared against raw constant `0x4431C71C`. Removed both caller-supplied gates from `update_alternate_mode_interface`; native display/result helper mutations remain outside the model. Release build succeeds; no tests were run.
- **2026-10-23:** Traced `0x43BF00`'s display messages through `0x46BFE0`, `0x46C0D0`, and `0x46E4F0`. Active resolved targets now receive the observed flag stores and can be appended to the bounded 32-entry queue; lookup and queue state remain inputs, and overflow reports the panic route. Release build succeeds; no tests were run.
- **2026-10-23 re-score:** The earlier whole-level scoring rule hid incremental progress. Changed to quarter-level increments and re-scored current evidence. Reverse-engineering coverage rises from 44% to 54% on composed startup, simulation, rendering, and multiplayer analysis. Playable reconstruction rises from 19% to 25% for stronger integrated model foundations; no playable game or original-runtime comparison is claimed. Recent Release builds succeed; original behavior validation remains outstanding.
- **2026-10-23 latest review:** Added shipped SEAS decoding connected to event ranking, deeper resource loader/registration paths, and physics-task record population/track service models. Release build and whitespace checks pass. Updated reverse-engineering coverage to 56% and reconstruction foundations to 28%; end-to-end playable acceptance stays at 0 because there is still no game that can launch and complete a race. No tests or original-game comparison were run.
- **2026-10-23:** Added the ordered `0x4031F0` dashboard resource/font initialization plan and composed it at its `0x401EA0` race-setup call site. The helper now preserves cache store order and widths. Scene/rendering maturity and both headline scores remain unchanged because renderer integration and original-game validation are still open; Release builds and `git diff --check` pass, no tests were run. Evidence: `ev_f63a1c54ee77e2b07363968872ef6738bfdb9961d2540c41da139ef5d11fa6e8`.
- **2026-10-23:** Added the bounded STMP payload parser and `--inspect-stamp` command, then projected the font character tables built by `0x44DA00` and added `--inspect-font`. Shipped `common.res` samples `euro6.fnt` and `cross.stp` produce complete pointer tables with in-payload fixup targets; the font map preserves both metrics without assigning labels. The resource-format maturity level remains unchanged because other payload types and original runtime comparison remain open. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`, `ev_4d95bea32a566762f91f581a174b2b9c3919d92416e39659afdfe44913a3bbee`, `ev_9c87753a7b96dd2eab7167774892dd8c6877748cc1df5c8cabe67c4f1dd8917f`, and `ev_257c3693e424fe89f3867434404a21c4e59f14182207e0decf39e99206b74e4d`.
- **2026-10-23:** Decoded the state-4 results-branch layout in `0x404970` from raw stack-slot evidence: all three row columns derive from `surface_width/2` (spilled at `0x404EA0`), rows start at `(H-0xB4)/2+0x11` after the pre-loop `add esi,14h` at `0x404F71`, and both `0x44DC70` heading texts (title `0x4EB24C`, ESC prompt `0x4EB414`) now have decoded `(font, color, flags, x, y, format)` geometry. The caller-supplied x seeds and the implicit heading payloads are gone; the swapped width/height helper constants were corrected. Scene/rendering evidence deepens without meeting level-3 criteria because DirectDraw submission and original-output comparison remain open; scores unchanged. Release build and executable smoke test pass; no tests were run.
- **2026-10-24 review:** Added the NCAR integrator's signal/smoother and four-wheel `+0xE0` setup, connected those wheel values to the existing spin-response model, and corrected `+0xDDC` to a float store from the instruction evidence. This deepens the physics model but does not meet level-3 coverage criteria: major simulation behavior and original-runtime comparison remain open. Reverse-engineering coverage remains 56%; playable-reconstruction foundations remain 28%, with end-to-end playable acceptance unmet. Release build succeeds; no tests or original-game comparison were run. Evidence is retained in `metadata/ncar_integrator_signal.evidence.json` and `metadata/wheel_update_value_e0.evidence.json`.
- **2026-10-24 continuation:** Recovered the vtable `+0x4C` wheel-buffer method at `0x432170` and replaced its opaque four-value callback result with the observed tangent, threshold, selector, lane, and max-update logic. This further deepens partial physics analysis without meeting level-3 coverage or runtime-validation criteria; the 56% reverse-engineering and 28% playable-foundation scores remain unchanged. Evidence is retained in `metadata/ncar_integrator_wheel_callback.evidence.json`.
- **2026-10-24 continuation:** Recovered the `0x433C00` vehicle-constructor stores that derive wheel-callback geometry from setup-record offsets `+0x58`, `+0x5C`, and `+0x4B0`. The wheel-scalar model can now derive `+0xD84`, `+0xD90`, and `+0xDCC`; the interpolated `+0x500` remains an existing explicit state input. Constructor value-flow is truncated, and major simulation behavior/runtime comparison remain open, so coverage scores and maturity do not change. Release build succeeds; no tests were run. Evidence is retained in `metadata/ncar_wheel_callback_setup.evidence.json`.
- **2026-10-24 continuation:** Recovered the `0x427630` record-selection wrapper and corrected the interpolated-object record stride from 0x3E to 0x3C bytes, keeping it distinct from the 0x3E-byte timed-history payload. Added a bounded ring selector and its call gate; no score or maturity change because the integrator remains partial and runtime comparison is outstanding. Evidence is retained in `metadata/ncar_interpolation_ring.evidence.json`.
- **2026-10-24 continuation:** Traced the selector's `DAT_00508A58` count to the replay loader's table-capacity check and verified `0x427630` compares the next slot against its raw fourth parameter. At the function-dossier stage, the source of that parameter was not yet recovered, so the model kept it explicit; the following instruction-level pass resolves it from object `+0xEF8`. Evidence is retained in `metadata/ncar_replay_cursor.evidence.json`.
- **2026-10-24 follow-up:** Instruction-level reads around the orphaned `0x427DA3` call resolve all four arguments: object `EBX`, record pointer `object+0xEF4`, signed frame step `global[0x508774]-object[0xEDC]-object[0xEE0]`, and upper limit `object+0xEF8`. The selector now derives this wrapped 32-bit value, preserves negative division/remainder and slots, and checks the supplied pointer origin before copying. Ghidra still has no containing function for the callsite, so earlier virtual/state gates remain open. Release build succeeds; no tests were run. Evidence is retained in `metadata/ncar_interpolation_callsite.evidence.json`.
- **2026-10-24 continuation:** Reconstructed the body integrator local matrix from the nine-float copy and subsequent overlapping stores in `0x43ECE0`, resolving the prior uninitialized-stack interpretation. Physics remains at the same maturity because native virtual effects, x87 fidelity, and whole-game/runtime validation remain open; no percentage change. Release target build and `git diff --check` pass; no tests were run. Evidence: `body_integration_basis_sync.evidence.json`.

- **2026-10-24:** Modeled the duration-normalized integrator virtual-record gate, timeout writes, and the complete type-5 event payload encoding. Ratings unchanged; nested mesh/resource mutation and runtime validation remain open.

- **2026-10-24:** Composed the known 0x430610 mesh deformation, two UV mask writes, five indexed group vertex copies, and descriptor refreshes. Confirmed the 0x450B40/0x450B20 resource descriptor layout and zero-length group behavior. Ratings unchanged because runtime mesh views and whole-game validation remain external observations.
- **2026-10-24 continuation:** Added a bounded `0x45BEB0` scene renderer frame model with the render-state gate, camera transform, viewport/state order, and object virtual dispatch; followed `0x448940` through 4x4 matrix construction, cache invalidation, device transform submission and HRESULT logging; and traced viewport setup into the `+0x44` device call and buffer clear through `+0x30`. Scene/rendering maturity remains 2.25, for reverse-engineering coverage of 56.88% (57% rounded). Playable foundations remain 28.13% because there is no integrated renderer or original-runtime comparison. Geometry submission, DirectDraw behavior, and full scene rendering remain open.
- **2026-10-24 continuation:** Extended `0x4805C0` through its one-time localized-label registration and cache refresh gates, and recorded the player-name helper and selection-result flow in `plan_main_menu_screen`. Raised startup/frontend maturity from 2.5 to 2.75. Reconciled the preceding renderer review's 2.25 scene maturity, bringing total reverse-engineering coverage from 56.88% to 57.81% (58% rounded). Playable foundations remain 28.13%; menu resource effects and original-runtime comparison remain open. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_46296befbc4dd8fa5baf76ce27d21d870878fd89e0817e31afeb7087f4d97e0e`.
- **2026-10-24 continuation:** Extended the main-menu composition through `0x472B30`'s full-screen controller path, allocation/registry request, default-layout auxiliary control, descriptor and callback setup, result read, cleanup, and shared-context restore. Kept the 2.75 startup/frontend maturity because resource-manager effects, allocation outcomes, and callback execution remain open. New evidence was merged into `metadata/frontend_dispatch.evidence.json` with its existing records preserved.
- **2026-10-24 continuation:** Extended the multiplayer status supervisor with the `Synchronizing`/`Waiting` one-time registration records, generation-driven cache refresh inputs, and detailed thread-ID/lock cleanup path. Ratings remain unchanged because network, event/timer, callback, and end-to-end runtime behavior are still incomplete. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_cc38133465aea6a6270a54acf73b71c9e8bc156a986ca08c8d3c9c20859609c3`, `ev_0e4cae09590de2e6eea747d730b5f1d08ec6ed7dc1f7dfba0e5cfdc573624561`, and `ev_d26c7a2706002627197aa284f45bb90e17ec56407bd9fffc60570c2ec13423c6`.
- **2026-10-24 continuation:** Added `raster_scene_dashboard_core_callback` to run the state-0 callback's ordered text calls through the existing font layout and atlas rasterizer, yielding ordered pixel writes and explicit incomplete-input reporting. Ratings remain unchanged because surface ownership, DirectDraw submission, full-scene coverage, and original-output comparison remain open. Release build and `git diff --check` pass; no tests were run.
- **2026-10-24 continuation:** Connected the state-0 dashboard text rasterizer to `plan_race_loop_shared_overlay`, deriving renderer context from successful frame acquisition or the active state-0 render observation. Ratings remain unchanged because this still projects text writes only; surface ownership, device submission, full-scene behavior, and original-output comparison remain open. Release build and `git diff --check` pass; no tests were run.
- **2026-10-24 continuation:** Added both ordered calls to the `0x404890` state-0 geometry suppression predicate. Its exact results-byte/`0x4EB1E8` condition is now exposed by the geometry plan; coverage ratings remain unchanged because runtime render output and broader device effects remain unvalidated. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_2d409f9f2cca7c8c137ba2687dc95a0170ec2fcdc3faa45f95cd0f5c3d465b94` and `ev_ab094b5171d133fe86ce13f0add682eed6ee00de9f12a4e79a74c0541609edca`.
- **2026-10-24 continuation:** Recovered style descriptor 14's 13 DWORD values at `0x4DE7A8` and the runtime row mapping in `0x478EE0`; the state-4 rasterizer now accepts this projection to derive zero flags and the `0x24` y offset. Resource-constructor results and runtime x offset remain inputs, and coverage ratings remain unchanged because icon/device rendering is still open. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_c31bccf8cd1585f16412312883af4a935afd06efb76b2e2dce000f0c50a13fa3` and `ev_c0e9ebbece1ee5ea54ee6633c9354485b103ea13ea9c230158836bddca2009e6`; bundle: `metadata/state4_style14_initialization.evidence.json`.
- **2026-10-24 continuation:** Added optional state-4 `0x44D0C0` resource-frame rasterization through the shared indexed-RLE path, carrying resource dimensions/origin/frame rows/palette planes and renderer context into pixel writes. Runtime resource-manager lookup and native presentation remain external; coverage ratings remain unchanged. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_3bbd3fcdbe60647b79111eef559e4f597ef123048430898b07f080d4fa525bde`; bundle: `metadata/state4_paused_banner_resource.evidence.json`.
- **2026-10-24 continuation:** Added `parse_stamp_image` and `--inspect-image`, then changed the paused-banner raster input to accept the decoded STMP object directly. `Load.stp` and `cross.stp` in `common.res` report complete pointer tables and row streams. Release build, asset inspection, and `git diff --check` pass; no tests were run. Consumer evidence bundle remains `metadata/state4_paused_banner_resource.evidence.json`.
- **2026-10-24 continuation:** Connected the state-4 `0x40C790` branch to `plan_resource_get` for its fixed `load.stp` STMP lookup, added `0x44CFD0` plus ordered `0x44D460`/`0x44D450` and `0x479450` measurement calls to the trace, and derived resource placement dimensions from `StampImage`. Runtime loaded-set/resolver outcomes remain inputs; Release build and `git diff --check` pass; no tests were run. Evidence: `0x44CFD0` `ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`, `0x40C790` `ev_1c065c92998a4cd49e818f980a1d6b5ddaba78b56125ce8b9747963977b4210b`.
- **2026-10-24 continuation:** Modeled the state-4 style-14 `0x479450` fallback-width rule and connected it to the race-loop composer. The helper calls text measurement, but style 14's zero alignment flags make its returned width `max(0x24, optional style-resource width)` for non-null text; a null pointer returns the optional resource width. The optional resource width remains an explicit input, so absent evidence marks the projection incomplete. Release build passes; no tests were run.
- **2026-10-24 continuation:** Added a runtime-font adapter for parsed STMP character maps and connected it to the paused-banner race-loop observation. It reproduces `0x44DA00`'s 256-slot initialization (`-1` glyph index and zero metrics) and record writes for index, advance, and x-offset; `euro12.fnt` in `common.res` supplies 222 populated records. Baseline helper results stay explicit inputs. Release build and `git diff --check` pass; no tests were run. Evidence: `0x44DA00` (`ev_7d05e719008e658a35c6e70c8faeef220798081bf3d6f01e67cd1b65744b9fc2`).
- **2026-10-24 continuation:** Extended `parse_stamp_font_index` to retain STMP header height and metrics, and derived the renderer's baseline values from `0x44D460`/`0x44D480`/`0x44DBC0`/`0x44DC00`. `--inspect-font` now prints those values, and parsed fonts can supply the paused-banner layout baselines without manual metric inputs. `euro12.fnt` reports height 16, top 13, bottom 3, and 222 populated records. Release build, asset inspection, and `git diff --check` pass; no tests were run. Evidence: `ev_09f7b129c2801b51a9e11c7d8851fee08efd7a59ecd1b3f35d7fced94dc5e318`, `ev_5aede7efdadbfe09f814eb67c4b0cd0d26fc5d2ad6e9ff005a08695d786d8d8d`, `ev_cec4a590ed6c06a30ddb4ae060b2ef75db678e018800f7828e65f4c5da32a8e0`, and `ev_7e1c5e7f809dd9f3d20fb7162b68127059bbdd3b59da126cc4762ad41de07bec`.
- **2026-10-24 continuation:** Added a race-loop paused-banner write sequence that merges fallback rectangles, optional STMP resource pixels, and text in native operation order. The new apply helper prevalidates the complete batch before mutating a supplied framebuffer and preserves masked rectangle writes. Release build and `git diff --check` pass; no tests were run. Surface ownership and native lock/unlock remain outside the model.
- **2026-10-24 continuation:** Added an optional raw 27-DWORD lock descriptor to render-context acquisition. `plan_render_context_acquisition` now derives the locked surface pointer from descriptor `+0x28` and pitch from `+0x10` after a successful lock; the other context fields remain sourced from their observed globals. Release build and `git diff --check` pass; no tests were run. Evidence: `0x44E210` (`ev_9370d82a0b15bd920e1a5eb259e692723161968f45b7da03a523b5b0fd1585f7`).
- **2026-10-24 continuation:** Added the state-4 paused-banner `0x479580` text path through newline splitting, font-height spacing/alignment, and ordered font/atlas pixel-write projection, then connected it to the race-loop composer. Style/localized text and renderer data remain supplied observations; resource drawing and DirectDraw presentation remain open, so coverage ratings remain unchanged. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_8ea0d058927243f23b31fa69d01a4bcf40c15e14d826b47e3affabc9e68dfe54`, `ev_cb05306e5d275df4cc6d96917141986b444a6bad6fee1922abc3d2b120591923`, and `ev_9606ba33e38b3270a8a3e18af26cc2a1748024c266184e2e06dacaa0bb1cf801`.
