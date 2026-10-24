# Viper Racing source reconstruction

## Runtime implementation and acceptance

The completion plan is tracked in [the feature ledger](metadata/completion_ledger.md)
and [reference capture protocol](metadata/reference_capture_protocol.md).
`viper_game` is the new Windows runtime foundation: it loads external assets,
parses the selected track's terrain and ideal line plus the selected car's
human setup, the track bundle's AI default setup, the selected car's level-0
indexed mesh, and Viper's AI mesh, and draws them in a top-down Direct3D
development view. It loads the selected track's `track.obt` placement table
and renders the selected model at the human grid slot and the native Viper
model at other valid `obj car` start positions. The human and opponents follow
the normalized ideal-line route with provisional kinematic motion. Keyboard
bindings can come from `options.def`; route crossings count laps, and the race
shows an in-scene standings overlay after all entrants finish. A Win32 setup
screen selects installed tracks and cars, and the results screen returns to
setup. Trace capture/playback, WARP rendering smoke cases, and parser
regressions support repeatable development checks. This remains a playable
route-motion prototype, not a complete reconstruction: native AI, physical
vehicle dynamics, original-matched frontend flow, career progression,
original-compatible replays, and multiplayer remain open. See the feature
ledger for current evidence and acceptance criteria.
`viper_reconstruction` remains the asset inspection tool.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./build/Release/viper_game.exe --data-dir ../game_install/GameData --validate-assets
./build/Release/viper_game.exe --data-dir ../game_install/GameData
./build/Release/viper_game.exe --data-dir ../game_install/GameData --track dundas --car sports
# Force native track-basis grid placement instead of spline placement:
./build/Release/viper_game.exe --data-dir ../game_install/GameData -grid
# Bounded graphics initialization/presentation check using software rendering:
./build/Release/viper_game.exe --data-dir ../game_install/GameData --software --smoke-frames 3
```

`--track NAME` and `--car NAME` select a `.trk` and `.car` bundle by filename
stem. The original `-grid`/`/grid` switch selects the default track-basis car
placement path; normal startup attempts native ILD spline placement. Starting
without CLI asset overrides opens a keyboard setup window: up/down selects a
track, left/right selects a car, and Enter loads the race. After the current
race prototype displays results, Enter returns to setup. Escape exits. This is
a basic runtime selector and session loop, not the original frontend or a full
settings editor. Explicit `--track`/`--car`, validation, smoke, audio smoke, and
trace-recording commands bypass the selector. `--play-trace` without explicit
track/car overrides opens setup and keeps the replayed results available for
return to setup. The runtime can load and validate the eight
shipped track bundles and five shipped car bundles. When the original data
directory is beside the checkout, CTest adds an asset-validation case for each
shipped track and car, plus a WARP rendering smoke for each selected track/car.
The Windows frontend lifecycle smoke also uses a deterministic full-lap trace
to verify Viper/Bemidji results return, selection of Sports, a second race
session, another results return, and clean shutdown. This is acceptance evidence
for the route-kinematic lifecycle, not physical-race or original-game parity.

Public CI runs without original assets. Runtime checks use an external data
directory, also configurable through `VIPER_DATA_DIR`. On Windows, CTest adds
asset validation and a three-frame WARP render smoke when
`../game_install/GameData/common.res` exists beside the checkout. Passing these checks
does not establish gameplay fidelity; original-game comparisons remain required.

Project percentages, scoring rules, and acceptance checks are tracked in
[`metadata/project_tracking.md`](metadata/project_tracking.md). It reports
reverse-engineering coverage separately from playable reconstruction.

This source-only repository contains analyst-written reconstructions based on
the shipped 32-bit Windows binaries. The original executables and extracted
game payload remain outside the repository. No original symbols or source
files were present.

The Windows runtime now validates the current `drivers.res` AIDR v2 layout
and preserves its eight groups of raw driver records; see
[`metadata/ai_driver_resource.md`](metadata/ai_driver_resource.md). The ADNT
filename builder and bounded header parser are in
[`metadata/ai_notes_resource.md`](metadata/ai_notes_resource.md); legacy AIDR
conversion and full note checksum/resource-set integration remain open.

## Current reconstruction

- `src/launcher_reconstruction.c` documents and reconstructs launcher checks,
  registry/fallback data-directory resolution, and the child-process call.
- `src/engine_init_notes.c` records DirectDraw and DirectDraw2 acquisition;
  the later queried interface is kept unresolved because its GUID/type is not
  confirmed by the current evidence.
- `src/engine_crt_reconstruction.c` captures the engine entry's environment
  and command-line setup behavior.
- `src/engine_exception_reconstruction.c` records observed floating-point
  exception mappings to internal runtime codes.
- `src/engine_panic_reconstruction.c` reconstructs the common panic message,
  logger/last-resort reporting branches, and final faulting path at
  `0x411000`/`0x411020`.
- `include/viper/startup_gates.hpp` and `src/startup_gates.cpp` model the
  `0x4120A0` single-instance semaphore/window path and the `0x412050` canary
  gate, preserving their low-byte caller results and handle/message branches;
  they also project the `0x412270` loading-window path and `0x416360`'s
  initialization call order, cleanup routes, and return gates, plus the
  `0x414F80` critical-section pool gate and its 16-slot allocator and the
  `0x413610` CPUID/TSC-versus-QPC backend selection, callback/global setup,
  and TSC calibration. `plan_application_startup_handoff` composes the observed
  `0x411F10` dedicated and non-dedicated gates, call order, conditional
  frontend entry, shutdown, and handle-finalizer routes from explicit external
  outcomes; the VxD open parameters and conditional section scan at `0x416080`
  are also projected. It composes `plan_config_directory` so invalid install
  paths reach the modeled exit branch and directory-create failures remain
  observations ignored by the native caller, then feeds the successful path
  into the `0x401280` command-line parser model. It records the `0x414110`
  heap size/flags and secondary allocator branch. The initialization plan
  consumes critical-section pool state directly; startup composes the shared
  16-name registry for `task`, `File`, `Log`, `Mono`, `BG`, and `Mem`, including the
  duplicate check and empty-slot reuse. The `BGTask` worker setup and its
  global callback-mode writes are also projected. Startup also tracks the
  32-entry file-handle registry, exception-log registration, and the
  `0x4154E0` read-only executable-overlay mapping and resource lifetime.
- `include/viper/config_directory.hpp` and `src/config_directory.cpp` model
  `0x412120`'s executable-path `\Data` check, `\Config\` derivation,
  ancestor directory-creation order, and fatal exit-code-6 paths.
- `plan_startup_shutdown` in the startup-gate model records `0x416450`'s
  initialized-flag guard, ordered cleanup calls, cleanup thunk, and flag clear.
- The startup-gate model also distinguishes `0x4161A0`'s conditional VxD
  handle close/reset from `0x412110`'s unconditional semaphore-handle close.
- `include/viper/frontend_dispatch.hpp` and `src/frontend_dispatch.cpp`
  reproduce the verified numeric selector-to-handler dispatch table from
  `0x401E10`, the verified visible main-menu selector values, and the
  pre-race screen's observed return classifications. The `0x4805C0` menu plan
  can resolve all eight registered label keys from a parsed LANGRES0 table and
  derive cache refresh gates from captured language-generation words;
  the caller-side setup/initialization routes and escape-menu numeric events
  are also modeled, while mapping those values to visible actions is still
  unknown.
- `include/viper/startup_command_line.hpp` and
  `src/startup_command_line.cpp` model the switch scan in `0x401280`, including
  `-location`, `-tri`, `-grid`, `-d`, `-ub/-ud/-ur`, `-z`, and `-1`; directory
  change success is an explicit observation and the original `-d` truncation
  of the mutable command-line tail is preserved.
- `plan_startup_frontend` in the frontend model records `0x401460`'s startup
  mode switch, mode-0 dispatcher loop, branch call, and shared finalizer.
- `plan_multiplayer_frontend` models `0x40A570`'s zeroed `0xCD4` state record
  and repeated `0x401EA0`/`0x40A400` sequence from observed `0x40A230` results;
  `plan_multiplayer_frontend_detailed` composes each active iteration with the
  observed `.car`, difficulty, track, and shared-resource teardown/load work in
  `0x40A600`; `plan_multiplayer_telephony_initialization` models the 260 event
  handles and TAPI version negotiation reached by the first connection poll;
  `plan_multiplayer_connection_object` projects its allocation and worker-ID
  acceptance gate; `plan_multiplayer_communication_setup` records the direct/line
  transport branches and UI gate; `plan_multiplayer_telephony_shutdown` models
  the 260-slot wait and handle-close cleanup; and `plan_multiplayer_worker`
  projects the callback fragment's timed poll cadence and object-presence gates;
  `plan_multiplayer_status_supervisor` models the numeric-state loop, thread
  identity gate, terminal return byte, and status-`0x0F` string-copy size and
  destination-capacity observation, plus the state-15 transition to state 16
  and its connection-gated 0x34-byte message send; `multiplayer_status_label_id` maps the
  22-entry status text table; `plan_multiplayer_status_callback` projects the
  `+0x84` per-state flag lookup from bounded ChatLine bytes. The status-15
  packet layout and immediate-versus-queued dispatch gate are projected from
  bounded inputs; packet-memory mutation, native transport results, lobby
  state, and other protocol transitions remain open.
- `plan_career_frontend` records `0x4AA460`'s career-menu polling loop and its
  save/restore of globals `0x4D10F8` and `0x4D10FC`. `career_file.hpp/.cpp`
  model the career slot path, `0x10`-byte header, `0x674`-byte XOR-`0xA4`
  payload, checksum, and observed read/write branches. The Save callback's
  `0x684`-byte file image encoder is included. The Events/Ranking/Testing/
  Upgrades/Save/Back menu, summary screen, progression loop, and testing route
  are documented. `career_progression.hpp/.cpp` model `0x4AA410`'s eight-value
  result history, the completion suffix of `0x4AA5E0`, per-event payout and
  standings updates, and the eight-row `0x4AC020` rank sort including its
  unstable equal-score ordering. The Testing selector's track/reversed outputs,
  two race modes, and post-run season-table reload are traced; upstream
  progression inputs, season-file parsing, race-result writes, and runtime
  filesystem outcomes remain open.
  `make_new_career_payload`
  reconstructs the observed New Career payload initialization used by Create;
  Delete confirmation and slot-file removal are documented.
- `include/viper/frontend_initialization.hpp` and
  `src/frontend_initialization.cpp` preserve `0x4014D0`'s ordered setup calls,
  the optional registry-derived `intro.avi` call, its 3-by-4 identity-layout
  matrix write, and its resource-load names.
- `include/viper/race_setup.hpp` and `src/race_setup.cpp` record the observed
  outer call order and branch points in `0x401EA0`, including separate
  pre-loop and post-cleanup mode checks, and project its observed three-word
  result rows; the called subsystems remain unimplemented.
- `include/viper/race_participants.hpp` and `src/race_participants.cpp`
  project `0x40A180`/`0x40A850` participant setup: row count and stride,
  the driver-order lookup for the player row, human/AI row markers and names,
  the `viper` AI car stem, raw row fields at `+0xC0`/`+0xC4`, resource
  load order, `0x40A620`'s human/AI initializer dispatch, and wrapper-level
  operation order for `0x45EFD0`/`0x41A000`; the AI configuration resource,
  fallback coefficient constants, recovered coefficient formula and tables,
  table selector, and division mode in `0x41A0C0` are also represented. The
  participant-row adapter feeds marker/car-stem values to the world-car
  placement planner at `0x45CAE0`.
- `include/viper/single_race_setup_screen.hpp` and
  `src/single_race_setup_screen.cpp` project `0x480C80`'s accepted car/track
  outputs, time-source modulo track selection, and ordered option writeback.
- `include/viper/alternate_race_mode.hpp` and
  `src/alternate_race_mode.cpp` model the alternate `0x41AB00` path's mode-3
  observation limit, 500 ms wait branch, ESC/`1` exits, and optional
  `test_prompt` wait sequence; they also model the numeric state-3 gate in the
  associated callback fragment, shared readiness/reset methods in two
  observed vtables, the shared slot-`+0x38` interface position/adjustment and
  state/timer update, and the visible lap-index window/early-stop behavior.
  X/Z region queries are derived when bounds are supplied; display and
  result-helper mutations remain caller observations. The object's virtual
  timer and velocity gates are derived from their observed implementations.
  Reached display requests update active targets and enter the bounded shared
  display queue when its state is supplied. The alternate scoreboard and
  `test_prompt` text calls now have ordered layout projections, and the
  `advance_alternate_test_prompt_workflow` connects the optional prompt to the
  `0x41F4C0` loop until three frame acquisitions succeed.
- `include/viper/entity_update_dispatch.hpp` and
  `src/entity_update_dispatch.cpp` model the `0x45BD20` virtual callback order,
  including its sorted second phase and raw float insertion rule; callback
  outcomes are supplied as input rather than invoked by this portable model.
- `include/viper/race_loop_callbacks.hpp` and
  `src/race_loop_callbacks.cpp` preserve the three callback addresses read by
  `0x4020E0` for its observed state values 0 through 5, including which slot
  receives the render context, and model key `0x31` switching state 0 to 4 or
  any nonzero state to 0 and re-entering the loop. Its `0x402960` setter sets
  the startup byte and refreshes resources, but does not set the overlay dirty
  flag. The race-loop event switch's numeric page writes at `0x4CF5B4` are also
  modeled, with their event labels left unresolved. `race_camera_control.hpp`
  and `race_camera_control.cpp` reconstruct page 11's damped keyboard camera
  translation/rotation, modes 0-12 transform setup, the shared
  look-at basis math, the `0x4259B0` camera-mode store, the preset-capture key
  edge from `0x401930`, the position/conditional angle writes from `0x4203E0`,
  and the `0x46B920` transform/velocity handoff.
  `plan_race_loop_outer_iteration` composes the ordered outer loop through the
  page dispatch and active-frame submission gates, while `update_race_loop_43b290`
  samples its 15 input bindings and applies the steering, analog, and control
  transitions. Shared-flow planning can supply the detailed DirectDraw
  acquisition inputs so lock/restore outcomes determine whether frame
  submission and later context callbacks follow. `plan_escape_menu_initialization`
  and `plan_escape_menu_event`
  retain the timeout menu's initialization guards and event routes. Platform
  polling, effect devices, and opaque callback results
  remain observations; details are in `metadata/simulation_loop.md`.
- `include/viper/scene_overlay.hpp` and `src/scene_overlay.cpp` record the
  text draw calls from the four numeric pages in the state-1 render callback,
  state-2 physics-overlay labels, four wheel-bar geometry and `%1.0f` label
  output, `c`/`w` flag updates, and the state-3
  graphics overlay's LOD/key mapping and FOG/LIGHT action bits; geometric
  drawing and most renderer/device effects remain unmodeled. State-0 dashboard
  viewport and background bitmap ordering, bitmap format dispatch and clipping,
  and full-surface fill projections are modeled from the binary's tables and
  renderer sequence. The dashboard core can derive the global object's
  virtual `+0x40` and `+0x20` results from its factory selector and bounded raw
  object bytes. It can also derive the observed dashboard record fields from
  a bounded view beginning at the callback's ESI pointer. The gear display
  model covers resource/mode gates, draw calls, pointer vertices, fill spans,
  outline pixels, pixel-format projections,
  geometry parameters, and the `+0x34` clamp. A separate helper projects the
  value of one source pixel through the recovered bitmap format conversions,
  and can walk and apply the clipped bitmap writes to supplied linear buffers.
  Other framebuffer paths and integration with the runtime surface remain
  incomplete.
  `scene_dashboard_initialization_plan` now recovers the `0x4031F0` startup
  sequence for dashboard resource and font registrations, status-text context,
  colored resources, and redraw-cache sentinels; returned handles remain
  observations.
  The raw state-0 `0x4052E0` callback now has its six-byte state copy, resource
  metric helper calls, conditional rectangle/line geometry, and batch ordering
  projected; measurements remain runtime inputs.
  Dashboard text
  call sites have a gate-projected plan, and the Position row cache gates are
  modeled along with the ordinary Lap branch's type/predicate gate, lap cache,
  the LapTime branch's signed raw-field gates and asymmetric label/time redraw,
  the TestTrack kind/resource setup and cached position row, the adjacent
  final-race-time branch, and dirty-rectangle bits. Other dashboard
  text gates and bitmap/device rendering effects remain incomplete and are documented in
  `metadata/simulation_loop.md`; the final-time result lookup and formatter now
  feed the composed callback, along with the raw type-0 supplemental rows and cached
  `+0x10`/`+0x14` metric projections, type-selected text formats, and Best/Last
  resource registration and refresh gates; the following scaled integer
  auxiliary readout and its full-redraw string calls are also modeled. The
  16-entry indexed string table used by its supplemental rows is modeled with
  the record-index lookup path. The `0x418500` locale initializer now projects
  both raw 13-DWORD presets,
  decimal-marker bytes, and multiplier source fields; the target `%s` null
  pointer fallback (`"(null)"`) is modeled from its VS 1998 formatter, and
  locale bindings feed those values into the affected dashboard row models.
- `include/viper/matrix_quaternion.hpp` and `src/matrix_quaternion.cpp`
  reconstruct the state-5 callback's row-major 3x3 to quaternion conversion,
  including the positive-trace and three diagonal branches, and provide the
  observed shortest-arc quaternion slerp and row-major quaternion-to-matrix
  conversion used by NCAR rotation updates; `scene_overlay.cpp` also models
  its rotation-vector math, camera/position text, background
  renderer-mode dispatch, glyphs, clipped pixel calls, and format-specific
  framebuffer writes, with `apply_scene_text_pixel_writes` applying the
  validated glyph writes to a caller-supplied buffer. The extracted
  8x8 font table is in
  `include/viper/scene_font_8x8.hpp` and can be regenerated with
  `tools/extract_scene_font.py`. Shared state-4
  overlay gates, cheater-banner placement, paused-banner resource refresh and
  centered/fallback layout, a recovered style-14 descriptor, line-by-line text
  rasterization, and optional loaded-resource frame rasterization from decoded
  STMP image and renderer observations. Parsed STMP fonts project their header
  baseline metrics and 256-entry glyph-index, advance, and horizontal-offset
  tables into text layout. The race-loop plan merges paused-banner pixel writes
  in native draw order and can apply them to a caller-supplied framebuffer
  after validating the complete write batch. The model also
  covers two raw-flag text setup sequences, the results heading/prompt setup, and
  results record selection, conditional formatted time values, and per-row text
  calls are modeled in `scene_overlay.cpp` with all row and heading positions
  derived from the surface dimensions. Row rasterization formats the ordinal
  integer argument before glyph layout, and the results-title/prompt calls can
  be rasterized from supplied localized strings and renderer observations; other
  renderer effects, and the exact Pentium FDIV compatibility correction remain
  partial.
  The state-0 dashboard's ordered `0x405100` full-surface fill and background
  bitmap writes can also be applied transactionally to a caller-owned surface
  when the race-loop submission/callback/unlock gates match the observed order.
  The batch also handles the mode-0 gear rectangle or, using `rpm.stp` resolved
  from the `race.res` bundle, the nonzero-mode gear resource, triangle, and edge
  writes. `parse_state0_dashboard_gear_image` performs that STMP lookup/parse.
- `metadata/frontend_dispatch.md` records the recovered routing evidence and
  remaining uncertainty.
- `include/viper/vehicle_setup.hpp` and `src/vehicle_setup.cpp` implement the
  observed vehicle unit conversions, wheel-lock bit-range check, fixed-size
  TIRE payload decoder, tire and wheel response evaluators including the
  trigonometric curve at `0x435780` and signed slip normalizer at `0x4356D0`,
  plus wheel slip smoothing/state writes and the `0x443F60` wheel angle/basis
  update, kinematic ratios,
  response-vector assembly and correction, the wheel contact gate and
  suspension scalar, surface-adjustment, and finalization blocks, and a normal
  contact-response/impulse path; `apply_wheel_collision_impulse` connects the
  accepted collision response to the conditional wheel-flag write and
  `0x43E830` point-impulse accumulator;
  `apply_four_wheel_collision_impulses` carries the four collision gates into a
  shared body accumulator in integrator order; `advance_four_wheel_body_step`
  now interleaves the observed per-wheel `0x43E7F0` moment-vector additions
  before each wheel's collision impulse, then accumulates the two supplied
  `0x43E6F0` tire-force vectors in wheel order; when enabled, the per-wheel
  contact/suspension pipeline runs between moment accumulation and collision,
  and supplies the `+0xC4` projection terms; it accumulates finalized suspension
  force at transformed wheel `+0xC8` before tire forces. The step then advances wheel tire
  basis state from `0x443F60`, advances wheel tire dynamics, derives the
  projected curve input and response factors, assembles
  the contact-normal lateral basis, applies the `1e8` normalization cap and
  `+0xF4/+0x100/+0x7C` projection, accumulates the two tire forces and the
  mode-zero feature-gated third force, then runs the
  `+0x120/+0x124/+0x128` tire-state finalizer. The computed tire projection
  feeds both force calls; upstream vectors, bases, and points remain inputs;
  `evaluate_wheel_suspension_pipeline` connects the three scalar stages;
  `advance_four_wheel_contact_updates` applies that contact/suspension projection
  across the four wheel slots in the `0x431150` `0x1A4`-byte stride order;
  `compose_wheel_tire_response` connects those wheel response stages in their
  observed order. It also includes the recovered linear
  body-velocity integration, speed cap,
  position step, angular-velocity cap, orientation matrix step, conditional
  basis synchronization products, and integrator tail clears; the
  `advance_body_integration_kinematics` composition joins those body stages in
  their observed order; `consume_body_point_impulse_accumulator` now maps the
  duration-scaled wheel impulse/moment sums into the body's six running
  accumulators before kinematics, and
  `materialize_body_integration_virtual_record_calls` decodes the adjacent
  duration-gated seven-float callback records;
  `prepare_body_integration_force_collection` joins the recovered gravity,
  embedded-record accumulator inputs, impulse transfer, and callback argument
  decoding before kinematics; `advance_four_wheel_body_step` connects the
  four-wheel collision accumulator, observed mode-0/mode-1 force branch and
  the initial `+0x4FC` X/Z velocity reset and `+0xE3D` vertical-force prefix,
  plus the `0x433570` time-window ramp and caller-supplied ordinal-record gate,
  and the inverse `0x432130` out-of-bounds callback gate,
  then the guarded `0x4402D0` curve-byte and `+0xDE0` setter path,
  followed by the composed `+0xC98` child initialization and `0x43FAE0`
  animation-controller update,
  contact-gated wheel-pair differences, the following contact-count split and
  low-contact resistance/high-contact quadratic force projection, and the two
  post-resistance velocity/projection forces with their per-step energy sum;
  the gated four-object `0x440B40` force law and its four target-angle stores
  are also composed before body integration,
  eighth-step animation gate, counted
  terrain-backed embedded records, body collection/kinematic tail, and the
  direct/angular/max-speed and timeout-triggered reset updates after
  `0x43ECE0`.
  The code also includes `transform_body_local_point`
  for the embedded-record basis transform at
  `0x4473D0`, plus `prepare_embedded_record_contact` for its feature-specific
  spring and damping/friction force before the virtual callback, with
  `make_embedded_record_contact_input` adapting successful terrain-query
  results and `finish_embedded_record_update` for its hit/miss tail stores, as
  well as `apply_embedded_record_force_to_body` for its PCAR/NCAR virtual force
  accumulator and energy effects. `update_embedded_record_contact` composes
  those stages when the terrain result is supplied. World car-variant tag
  mapping is also included; `update_embedded_record_contact_batch_from_terrain`
  applies the counted 0x30-byte embedded-record loop against parsed terrain
  while sharing its energy and force/moment accumulators;
  `metadata/vehicle_setup.md` records their binary evidence and the traced
  car-to-wheel update path.
- `include/viper/resource_bundle.hpp` and `src/resource_bundle.cpp` read the
  recovered RST0 `.res` and `.car` resource containers;
  `parse_car_file_resources` connects those entries to CARF/UPGS parsing, and
  `metadata/resource_loading.md` records the file layout and loader evidence.
- `metadata/reconstruction_notes.md` ties those fragments to observed
  addresses, strings, imports, and unresolved helper behavior.
- `metadata/engine_function_priority.md` queues the next reachable engine
  functions for manual tracing.
- `disasm/` contains full x86 `.text` disassemblies.
- `metadata/pe_report.md` and `metadata/strings.md` record PE structure,
  imports, hashes, and string RVAs.
- `metadata/*_function_candidates.csv` contains heuristic control-flow block
  starts, not verified function boundaries.
- `tools/analyze_binaries.py` regenerates the static artifacts from extracted
  binaries when the `pefile` and `iced-x86` Python packages are available.
- `CMakeLists.txt`, `include/viper/asset_catalog.hpp`, and the portable asset
  catalog utility provide the first C++20 build target without copying game
  assets into this repository.
- `metadata/engine_application_entry.md` records direct Ghidra evidence for
  the engine's application startup handoff.
- `include/viper/legacy_fdiv.hpp` and `src/legacy_fdiv.cpp` model the observed
  MSVC 1998 FDIV-compatibility initialization and its affinity-probe ordering;
  runtime API and arithmetic results remain explicit observations. Details are
  in `metadata/legacy_fdiv.md`.
- `metadata/simulation_loop.md` and `metadata/physics_task_objects.md` record
  the main entity callback order, elapsed-time substep scheduler, object-array
  factory, the shared scene/physics car records, and verified conditional
  car-to-body update paths.
- `include/viper/scene_car_update.hpp` and `src/scene_car_update.cpp` model the
  observed scene-car numeric state selection, 99-float pose copy, 16-word edge
  comparison, and four-wheel transform arithmetic;
  `metadata/scene_car_update.md` records the indirect callback, transform
  layout, effect-row gates, and unresolved resource semantics.
- `include/viper/scene_renderer_dispatch.hpp` and
  `src/scene_renderer_dispatch.cpp` model the `0x45BEB0` render gate, camera
  transform, graphics-state setup/restore sequence, and bounded virtual-method
  dispatch over registered scene objects. Its 4x4 view-matrix installer,
  transform-cache invalidation, viewport record, buffer-clear calls, and
  device HRESULT logging are projected. Wheel-manager draws preserve their
  `0x448ED0` → `0x452700` forwarding and device vtable `+0x78` contract; actual
  graphics execution and per-object virtual return values remain observations.
- `include/viper/world_track_objects.hpp` and
  `src/world_track_objects.cpp` model the BATS `track.obt` table, its
  checkpoint-derived track basis/translation setup, circular spline-segment
  selection, ILIN geometry parsing/normalization, point-to-track projection,
  and car pose math, top-level
  directive classification, and checkpoint, wobble, static-box, and obstacle
  parsing, including checkpoint terrain-height endpoints, route selection, raw
  record fields, static rotation conversion, and obstacle model-bound
  calculations, plus CARF v6 parsing/field mapping, RST0-backed `.cf`/`.ugs`
  lookup, configured-row extraction, bounded UPGS overlay, model-name
  normalization, and runtime-record composition, car subtype/placement
  planning, ACAR adjustments,
  the configured BALL effect record, and a
  composed ILIN key-to-segment-to-pose plan connected to car-line planning
  and its `+0x18` runtime-record transform write;
  `metadata/world_track_objects.md` records binary evidence and the remaining
  track-loader gaps.
- `include/viper/terrain_surface.hpp` and `src/terrain_surface.cpp` decode
  the track bundle's `track.bpp` TPPB v2 plane/tree payload and feed its
  normalized records into the shared terrain sampler. The parser uses bounded
  records and index checks; the original loader's panic path remains documented
  separately. Static evidence is retained in
  `metadata/terrain_resource.evidence.json`.
- `metadata/car_update_branches.md` records the numeric branch gates and
  integration paths in the `PCAR` and `NCAR` physics callbacks; their opaque
  record fields and remaining helper effects remain unresolved.
- `include/viper/ncar_state.hpp` and `src/ncar_state.cpp` preserve recovered
  NCAR constructor/destructor lifecycle, timing fields, and observed fields of each
  `0x3E`-byte record
  while preserving raw bytes, update the three-slot sample window, model the
  quadratic-derivative outputs, accepted-sample-to-linked-target callback,
  timed interpolation, weighted vector blend, wrapped tick and seconds
  conversion, packed rotation decoding/encoding, and the 24-byte payload
  producer/consumer mappings. `advance_ncar_callback_queue` connects the
  1/15-second producer cadence to the timed FIFO and accepted sample-buffer
  path; `apply_ncar_callback_gate_bytes` models the callback's four normalized
  sample flags. `select_ncar_interpolated_records` models the separate 0x3C-byte
  interpolation ring (distinct from the 0x3E-byte history payload). It derives
  its signed selector inputs from the recovered `0x427DA3` object fields;
  `dispatch_ncar_interpolated_object` composes the outer flag, callback,
  registered-transform, time-gate, ring-selection and tail-counter behavior.
  The 16-entry `0x507D70` object registry also has bounded registration and
  lookup projections; `plan_physics_auxiliary_pass` composes the ordered
  `0x432300` radius-weighted accumulator updates while keeping native
  out-of-range accesses explicit.
  `advance_ncar_callback_main` composes the recovered input,
  mode-3 setter/gate, and timer branches around the unreconstructed integrator
  body; it includes the integrator's entry-reset prefix.
  `advance_ncar_timed_callback` models the `0x43A130` half-second chain,
  sample-window callback arguments, observed latch writes, and the `0x433620`
  virtual target's pose/velocity resets, two secondary render-group passes
  (copying child vector state and clearing dirty bytes), finite-normal
  FDIV-compatible basis construction, the four child-slot field resets, five
  bounded resource-group copies, and event append.
  `advance_ncar_rotation_blend` models `0x439DB0`'s three-slot quaternion
  interpolation and row-major orientation-matrix update.
  `advance_ncar_position_prediction` models `0x439A10`'s sample-based position
  extrapolation, derivative, reset gate, and weighted position/vector updates.
  `advance_ncar_sample_dynamics` applies `0x439520`'s history shift, current
  and history derivatives, 9.81 magnitude cap, and interpolation-ramp update
  as one ordered state transition, with an optional approximation of the
  finite-normal Pentium-FDIV compatibility branch.
  `metadata/ncar_sample_dynamics.md` documents its body offsets, x87 precision
  limits, and the remaining CRT compatibility-helper gaps.
  `advance_ncar_timed_frame` composes the three helpers behind the `0x43A130`
  main gate and its later independent sample-flag callback.
  `accumulate_ncar_force_moment` reconstructs the shared `0x43E6F0` force and
  application-arm moment writes used by the NCAR integrator.
  `transform_ncar_vector_at_point`, `accumulate_ncar_moment_vector`, and
  `accumulate_ncar_point_impulse` cover three more shared body helpers used by
  the per-wheel update path.
  `advance_ncar_integrator_bookkeeping` models the eight-step counter and
  direct time/sum/copy stores after the four per-slot integrator updates.
  `setup_ncar_integrator_child_control` models the adjacent embedded-object
  stores, while `advance_ncar_animation_controller` models that controller's
  six transition states and applies its two direct child-target writes.
  `advance_ncar_integrator_signal` follows that controller with the observed
  float `+0xDDC`/byte `+0xE49` stores and the bounded/smoothed `+0xC7C` signal
  helper. `advance_ncar_integrator_wheel_scalars` applies the `+0x4C` callback's
  recovered tangent-scaled, per-lane max updates and conditional sign reversal
  before the strided wheel `+0xE0` writes, which feed their absolute values into
  the modeled wheel spin response stage. The tangent/x87 path remains
  approximate. Constructor-derived callback inputs can be enabled to compute
  `+0xD84`, `+0xD90`, and `+0xDCC` from the vehicle setup record; `+0x500`
  remains sourced from interpolated object state.
  Exact x87 rounding and the Pentium FDIV-emulation
  branch remain under study.
- `include/viper/timed_event_queue.hpp` and `src/timed_event_queue.cpp` model
  the NCAR timed FIFO and its chat-configured latency offset. The observed
  handler stores parsed values from 0 through 999 directly into the offset.
- `include/viper/pcar_control_update.hpp` and `src/pcar_control_update.cpp`
  model the finite-value `0x4330F0` clamp and four direct `PCAR` scalar stores,
  preserving their numeric field offsets, and cover the late callback helper
  branches that write `+0xC3C`, `+0xDE0/+0xDE4`, and `+0x508`, plus the raw
  five-DWORD post-integrator record writer and the nearby-cue target-byte
  notification at `+0x29..+0x2B`.
- `include/viper/update_curve.hpp` and `src/update_curve.cpp` model the
  piecewise curve arithmetic and PCAR embedded-object candidate selection
  plus the observed curve configuration arithmetic; the selector's virtual
  getter has no observed state effect, and raw offsets are preserved.
- `include/viper/language_resource.hpp` and `src/language_resource.cpp` parse
  LANGRES0 language tables as bounded key/value offset pairs. The
  `--inspect-language PATH [KEY]` command lists labels or resolves one key;
  the state-0 TestTrack callback can use a parsed table to resolve its three
  localized labels. Its composed callback can also bind distance formatting
  from the optional locale state produced by the `0x418500` model.
- `include/viper/legacy_allocator.hpp` and `src/legacy_allocator.cpp` model
  the optional VxD allocation marker and fill behavior; portable execution can
  use the normal host heap instead of opening `\\.\vexed.vxd`.
- `include/viper/legacy_timer.hpp` and `src/legacy_timer.cpp` model the
  legacy timer worker's interval wait, late-callback yield, stop, and overrun
  backoff decisions without opening the VxD device.
- `include/viper/render_context.hpp` and `src/render_context.cpp` model the
  race loop's render viewport clamp, full-surface reset, pixel containment,
  row-pitch offset, and tag-selected color conversion.
- `include/viper/graphics_startup.hpp` and `src/graphics_startup.cpp` compose
  `0x44E210`'s message-pump/readiness gates, observed surface-lock retries,
  two-surface recovery, null-page error, and successful render-context setup.
- `include/viper/physics_scheduler.hpp` and `src/physics_scheduler.cpp` model
  the fixed-step accumulator state branches, elapsed-time estimate, catch-up
  caps, anchor recapture, and mode-0 throttling guard.

## Build the current tools

With CMake 3.20+ and a C++20 compiler installed:

```sh
cmake -S . -B build
cmake --build build --config Release
./build/viper_reconstruction --list-assets --data-dir ../game_install
./build/viper_reconstruction --list-resources ../game_install/GameData/common.res
./build/viper_reconstruction --inspect-tire ../game_install/GameData/common.res def_tire.tir
./build/viper_reconstruction --inspect-car ../game_install/GameData/viper.car viper
./build/viper_reconstruction --inspect-season ../game_install/GameData/ui.res season0.ssn
./build/viper_reconstruction --inspect-stamp ../game_install/GameData/common.res euro6.fnt
./build/viper_reconstruction --inspect-image ../game_install/GameData/common.res Load.stp
./build/viper_reconstruction --inspect-font ../game_install/GameData/common.res euro6.fnt
./build/viper_reconstruction --inspect-terrain ../game_install/GameData/uptown.trk
```

On Windows, run `build\Release\viper_reconstruction.exe` instead of the
Unix executable path. The asset command inventories the extracted directory;
the resource command lists entries from an RST0 `.res` or `.car` bundle. The
tire command prints the fixed-size TIRE record fields without assigning
unknown physics meanings. The car command reports the CARF payload and parses
UPGS using the recovered relative-pointer fixup. None of these commands starts
the original game. `--inspect-season` prints raw 22-DWORD event records,
track names, placement payouts, and placement points from a `SEAS` resource.
`--inspect-stamp` reports the recovered STMP pointer table and projects its
conditional embedded-data fixups for `.fnt` and `.stp` resources.
`--inspect-font` prints the 256 character lookup rows built from a font STMP
resource; the two metric fields keep their raw offsets until their meanings
are resolved. `--inspect-image` decodes STMP indexed-image dimensions, palette
planes, frame rows, and bounded RLE data. It also reports first-frame
mode-4/mode-5 palette-write coverage and raster completeness. For
`status.stp`, it also populates the state-0 observation from `race.res` and
reports the reconstructed source-buffer sizes for both modes.

## Scope and next steps

The current target is a portable, source-only reconstruction, not yet a
playable game. The C files are evidence fragments and are not compiled into the
new target. The executable inventories external assets, lists RST0 bundle
contents, and decodes TIRE, SEAS, and STMP resources including indexed image
frames. The C++ library also models
observed wheel and tire response equations, kinematic ratios, response-vector
assembly and correction, wheel contact and suspension state, the tire curve
helpers, and a rigid-body contact-response/impulse path. Static
analysis now ties the fixed-step physics task to conditional car body updates,
shows that the main entity scene wrappers and physics objects are created from
the same serialized car records, and traces the scene-side pose/wheel update;
the native physics-task record-tag cursor advances are modeled, while the
complete scene serialization order and initial cursor remain unresolved;
the NCAR model captures recovered numeric timing gates and first-entry field
writes without assigning semantic labels to opaque record fields;
the game's full simulation, rendering, input, audio, persistence, replays, and
networking remain unimplemented. The active `0x4020E0` path now preserves
callback, render-context, viewport, and submission order, and the scene-car
update projects its ordered four-wheel transform/resource pass. The next
milestone is determining which registered scene-object classes pass the
`0x45BEB0` virtual gates and integrating the reached callbacks into a complete
race renderer; native DirectDraw execution, many class-specific effects, and
original-game comparison remain open. The registered wheel-resource manager's
`0x4623F0` callback is not reached through this generic frame path because its
`+0x0C` gate reads the nonzero value set by frame setup.
Build with CMake 3.20+ and a C++20 compiler. The full Release target has been
compiled successfully with Visual Studio 18 Build Tools (MSVC 19.51) and CMake;
this confirms the reconstruction utilities build, not game runtime behavior.

The merged state-4 paused-banner pixel batch can be applied through the race-loop
lifecycle helper when the frame submit, shared callback, and surface-unlock
sequence match the observed order. The destination locked-surface span and
native DirectDraw lock lifetime remain caller-owned.
