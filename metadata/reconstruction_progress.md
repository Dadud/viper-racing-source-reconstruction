# Reconstruction progress

- **2026-10-24 continuation:** Added raw generation-word comparison for the
  selected type-0 LapTime resource. Both positive-delta and fallback text paths
  now derive refresh from cached DWORD `0x4EB2B8` versus current generation
  `0x4D3868`, matching `0x403A1C`/`0x403A88`; the existing boolean remains a
  fallback. Release build and `git diff --check` pass; no tests were run.

- **2026-10-24 continuation:** Added the RaceTime resource refresh projection
  for the final-time branch. On full redraw or dashboard-kind change, it now
  compares optional cached/current generation DWORDs at `0x4EB3B8` and
  `0x4D3868`, matching `0x4037F7`-`0x403809`. The raw inputs flow through the
  composed callback; missing words leave the result unresolved. Release build
  and `git diff --check` pass; no tests were run.

- **2026-10-24 continuation:** Added the Position-resource refresh decision
  for full redraws by comparing optional raw generation DWORDs at `0x4EB480`
  and `0x4D3868`, matching `0x4036C1`/`0x4036CE`. The result remains
  unresolved if either word is unavailable. Release build and
  `git diff --check` pass; no tests were run.

- **2026-10-24 continuation:** Extended the selected-record projection with
  DWORDs at ESI `+0x20` and `+0x28` and connected them to the supplemental
  string-table indices. When those bytes and the table are available, the
  `+0x24`/`+0x1C` rows now resolve their record suffixes without caller-supplied
  indices. Release build and `git diff --check` pass; no tests were run.
  See `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Added raw generation-word inputs for Best and
  Last resource refresh. The metric projection now compares cached DWORDs at
  `0x4EB348`/`0x4EB338` with current generation `0x4D3868`, preserving the
  existing match flag as fallback. Release build and `git diff --check` pass;
  no tests were run. See `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Replaced TestTrack's opaque resource-generation
  match inputs with optional raw cached/current DWORDs. The branch now derives
  refresh decisions from the exact comparisons at `0x403D74`, `0x403E1F`, and
  `0x403EDE`; legacy booleans remain fallbacks when raw words are absent.
  Release build and `git diff --check` pass; no tests were run. See
  `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Connected the auxiliary numeric readout's
  centering width to the observed dashboard font's signed ASCII `'0'` advance
  (`+0x204 + 0x30*2`). The race-loop passes its layout font into the callback;
  the prior scalar advance is now only a fallback when no font table is
  available. Release build and `git diff --check` pass; no tests were run.
  See `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Corrected dashboard rotating-gear coordinate
  conversion to match `0x4BD1F8`, which sets x87 truncation before `FISTP` and
  restores the prior control word. Geometry, scanline intersections, and
  sloped-edge pixel coordinates now truncate toward zero instead of rounding
  to nearest; host trigonometry and division remain approximations. Release
  build and `git diff --check` pass; no tests were run. See
  `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Reconstructed the dashboard's
  `0x44D530` color-resource gradient for gear-label palette `0x4EB314`.
  The composer now derives the 256 packed color DWORDs from the observed
  endpoints and lays them out into the two 256-word planes written by
  `0x44D4F0`; the raster stage no longer requires a caller-supplied palette.
  Evidence and precision details are in `metadata/simulation_loop.md`.
  Release build and `git diff --check` pass; no tests were run.

- **2026-10-24 continuation:** Removed the race-loop's second TestTrack,
  Best/Last metric, and auxiliary-readout evaluation pass. These outputs now
  alias the state-0 core callback result, so caches and resource-registration
  bits advance once and the raster text uses the same observations as the
  callback state transition. The callback observation is the single input
  surface for these branches.

- **2026-10-24 continuation:** Connected final-race-time row lookup and
  hundredths formatting to the composed dashboard callback. It now derives the
  `0x403848` string from supplied `DAT_0050869C` rows and the selected ordinal;
  the race-loop composer forwards the same shared-results rows when available.
  Missing row bytes preserve the caller fallback. See
  `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Extended callback-owned text arguments across
  LapTime, TestTrack, supplemental rows, Best/Last metrics, and the auxiliary
  numeric readout. The core also binds the observed locale table into these
  projections and resolves the LapTime/Best/Last language resources before
  formatting. The raster path uses recovered strings when available, leaving
  runtime inputs for unresolved sites. The full Release build and
  `git diff --check` pass; no tests were run. See
  `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Carried resolved dashboard row strings on their
  text-call descriptors. The core callback now resolves Position, RaceTime,
  and Lap labels from the supplied language table and derives both numeric
  `position/count` and `lap/count` arguments from its modeled state. The
  dashboard raster stage consumes these known arguments directly and still
  accepts caller-formatted strings for unresolved sites. The source addresses
  and keys are recorded in `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Added a bounded projection of the selected
  dashboard record's observed DWORD fields relative to the `0x4035E0` ESI
  pointer (`record +0x138`). The core callback now derives Position, Lap,
  LapTime/TestTrack, supplemental, Best/Last, and auxiliary-readout values from
  those bytes when present, preserving explicit inputs independently for any
  truncated field. The raw offsets remain unnamed. Evidence and offsets are
  recorded in `metadata/simulation_loop.md`. The full Release target rebuilds
  successfully; no tests were run.

- **2026-10-24 continuation:** Added a bounded projection for the dashboard's
  global object's virtual `+0x40`/`+0x20` methods. It derives selector-0's
  per-ordinal table byte and selector-specific count/denominator fields, and
  recognizes the false-return vtable stub for selectors 1/3/5. The composed
  callback uses these results when their source bytes are available and falls
  back only for unresolved values. Semantic type names and object construction
  inputs remain open. Evidence and addresses are recorded in
  `metadata/simulation_loop.md`.

- **2026-10-24 continuation:** Propagated the race-loop's optional language and
  locale state into its separate TestTrack projection, keeping labels and
  distance formatting consistent with the core callback. The paused and
  loading text raster stages now resolve `Dashboard:PausedBanner` and
  `Splash:Loading` from that shared language table when no branch-specific
  value was supplied. Evidence: `ev_ca260757f720e3c0ff9c2abc12db97068c2470e44c8709aac9e856d753979e7d`.

- **2026-10-24 continuation:** Threaded an optional parsed language table
  through the race-loop overlay path. It now supplies `Splash:Loading` and
  `Dashboard:PausedBanner` to their text rasterizers when branch-specific
  localized text is absent, and becomes the fallback language source for the
  nested state-0 dashboard callback. The shipped English strings are
  `LOADING...` and `PAUSED`; native resource lookup and device effects remain
  separate. Evidence: `ev_ca260757f720e3c0ff9c2abc12db97068c2470e44c8709aac9e856d753979e7d`.

- **2026-10-24 continuation:** Replaced the main menu's supplied stale-label
  gates with direct comparisons when language-generation data is available:
  `DAT_004D3868` versus each localized string object's cached `+0x08` word.
  Explicit stale flags remain a fallback when those raw values were not
  captured. Native lookup failure/warning effects remain modeled separately.
  Evidence: `ev_1389e249a360954552d9c9b02125e93746c3a870077ef404bdff9ae44f0478a6`.

- **2026-10-24 continuation:** Connected the parsed LANGRES0 table to
  `plan_main_menu_screen`; it can now resolve all eight `Main_Menu:*` keys
  registered by `0x4805C0` and returns a resolution bit mask alongside the
  localized text. The shipped `english.lng` contains all eight keys. Main-menu
  resource-manager side effects and callback execution remain open. Evidence:
  `ev_46296befbc4dd8fa5baf76ce27d21d870878fd89e0817e31afeb7087f4d97e0e`.

- **2026-10-24 continuation:** Extended render-context acquisition to accept
  the raw 27-DWORD `0x458FB0` output descriptor. After lock success, the plan
  now derives the pitch from `+0x10` and locked pixel pointer from `+0x28`
  before calling the `0x449350` context initializer; renderer format, width,
  and height remain their separately observed globals. Release build passes;
  no tests were run. Evidence: `0x44E210`
  (`ev_9370d82a0b15bd920e1a5eb259e692723161968f45b7da03a523b5b0fd1585f7`).

- **2026-10-24 continuation:** Added a paused-banner frame-write compositor
  that merges fallback boxes, the optional STMP resource image, and text in the
  recovered operation order. The race-loop result now exposes the merged list,
  and `apply_scene_paused_banner_frame_raster` validates all destination ranges
  before writing to a caller-supplied buffer, including masked rectangle
  updates. Release build passes; native surface ownership and DirectDraw lock
  handling remain external. No tests were run.

- **2026-10-24 continuation:** Added `scene_dashboard_font_projection` to
  convert the decoded STMP character map into the runtime's 256-entry glyph
  index, advance, and x-offset tables. The race-loop paused-banner input can
  now accept the parsed font directly and uses it for layout and text raster
  projection; incomplete font maps propagate as incomplete output. The parser
  now also reads header height and metric fields, allowing it to derive the
  `0x44DBC0` top metric and `0x44DC00` bottom metric used by layout. Shipped
  `euro12.fnt` reports height 16, top 13, bottom 3, and 222 populated records.
  Evidence: `0x44DA00`
  (`ev_7d05e719008e658a35c6e70c8faeef220798081bf3d6f01e67cd1b65744b9fc2`),
  `0x44D460` (`ev_09f7b129c2801b51a9e11c7d8851fee08efd7a59ecd1b3f35d7fced94dc5e318`),
  `0x44D480` (`ev_5aede7efdadbfe09f814eb67c4b0cd0d26fc5d2ad6e9ff005a08695d786d8d8d`),
  `0x44DBC0` (`ev_cec4a590ed6c06a30ddb4ae060b2ef75db678e018800f7828e65f4c5da32a8e0`),
  and `0x44DC00` (`ev_7e1c5e7f809dd9f3d20fb7162b68127059bbdd3b59da126cc4762ad41de07bec`).

- **2026-10-24 continuation:** Modeled the state-4 style-14 `0x479450`
  fallback-width rule and connected it to the race-loop composer. It invokes
  text measurement, but the zero alignment flags mean non-null text returns
  `max(0x24, optional style-resource width)`; null text returns the optional
  resource width. That width remains an explicit input, so an unavailable
  value marks the projection incomplete. Release build and `git diff --check`
  pass; no tests were run.

- **2026-10-24 continuation:** Composed the paused banner's fixed `load.stp`
  STMP request with `plan_resource_get` and exposed the `0x44CFD0` call in the
  inner operation trace. An explicit lookup plan now chooses resource drawing
  versus fallback boxes; a decoded `StampImage` supplies centered dimensions
  and implies success when no separate runtime lookup result is supplied.
  Added native call-order entries for resource height/width and fallback
  style-width measurements. Release build and `git diff --check` pass; no tests
  were run. Consumer
  evidence: `0x44CFD0` (`ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`)
  and `0x40C790` (`ev_1c065c92998a4cd49e818f980a1d6b5ddaba78b56125ce8b9747963977b4210b`).

- **2026-10-24 continuation:** Added `parse_stamp_image` for the `0x44D0C0`
  STMP frame format: it validates frame-row pointer counts, reads both 256-entry
  palette planes, and decodes bounded RLE rows. The paused-banner rasterizer
  now accepts the decoded image directly. `--inspect-image` confirmed `Load.stp`
  (126x30) and `cross.stp` (16x16) in `common.res` have complete row streams.
  Release build, asset inspection, and `git diff --check` pass; no tests were
  run. Evidence: `ev_3bbd3fcdbe60647b79111eef559e4f597ef123048430898b07f080d4fa525bde`.

- **2026-10-24 continuation:** Connected the paused-banner `0x44D0C0`
  resource-image branch to the shared indexed-RLE raster core. The resource
  frame supplies dimensions, origin, palette planes, and row runs; the banner
  operation supplies destination coordinates/frame and the acquired render
  context supplies clipping and framebuffer addressing. Resource lookup,
  decompression, surface ownership, and presentation remain open. Release build
  and `git diff --check` pass; no tests were run. Evidence:
  `ev_3bbd3fcdbe60647b79111eef559e4f597ef123048430898b07f080d4fa525bde`.

- **2026-10-24 continuation:** Recovered state-4 paused-banner style descriptor
  14 at `0x4DE7A8` from `0x4714E0`, and the descriptor-to-runtime-row copies in
  `0x478EE0`. Added a style projection that fixes its zero flags and `0x24`
  vertical offset while retaining the runtime horizontal offset and resource
  constructor results as inputs; the raster path can now consume it directly.
  Release build and `git diff --check` pass; no tests were run. Evidence:
  `ev_c31bccf8cd1585f16412312883af4a935afd06efb76b2e2dce000f0c50a13fa3` and
  `ev_c0e9ebbece1ee5ea54ee6633c9354485b103ea13ea9c230158836bddca2009e6`.

- **2026-10-24:** Added state-4 paused-banner text rasterization for the
  `0x479580` helper, including line splitting, style offsets, font-height
  spacing/alignment, and ordered `0x44DC70` atlas pixel writes. The race-loop
  composer applies the acquired viewport/base when available. Style/localized
  text/font/atlas inputs remain explicit; Release build and `git diff --check`
  passed; no tests were run.

- **2026-10-24:** Added the two ordered `0x404890` suppression-predicate calls
  to the state-0 `0x4052E0` callback geometry operation trace. The plan now
  exposes the exact result-byte/`0x4EB1E8` gate and records both call sites,
  including calls that skip their following overlay branch. Release build and
  `git diff --check` passed; no tests were run.

- **2026-10-24:** Connected `raster_scene_dashboard_core_callback` to
  `plan_race_loop_shared_overlay`. The compositor can now derive per-call
  renderer mode and framebuffer geometry from successful context acquisition,
  or use the state-0 render context when that branch is active; formatted text,
  font data, and raster contexts remain caller observations. Release build and
  `git diff --check` passed; no tests were run.

- **2026-10-24:** Added `raster_scene_dashboard_core_callback`, which composes
  the state-0 dashboard callback's ordered text calls through existing font
  layout and atlas rasterization. One formatted argument and renderer context
  remain explicit per call; the result preserves callback order, aggregates
  projected pixel writes, and reports incomplete input spans. Surface ownership
  and device submission remain open. Release build and `git diff --check`
  passed; no tests were run.

- **2026-10-24:** Extended the multiplayer status supervisor with the two
  `DAT_00564320` one-time text registrations, their localized keys and atexit
  callbacks, and optional generation-cache refresh gates. Documented the
  `0x49C8A0` thread-ID/lock check and `0x49C7A0` signal/poll/panic cleanup path,
  including its raw 3,000-unit deadline and `0x19` delay argument. Missing
  generation outcomes keep the supervisor projection incomplete. Existing
  evidence is in `metadata/multiplayer_worker.evidence.json` and
  `metadata/multiplayer_interface_followup.evidence.json`; Release build and
  `git diff --check` passed; no tests were run.

- **2026-10-24:** Extended `plan_main_menu_screen` through the full-screen
  `0x472B30` interaction path: centered coordinates, controller allocation and
  its 16-slot registry/panic gate, controller initialization/resource load,
  default-layout auxiliary control, descriptor/default selection setup,
  callback install/result read, cleanup, and shared-context restoration.
  Memory/resource effects and callback execution remain observations. Merged
  the three new REA records into the existing frontend evidence bundle without
  discarding prior evidence. Release build and `git diff --check` passed; no
  tests were run.

- **2026-10-24:** Extended the main-menu model through `0x4805C0`'s stateful
  setup. `plan_main_menu_screen` records all eight one-time localized-label
  registrations, the default-name and six menu-label generation refresh gates,
  and the raw/normalized `0x472B30` result. Localization data, resource effects,
  and activated callback behavior remain observations. Evidence is in
  `metadata/frontend_dispatch.evidence.json`; Release build and diff check
  passed; no tests were run.

- **2026-10-24:** Extended the dual-table planner to `0x456D00`, adding the
  distance-fade OR into the second palette word and mode-2 delegation to
  `0x4575A0`. The final alpha remains an explicit input; incomplete fade
  observations preserve the already-produced table word.
- **2026-10-24:** Added the prelit-object callback `0x456660` with its three
  descriptor modes, shared palette path, `DAT_0050D2DC == 2` transform gate,
  and mode-2 helper route through `0x457460`. Transformed `+0x18/+0x1C` words
  remain observations where x87 and helper effects determine their values.
- **2026-10-24:** Added the dual-table callback `0x4563E0`. The planner now
  records both palette lookups and mode-2's ordered `+0x14` initialization
  followed by replacement from the second table. x87-derived indices remain
  observations; source row bounds and partial writes remain explicit.
- **2026-10-24:** Added primary callback `0x456230`: modes 0–1 use the
  observed palette-index/table inputs and preserve destination `+0x14`; mode
  2 writes opaque white. The planner now covers this callback's mode-specific
  row writes, with x87 palette conversion left explicit.
- **2026-10-24:** Extended the bounded callback model to primary renderer
  callback `0x4569C0`: modes 0–1 select an observed palette index and mask the
  selected table word; mode 2 writes opaque white. All modes share the copied
  row fields and distance-fade alpha path. Palette conversion and exact alpha
  remain observations because of x87 precision.
- **2026-10-24:** Resolved all four panic-forwarder format strings from the
  engine string inventory and added their text to the callback plan. They
  report missing environment mappings for prelit objects across fog and
  environment variants; callback execution timing remains unresolved.
- **2026-10-24:** Mapped four callback-table panic forwarders to `0x411000`,
  preserving each static format-pointer address in a portable plan. Message
  text and indirect-dispatch timing remain unresolved; see `scene_car_update.md`.
- **2026-10-24:** Extended the bounded row-converter model to enabled-path
  callback `0x455F40`. It preserves the native copy, color, zero-alpha, faded-
  alpha write order and signed row bounds. Its distance/fade arithmetic is
  documented from instruction evidence, while final alpha bytes remain
  explicit observations to avoid claiming exact x87 conversion fidelity.
- **2026-10-24:** Modeled the two default row converters selected by `0x455C80`.
  The bounded projection preserves native copy/store order, signed row bounds,
  mode rejection, untouched destination `+0x0C`, and per-row counter updates.
  See `scene_car_update.md` and refreshed `scene_wheel_lifecycle.evidence.json`.
- **2026-10-24:** Extended the wheel renderer path from indexed submission
  through texture binding and shared graphics-state handling. `0x4525F0`
  projects texture-handle cache branches; `0x457AF0` projects changed-state
  helper calls and cache updates. The frame path now follows its `0x100`
  enable toggle through `0x4513A0`/`0x451610`, `0x457A20` invalidation
  sentinels, and `0x455C80` callback-slot selection. Runtime state remains an
  explicit observation; device HRESULTs and callback execution remain outside
  the portable model. Details are in `scene_car_update.md` and evidence in
  `scene_wheel_lifecycle.evidence.json`.

- **2026-10-24:** Added a bounded model of the generic render frame at
  `0x45BEB0`: exact enable-state gate, camera-matrix rewrite/restore,
  graphics-state and viewport call order, optional mode-dependent camera
  update, and per-object `+0x10`/`+0x0C`/`+0x14` virtual dispatch conditions.
  Followed `0x448940` into its 4x4 view-matrix construction, eight-slot
  transform-cache invalidation, device vtable `+0x68` call, and error logger.
  Expanded `0x4487A0` through its `+0x44` viewport call and `+0x30` buffer
  clearing, including the 0x2C viewport record, rect edges, raw flags, and error
  messages. Adjusted-FDIV ratio output remains an explicit observation.
  Connected each wheel-manager indexed-draw record to `0x448ED0` → `0x452700`,
  preserving the device vtable `+0x78` contract and ignored native return.
  Missing object observations and null pointers stop or bound the projection;
  other graphics API effects and virtual return bytes remain external. Evidence
  is retained in `scene_renderer_dispatch.evidence.json`.

- **2026-10-24:** Composed the full `0x45BC20` scene-car dispatcher in
  `plan_scene_car_dispatcher`: profiling bracketing, sixteen local record
  initializers, 12-DWORD shared-state copy and scalar stores, parser call,
  counted notifier callbacks with cache-update gates, and counted 14-DWORD
  car-row copies. Supplied spans expose incomplete observations and the
  possible over-read beyond the sixteen initialized local records. Added the
  new caller/helper evidence to `scene_car_dispatch_correction.evidence.json`.

- **2026-10-24:** Reconstructed the `0x462CF0`/`0x462D00`/`0x462D30`
  callback registry lifecycle and `0x462D90` byte parser. The portable parser
  preserves ordered duplicate-tag callbacks, last-handler cursor results,
  sentinel/unknown-tag termination, signed cursor wrap, and bounded missing
  observations. Recorded the scene-context tag-1/tag-2 handlers separately
  from the effects-context tag-0 handler; handler bodies remain explicit
  external effects.

- **2026-10-24:** Decoded the fixed return sizes of the three registered
  scene byte handlers from their shipped instruction ranges: tag 2 consumes
  `0x24`, tag 1 consumes `0x18`, and tag 0 consumes `0x1C`. The parser now
  applies these recovered lengths automatically, leaving only unknown handler
  return values to caller observations.

- **2026-10-24:** Projected tag 0's effect deduplication and pool selection in
  `plan_scene_car_effect_tag_zero_handler`: full 32-slot key scan, duplicate
  suppression, wrapped cursor update, generator dispatch, and bounded reports
  for incomplete/null slots. Added a bounded model of `0x468AF0` that includes
  velocity clamping, control points, random cloud particles, and lifetime
  fields, with missing random observations and x87 precision clearly reported.
  Evidence and implementation notes are recorded in
  `scene_car_dispatch_correction.evidence.json` and `scene_car_update.md`.

- **2026-10-24:** Added the tag 1 effect-pool callback model for `0x468450`:
  context gating, 32-slot cursor progression, strict 0.5-second slot retention,
  velocity scaling and three ordered random perturbations. Slot timestamps and
  random draws remain explicit observations; the original x87 rounding is
  approximate. Evidence: `ev_260c6486a94a1e7634d421d893fa63f67d2bea3b4bee5390d42c336909377fa6`,
  `ev_f826a09a5e60377e67248315096dc8878299c6c63f8ce1a1dc6641a129eb912f`,
  and `ev_878fa81da8d63d96b130a5a8c9968c52402b863cc2430660810a4bc2358dfeb2`.

- **2026-10-24:** Added the tag 2 callback model for `0x468290`: one-of-sixteen
  context cursor and its pre-timer initializer call, strict half-second
  emission gate, scale clamp, and sixteen three-dimensional jitter points.
  Added a separate `0x461AC0` child-field projection; its final renderer-side
  `0x461650` registration remains external. Partial random observations expose
  component write masks. Evidence: `ev_c2cac3248c1a63da85831ae40530abbb100df6ab94e47f402974dc32797378dd`,
  `ev_9d509939ffb7af7f87606397c1ed9e1e7eb001c378380eff367128b96553b858`,
  `ev_62fbb1add8c5be568ef2f3f807af41519711a3cbbc782f0c1db3f13418508838`,
  and `ev_0adb01115cbb64610af2ba80c7adc608b25c8da6c1dbf6e36d7fd36a5b61a9da`.

- **2026-10-24:** Projected `0x461650`'s four-vertex atlas quad: frame bounds,
  half-extent formula, UV gutters/mode offset, vertex stride, and its flags-7
  forwarding call. Extended through `0x44F9C0`'s CPU queue path with bounded
  centered short-vertex rebasing, ordered resource-binding reuse/allocation,
  resource load/release effects, node field writes and stale-node cleanup.
  Allocation and load results remain observations; device/backend rendering
  remains external. Evidence: `ev_2ffba83fd83f8daaf91a2409f11f5d18a0d3696efd9e7ebe3f8e95a4e7b42721`,
  `ev_bd4f35ac48c918544966d4212753f4f620c06c42d4a8180b73d1b0bee859b852`,
  `ev_b6b2346770ffedb63c33c61d359265f9344b7c36431aaee691df3a0bdef5b9a6`,
  and `ev_3f709d251610dddb4f5078f98a2c8c5ad682d359afdab0bd5e97c043a3d75ccd`.

- **2026-10-23:** Added a bounded STMP payload parser and
  `--inspect-stamp`, using the `0x44CFD0` layout: factor-counted pointer table
  at `0x81C`, embedded data base, and optional relocation. Added font character
  map projection from `0x44DA00` and `--inspect-font`, preserving the raw metric
  fields. Verified against shipped `common.res` entries `euro6.fnt` (1,776
  pointers) and `cross.stp` (16 pointers); every projected pointer target falls
  inside its payload. Release build and `git diff --check` pass; no tests were
  run. Evidence: `ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`,
  `ev_4d95bea32a566762f91f581a174b2b9c3919d92416e39659afdfe44913a3bbee`,
  `ev_9c87753a7b96dd2eab7167774892dd8c6877748cc1df5c8cabe67c4f1dd8917f`,
  and `ev_257c3693e424fe89f3867434404a21c4e59f14182207e0decf39e99206b74e4d`.

- **2026-10-23:** Added an ordered projection of dashboard initialization at
  `0x4031F0`: resource/font registration, status-font measurement and context
  setup, colored-resource initialization, and redraw-cache sentinels, retaining
  exact call/store order and write widths. Composed it into `plan_race_setup` at
  the call site. Handle values remain runtime observations. Evidence:
  `ev_f63a1c54ee77e2b07363968872ef6738bfdb9961d2540c41da139ef5d11fa6e8`.

- **2026-10-23:** Connected the active-frame surface unlock at `0x447CE0` and
  gated outer-loop flip at `0x447C50` to the race-loop projection. Flip
  observations model still-drawing retries, success, non-lost errors, and
  lost-surface recovery; DirectDraw results remain explicit inputs. Evidence
  and helper details are documented in `metadata/simulation_loop.md`.

- **2026-10-23:** Joined the optional `test_prompt` branch, its fixed text
  calls, input wait, and the nested `0x41F4C0` three-success frame loop into a
  resumable workflow. Failed frame acquisition stays in the tight retry stage.
  Release build and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added ordered alternate-race scoreboard text calls and wired
  them to acquired loop frames; modeled the three fixed optional-prompt draws
  and the `0x41F4C0` three-success acquisition loop. Evidence includes
  `0x41AB00` `ev_1884fc31ca89ec1ab9fbe4a69e2547f155f2f0fc9b380c046248017a8885ac64`
  and `0x41F4C0` `ev_e58629ca1f2baa1264f04706137ef3037e60a8a407099f5a7effce7a165b623e`.
  Release build and `git diff --check` passed; no tests were run.

- **2026-10-23:** Connected detailed `0x44E210` acquisition observations to
  shared race-frame planning. It now only projects acquisition when the frame
  reaches `0x447C90`, then uses acquisition success to gate submission and
  context callbacks. Release build and `git diff --check` passed; no tests were run.

- **2026-10-23:** Composed the `0x44E210` render-context acquisition paths,
  retaining per-attempt lock retries, lock error reporting, ordered primary
  and backbuffer recovery, null-page failure, and successful context setup.
  Release build and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added ordered State-4 `Splash:Loading` operations for optional
  registration, generation refresh, and resource draw. Release build succeeded
  and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added the ordered State-4 results-heading operation projection:
  lazy resource registrations, centered `PressESC` resource draw, and the
  results/prompt refresh and text-draw sequence. Implicit `0x44DC70` payloads
  remain global renderer state. Evidence `ev_a058b9f23799a14959f9a4c014c5676ed3461968e94bbb8ac13c1c3cb3687e65`.
  Release build succeeded and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added a state-4 results-row raster adapter that composes the
  reconstructed row draw calls with existing `0x44DC70` font layout and
  `0x44D0C0` RLE atlas rasterization. It requires supplied loaded-font data and
  per-call renderer/palette observations and preserves row-call order. Added
  an atomic apply wrapper that refuses incomplete atlas plans. Release
  build succeeded and `git diff --check` passed; no tests were run.

- **2026-10-23:** Extended the state-4 paused-banner layout with ordered
  fallback box/text and resource/text/release operations; fallback boxes now
  produce clipped, renderer-mode-specific pixel writes through the shared
  rectangle path. Release build succeeded and `git diff --check` passed; no
  tests were run.

- **2026-10-23:** Added transactional application of each state-2 wheel bar's
  fill, circle outline, and indicator pixels. The helper validates every
  destination before writing and preserves draw order; the formatted label
  remains a separate text operation. Release build succeeded and
  `git diff --check` passed; no tests were run.

- **2026-10-23:** Added a transactional state-3 raster apply operation that
  validates background and glyph destinations together, then applies them in
  draw order. Also recorded direct bytes for the `0x44C950` circle helper
  (`ev_9bd15fa02ca8378379152537bc3764b404783eed4cff656f2bd6f3203dded73c`).
  Release build succeeded and `git diff --check` passed; no tests were run.

- **2026-10-23:** Connected state-3 text calls to the existing extracted 8x8
  glyph rasterizer. The combined raster plan now emits background-fill writes
  followed by formatted LOD, instruction, FOG, and LIGHT glyph writes, using
  explicit runtime colors and render-context inputs.

- **2026-10-23:** Connected state-2 wheel-bar fill and indicator geometry to
  the reusable clipped rectangle pixel-write path. The indicator uses the
  recovered `0x44C260` major-axis stepping and float slope accumulation;
  clipping and per-mode encoding are inherited from `0x44A1B0`/`0x449E90`.
  Replaced the earlier frame interpretation with the `0x44C950` eight-way
  midpoint-circle outline and projected its symmetric pixel calls. Rebuilt Release
  successfully and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added reusable clipped `0x44A1B0` per-pixel fill projection
  and a validated framebuffer apply helper. Connected state 3's background
  rectangle to this path, including renderer-mode color encoding and mode-1
  high-nibble preservation. DirectDraw ownership/locking remain external.
  Rebuilt Release successfully and `git diff --check` passed; no tests were run.

- **2026-10-23:** Added `scene_graphics_overlay_render_plan` for state 3's
  background fill and four ordered text calls. The recovered instruction
  listing corrected a stale note: the helper is `0x44A1B0` (fill rectangle),
  not a screen-setup function at `0x4A1B0`. Backend rendering remains an
  external effect. Rebuilt Release successfully and `git diff --check` passed;
  no tests were run.

- **2026-10-23:** Extended the state-2 `0x4236F0` wheel-bar projection with
  ordered backend wrapper operations: conditional fill, frame, indicator line,
  and formatted text. The recovered listing confirms call order and arguments;
  the text record retains both its format source and computed numeric value.
  Actual rasterization remains outside the model. Evidence is listed in
  `metadata/simulation_loop.md` under “State-2 physics overlay callback.”
- **2026-10-23:** Rebuilt Release successfully and `git diff --check` passed.
  No tests were run.

- **2026-10-23:** Added the DirectDraw surface lock result projection for
  `0x458FB0`: success, surface-lost error, immediate retry on
  `DDERR_WASSTILLDRAWING`, and generic HRESULT reporting. Added the
  `0x44EBD0`/`0x459190` failed-lock recovery projection, including ordered
  restores of the primary surface and backbuffer and per-surface error
  reporting. DirectDraw calls remain supplied runtime outcomes. Evidence:
  `0x458FB0` (`ev_1130fc8f7ab56d05acea4b7b3732f09afa5b858f4eecc179b95db527144df3ab`),
  `0x44EBD0` (`ev_57a981467c870406fa93e4026960d63cd33667ef1adc17eec0f7a1e7a07ab430`),
  and `0x459190` (`ev_2359f304072f9863791601c4510f9c2023c0e62ce0b4deee24717070914362c8`).
- **2026-10-23:** Rebuilt Release successfully and `git diff --check` passed.
  No tests were run.

Target: Viper Racing installer version `1.00.000`; runtime module
`GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.

- **2026-10-23:** Reconstructed the career pre-race entrant estimate and order
  projection from `0x4AA5E0`: seven AI estimates combine the race baseline,
  profile times, and observed RNG samples; the player uses the race setup time;
  estimates fall back to 300 seconds when an AI profile time is nonpositive.
  Pairwise swaps produce the order consumed by career result writes. Profile
  selection and RNG state remain inputs. Evidence: `0x4AA5E0`
  (`ev_d306de34923170ef0eb22248da3f0dcdf2e921f857330c74a56e38c66a8e3a3`),
  `0x4AB0B0` (`ev_e8d526d4774e8b206df3b2a18a4a7f0a584473331505a83a1654e2265c2c0c1e`),
  and `0x4193F0` (`ev_ac4ff3b289c237c75c0e1fd826f40009860951ca0f75f62d7779fda1dab6ee56`).
- **2026-10-23:** Traced the career event column to caller state: `0x4AA490`
  pushes the `0x5649F0` cursor into `0x4AA5E0`, which writes it as each result
  column. Added `advance_career_event_from_input` for the two-predicate guarded
  advance (`cursor + 1 < event_count`) and its event-loop increment and ranking
  refresh. Evidence: `0x4AA490`
  (`ev_e1ccadbdca1e319b8454239e64b0a02c992e984de76c32f3aff8cb4149e493de`) and
  `0x4AA5E0` (`ev_2dd12de899fae8f45fcd758bdfefe7e4aefaa963ec97ad3fb5db51df3954e048`).
- **2026-10-23:** Rebuilt Release successfully and `git diff --check` passed.
  No tests were run.
- **2026-10-23:** Rechecked the season loader `0x4ADB00`: its validation pass
  resolves only the eight header money IDs and eight event money IDs at
  `0x0F + event * 0x16`; this provides no added semantics for event DWORDs 2–5.
  Corrected the priority note to distinguish these SEAS record reads from
  `0x4AEC30`'s independent career-payload reads at `0x10`, `0x14`, and `0x124`.
  Evidence: `0x4ADB00` (`ev_214fa267722f77484ead3a5c234bb1f8898aebf3c2196f9372e1434f8bf0960c`),
  `0x4AEC30` (`ev_85416a2e48e436c3802b3bf009063c157b475c29c07b1c118a560118d85fbe61`),
  and `0x4AA3E0` (`ev_8fee4d35e58b1d2396a9904e6a5fdb8a7609a72be43dc9e0f7dbd0f7bf73ff31`).
- **2026-10-23:** Matched `apply_career_event_ranking` to `0x4AC290` more
  closely. The shipped helper scans all eight entrant indices and retains the
  last occurrence of competitor 7; the portable projection now accepts
  duplicate in-range indices and reproduces that selection. It still bounds
  bad indices and a missing competitor 7 to avoid the binary's out-of-array
  read. Evidence: `0x4AC290`
  (`ev_ab4a871d9399f6f8799d227effe39f1fd6ef848a8f273759fe56f668304cc985`).
- **2026-10-23:** Reconstructed the `0x4228C0` camera table loader and connected
  its seven-column records to the existing camera-mode projection. The loader
  maps `fixed`, `pan`, `pan_zoom`, and `chase` to types 0–3, converts the six
  numeric columns through supplied `_atof` observations, and preserves the
  previous type word when an unknown type calls the panic helper. Mode 10's
  nearest-target selector and mode 12's index-zero selector are now documented
  alongside the parser. Evidence: `0x4228C0`
  (`ev_b906c2365d4e529592f7c40ba86ebcf4ace8ed85ee259f884d5eecb2f118b3e8`),
  `0x421120` (`ev_1ae43339ba97ab8e558c634a0d88a8c54eb714fc8881d6681ae7d05fdfa9711c`),
  and `0x4203E0` (`ev_fb2afb0c499dab4157b228aec5abf5e061fe5ed0bed86e7e3f92cf7c48a87ff1`).
- **2026-10-23:** Rebuilt Release successfully and `git diff --check` passed.
  No tests were run.
- **2026-10-23:** Resolved the camera modes' near-vertical fallback source in
  `0x421120`. The common preamble copies 12 floats from the selected car at
  `+0x38`; the fallback direction uses transform elements 6 and 8. Replaced
  the free-standing fallback vector input with that source transform in the
  mode-10/12 projection. Evidence: `0x421120`
  (`ev_3314c631f34b460398f9e0176a2161e3670cc8c9ee6fcbed396c68f19426b8dc`).
- **2026-10-23:** Added the `0x422580` physics-task object factory projection
  for all nine recognized FourCC tags, allocation sizes, and constructor
  addresses. Allocation results, constructor return values, and panic-helper
  continuation remain inputs. Evidence: `0x422580`
  (`ev_16e9359e1fff260aa38cf33c56d1c882043ea11712c8fa31761a929ff9f41f37`).
- **2026-10-23:** Reconfigured and rebuilt Release successfully after adding
  the factory source; `git diff --check` passed. No tests were run.
- **2026-10-23:** Rebuilt Release successfully and `git diff --check` passed.
  No tests were run.
- **2026-10-22:** Extended the `0x465800` scene-car model with an ordered
  projection of special transition and per-bit `0x447E50` calls and their
  `0x447EE0` refreshes. The grid rectangle uses the observed `0x4C940C` value
  (`0.0625`); raw bit and resource semantics remain unresolved. Evidence:
  `ev_9bc59c0c0669289e8d06530e19a0d8bd4088c0dab36f2066e9d10870d81aacc6`.
  No build or tests were run.
- **2026-10-22:** Modeled the `0x462600` per-wheel auxiliary update: initialization,
  allocator outcome, timer/distance release gate, cross-product normalization
  gate, and four resource position triplets. Pool allocation and lifecycle
  stores are modeled when pool state is supplied; x87/Pentium-FDIV precision
  remains approximate. Evidence:
  `ev_16ef11a026ee34982b250f91f5e205dce42af00d23f03c529c4bc366e81cdb95`.
  No build or tests were run.
- **2026-10-22:** Composed the `0x465800` per-wheel outer gates and the
  `0x462940`/`0x462960` release siblings with the auxiliary state. Allocation
  sets resource `+0x30` to 1; each observed release path sets it to 2 and clears
  the pointer. The world, selector, suppression, and record-bit inputs remain
  explicit observations. Evidence: `ev_9bc59c0c0669289e8d06530e19a0d8bd4088c0dab36f2066e9d10870d81aacc6`.
  No build or tests were run.
- **2026-10-22:** Reconstructed the `0x462590` resource allocator's counted
  circular scan, `0x38` slot stride, `+0x30 != 1` availability test, lifecycle
  and size stores, cursor advancement, and null-on-full behavior. The
  `0x462600` model can now use an explicit mutable slot pool, including its
  four position triplets, instead of a supplied allocation result. Evidence:
  `ev_713bb6ee2b9dcfac640fa7261907bbb2f18965a88d66912fb5dc113dffcf3e39`.
  No build or tests were run.
- **2026-10-22:** Promoted the recovered common VC98 FDIV finite-normal
  approximation into the shared `viper_legacy_fdiv` library and applied it to
  the `0x462600` normalized cross-product divisions using the explicit
  `0x4E9754` compatibility input. Exceptional/subnormal behavior and exact
  x87 rounding remain open. No build or tests were run.
- **2026-10-22:** Added the `0x4622F0` pool-manager initializer, `0x462380`
  reset behavior, and `0x4623B0` ordered cleanup plan: initialize slot lifecycle
  fields and texture/cursor state, preserve live state-1 entries on reset, then
  project texture release, array free, and base destruction. Slot allocation,
  texture lookup, and external cleanup effects remain observations. Evidence:
  `ev_b70eb7f4fe82144e87ef91286f9c5eb5810bf511a36c9d82a0f6cfbb7529a13f`,
  `ev_55c21a5ed04271fef9a5a9d8cae98e2300ab109199cee2c281ac0240308ea288`,
  `ev_b36453554f2c9a86f047e709aaa2623c5a68401b5c5b3941a3eaebf19a2aa628`.
  No build or tests were run.
- **2026-10-22:** Added an ordered draw projection for the manager block at
  `0x4623F0..0x462574`: reset gate, render-state helper calls, texture bind,
  nonzero-slot indexed draws, quad vertex/UV/index words, and cleanup calls.
  The block is not a Ghidra function entry; one vertex word remains an explicit
  stack observation and graphics backend effects remain unexecuted. Wrapper
  evidence: `ev_fd752a426d357596f80fec5d14eecf72530238d9cc9ba11a13bc177b359cdd0d`,
  `ev_3bc4747a9027358eddde04b53dcc349045ec7c294a3eb68c08995f1f37fa069f`.
  No build or tests were run.

Latest wheel-loop increment: added the direct contact gate/point stores, base
suspension-response scalar, surface-feature adjustment, and response
finalization from `0x442530`, plus the conditional normal collision response
and impulse preparation. Added the TIRE-backed trigonometric evaluator at
`0x435780` and signed clamp/normalizer at `0x4356D0`, including runtime
coefficient sourcing at `0x435BF0`. Added the following wheel filter-state
updates at `+0x170/+0x174/+0x180/+0x11C`, connected to the trigonometric
response. Modeled the post-response `0x443F60` angle/basis updates at
`+0xB0..+0x118`. Contact feature `+0x19C` is reset before the optional query;
contact activation is conditional on nonnegative projected depth. Details and
limits are in
`metadata/vehicle_setup.md`.

Latest NCAR precision increment: moved the `0x439520` derivative coefficients,
sample products, magnitude gate, normalization, and ramp division to host
`long double` intermediates. This better preserves the target's x87 precision
on hosts where `long double` is wider than `double`. Added an explicit
`0x4E9754` compatibility input and the CRT's common finite-normal significand
gate plus paired operand scaling; exceptional cases and exact x87 rounding
remain open. A targeted Ghidra pass confirmed the complete `0x439520..0x439A07`
body and traced its compatibility calls through `0x4BC9FC`, `0x4BC8E9`, and
`0x4BC320`; the latter inspects a significand table and conditionally scales
operands under a modified x87 control word. Confirmed `0x4BC260` stores the
`__ms_p5_test_fdiv` result from `0x4BE370` into `0x4E9754`; the CPU/OS probe
result remains environment-dependent. Added `viper_legacy_fdiv` to project the
ordered dynamic API lookups, affinity-mask loop, early-return conditions, and
restoration call from explicit runtime observations. Evidence and residuals are in
`metadata/ncar_sample_dynamics.md`. The history derivative now also preserves
the binary's two single-precision time-delta temporaries at `0x4395E3` and
`0x4395E7` before computing its three component equations.

Latest wheel-loop increment: reconstructed the direct `+0xAC` angle
accumulator update at `0x442530`, including its float store and one-step wrap
branches using the target's signed/unsigned bit comparisons. The scale and
wrap constants remain inputs; this helper does not imply full wheel-state
coverage. It now also models the immediately preceding `+0xA8` product and
conditional distance-threshold write to `+0x20`, driven by the byte at
`0x50878C` returned by `0x425870`. Also modeled the later `abs(+0x124) < 16`
blend block, including its signed response-local adjustment and `+0x124`
update, with constants supplied by the caller. Added the subsequent
positive-excess update to the response weight at local `+0x2C`, preserving
its signed-bit gate and upper cap. Connected the wheel point construction and
`0x437BA0` body affine transform to the terrain query and existing contact
state update, including the intermediate float stores. The composed path now
also uses the transformed `+0xC8` point as the contact-depth projection origin,
matching the direct subtraction in the wheel caller. Added
`advance_wheel_update_pipeline` to carry the accepted contact depth, feature,
activation, and wheel direction through suspension response and force-vector
finalization. It also writes the post-finalization `+0x168` reset and applies
the contact-feature profile dispatch at `0x0044302F..0x0044324B`. Tightened
the following normal-impulse helper to consume the
actual `+0x28` threshold response and reproduce the caller's raw signed-bit
gate; its high-response hook now uses the observed `0x50878C` global byte.
Added the subsequent persistent `+0xA4` and `+0x10C` tire-response accumulator
updates, including the raw-bit sign selection and observed intermediate store.
Extended that stage to the contact-gated divisions that write wheel `+0xEC` and
`+0xF0`, with the compatibility-FDIV path surfaced as an explicit condition.
Added the following tire-tail state writes: raw scaled response at `+0xF4`,
projection-corrected response at `+0x100`, and the second-basis scalar
projection into local `+0x7C`. Added the routine-exit update to wheel `+0x124`
and `+0x128`, including the conditional animation flag `0x40` write.
Added `viper_wheel_body_coupling` to route the corrected wheel tire vector at
`+0xC8` through the existing NCAR `0x43E6F0` accumulator model. Added the
second force application with its three ordered scale factors and weighted
wheel/body application point, and composed both accumulator calls in their
observed order against one body state. Reconstructed the four-wheel
`0x443F10` timer update used by `0x4323E0` and the trailing `0x440440`
vehicle-level update, which advances two animation channels under the vehicle's
`+0xBFC` block.

Latest dashboard increment: added the state-0 gear-display resource and mode
gates, mode-specific drawing calls, and the rotating pointer triangle/edge
coordinates in `scene_dashboard_gear_display_plan`. The filled triangle's
horizontal spans now follow the recovered edge-crossing and half-open scanline
loop. The two outline edges now produce ordered pixel-dispatch coordinates
using the recovered major-axis stepping and endpoint-exclusive loop. Edge
pixels now pass through the shared viewport gate and raw-tag pixel conversion,
yielding framebuffer offset/address/value projections. Triangle fill spans now
also project mode-specific writes; mode 1 preserves the old pixel's upper
nibble. Host trig/division/rounding approximate the target x87/Pentium FDIV
path; the state-0 full-surface redraw fill now has a clipped rectangle and
backend-value projection. The background bitmap blit now follows the format
dispatch and destination clipping, with adjusted source/destination address
projections. Actual surface/device mutation and original-runtime comparison
remain outstanding. See
`metadata/simulation_loop.md` for addresses and evidence IDs.

Latest bitmap increment: added `scene_dashboard_bitmap_pixel_conversion`, a
value-level projection for each supported background blit conversion. It
applies the word masks/shifts and mode-5 component packing recovered from
`0x44A940`, `0x44AC60`, `0x44ADC0`, `0x44B210`, and `0x44B250`.
`scene_dashboard_background_bitmap_pixel_writes` now walks the clipped
rectangle over supplied source bytes and returns converted destination values
and addresses, rejecting incomplete input without partial results.
`apply_scene_dashboard_background_bitmap_blit` can apply the complete validated
transfer to a caller-supplied linear destination buffer. The surrounding
`0x415090`/`0x4150A0` calls are `ret` stubs in the shipped assembly; using the
runtime surface pointer and integrating other renderer paths remain open.

Latest dashboard increment: modeled the state copy and two geometry branches
in the raw `0x4052E0` callback. `scene_dashboard_callback_geometry_plan`
derives the rectangle arguments from the raw surface dimensions and supplied
resource metrics, retains the original helper argument order and batching
calls, and now includes the two suppression-gated line rectangles and their
conditional resource refresh calls. It also preserves the two text/font metric
helper calls and raw pointer values. Exact resource measurements still come
from runtime observations.

Dashboard follow-up: added `scene_dashboard_measure_text_width` for the
`0x44DB10` helper. With a bounded NUL-terminated string and the font's 256
signed advances at `+0x204`, it sums advances using unsigned byte indices; a
null font pointer follows the exact 8-pixels-per-character fallback. The two
geometry text sites can now derive their widths from these inputs while
preserving prior observed widths when data is incomplete or omitted. The
`0x44DC50` combined font extent remains an input. Evidence:
`ev_91922500710b23f18621ab91f0c33e44f5e07487cbd09559fe30d08f945f031f`,
`ev_a43fa8b01e191080db9a371ae31bdca6cbd255859bb5e51dfc1cf76f82043b0e`.

Font-extent follow-up: decomposed `0x44DC50` through `0x44DC00`, `0x44DBC0`,
`0x44D480`, and `0x44D770`. The geometry model now returns the fallback 8 for
a null font object; for a nonnull font it returns the nested payload's `+0x04`
DWORD, with zero for a null nested payload. Bounded object reads remain explicit
inputs, and incomplete observations retain the prior measured extent.
Evidence: `ev_947d49214fc6bddd790a2e03d3ee10833a4d09b52b117f2819183691de9d95ee`,
`ev_bbe7ca7f77770f08e51c0607b7ebb9f941637cea3390180e164640571192f23d`,
`ev_152cd44e150e3593386b10501cc0c8a33ba25538747adcf51456fb9bfc998666`,
`ev_525ccb9c1707aae6df6092ae6a6cdea85647cf0e76015f797f00c0549009e44f`.

Latest renderer increment: added `apply_scene_text_pixel_writes` for state-5
glyph projections. It validates every signed framebuffer offset and 16/32-bit
write width before applying any little-endian pixel, so an undersized buffer
is left untouched. Runtime surface acquisition and backend submission remain
open.

## Completed in this increment

- Modeled the locale initializer at `0x418500`: short-date-first-byte table
  selection, decimal marker, two locale bytes, and all 13 raw preset DWORDs.
  The dashboard's `+0x0C` and `+0x1C` multipliers now have a documented
  initializer source; the zero-valued `+0x08` and `+0x18` pointer fields stay
  raw. Traced the target VS 1998 formatter's null `%s` fallback to `"(null)"`
  and added `scene_dashboard_crt_string_argument`. Added locale binders that
  feed the recovered marker and scalar bits into the LapTime, TestTrack,
  supplemental-time, metric, and auxiliary-readout observations, including the
  formatter-produced `"(null)"` text for the zero pointer fields used by the
  TestTrack and metric `%s` paths. The binder sets the TestTrack distance
  suffix, both metric string arguments, and the auxiliary heading string from
  those null fields. Evidence is recorded in `metadata/simulation_loop.md`.
- Modeled the dynamic 16-entry string table written by `0x45DEB0` and read by
  `0x45DFB0`, including its `0x170` stride, `+0x05`/`+0x12` strings, raw mode
  and `+0x134` field, and exclusive entry count. Supplemental dashboard rows
  can now resolve the record's `+0x20`/`+0x28` indices through that table.
  Traced constructor string sources to offsets `+0x04` and `+0x11` in the
  200-byte car record; their runtime contents remain inputs. Modeled
  `0x49C5C0`'s global-interface and signed-bound gates,
  virtual `+0x84` dispatch condition, and returned low-byte test; only the
  indirect method result remains an input. Confirmed the same indexed
  callback is used by the multiplayer setup path to gate per-record handling.
- **2026-10-23:** Re-traced the dashboard string-table initializer, setter, accessor, and no-op helper. Corrected the accessor stride to `0x170` bytes (`0x5C` DWORDs), added NUL-aware native copy lengths and explicit overlap/entry-overrun flags for the setter's unbounded `strcpy` writes, and saved the REA evidence bundle at `metadata/car_string_table.evidence.json`. No build or tests were run.
- **2026-10-23:** Added `make_scene_dashboard_string_table_set_observation`, which safely derives the `0x45DEB0` setter inputs from the runtime car record (`+0x194`) and configured-car row (`+0x00`, `+0x04`, `+0x11`), including the constructor's `this+0x3C` stored pointer. It rejects spans missing the index, row, or either NUL terminator. No build or tests were run.
- **2026-10-23:** Re-traced `0x45EC20` and added `plan_car_file_model_name`/`apply_car_file_model_name` for its copy-to-260-byte-local, last-dot truncation, last-backslash basename, and copy to runtime `+0x198`. The composed car-record writer now includes this name. Paths that exceed the local buffer or remaining `0x4C` runtime bytes are reported incomplete. Fresh loader evidence is in `metadata/car_file_loader.evidence.json`; `0x45CAE0`'s instruction evidence confirms the table index at `+0x194` is the configured-car ordinal.
- **2026-10-23:** Traced `0x45B6B0` → `0x45C630` → `0x45CAB0` → `0x45CAE0` and added `make_single_race_car_track_line_observation`. It joins a configured entrant's marker/car stem to placement inputs using the caller's current global car ordinal, matching the shared 0xC8 row layout. Focused evidence is in `metadata/car_runtime_config.evidence.json`; actual global cursor and live caller handoff remain observations.
- **2026-10-23:** Added a bounded extractor for the 35 raw configuration DWORDs at configured-car row `+0x34..+0xBC`, the exact input pointer passed from `0x45CAE0` through `0x45EC20` to `0x45E4B0`. This lets real 0xC8 setup rows initialize the CARF mapping observation without manual word decoding. No build or tests were run.

- Recovered the career save-file read gate in `0x4AA490`/`0x4ABE20`: slot
  path construction, 16-byte header validation, fixed `0x674`-byte payload,
  XOR-`0xA4` decode, byte-sum checksum, panic/log branches, and opened-handle
  cleanup. Added `viper_frontend_model`'s `career_file.hpp/.cpp` implementation
  and documented the evidence in `frontend_dispatch.md`. Followed the
  successful-load flow into the six-control career screen, summary/progression
  loop, and testing route through pre-race setup. Detailed simulation result
  effects, event meanings, and persistence writes are still unresolved. Added
  `CareerRecentResults` and `append_career_recent_result` for the exact
  eight-entry fill-or-shift behavior in `0x4AA410`, preserving raw value
  semantics. Traced Save through `0x4ABB90` to `0x4ABF10`; added
  `make_career_file_save_image` to produce the exact little-endian header,
  decoded-byte checksum, and XOR-encoded payload. Directory creation and
  actual file operation outcomes remain observation inputs. Traced chooser
  Create and Delete callbacks: added `make_new_career_payload` for the cleared
  slot state, NUL-terminated player name, raw settings, initialized flag, and
  eight seeded dwords; documented the confirmation-gated `DeleteFileA` path.
  Added `finish_career_stage` for the observed progression-completion suffix:
  numeric history result, reward accumulation, table clear, event-counter reset,
  and conditional history-row advance/seed effects. Added
  `apply_career_event_ranking` for the slot-7 placement payout and per-event
  points/placement table writes from caller-supplied ranking observations.
  Added `rank_career_progression_rows` for the observed eight-row score order,
  including the VS 1998 shortsort's deterministic but unstable equal-score
  behavior. Traced the career Testing selector output (track string and
  Reversed byte), the two `0x401EA0` mode choices in the track runner, and the
  post-run `season_<index>.ssn` reload. The season loader's header and per-row
  string-resource checks are documented; record parsing and race-result writes
  remain open.
  The remaining career gaps are progression outcomes, track-test effects, and
  runtime file-system results.

- Added `viper_race_setup_model` to preserve `0x401EA0`'s ordered outer calls,
  the loop-entry choice, optional camera-setting lookup, post-cleanup callback
  branch, and result-copy point. Its output projection captures the three
  observed source-derived row words, including the positive-field gate on the
  associated value. The two mode checks are modeled separately; callees and
  field semantics remain open.
- Traced the alternate `0x41AB00` path and its `0x41B020` mode byte. Added
  `viper_alternate_race_mode` for the cumulative mode-3 observation cutoff,
  500 ms wait branch, ESC/`1` exit effects, and the optional `test_prompt`
  context/input wait sequence. Also recovered the state-1-to-state-3 callback
  gate and shared type-1/type-3 vtable readiness/reset methods. The lap index
  window and early-stop lookup behavior are now documented and modeled. The
  factory selector's source, object-type semantics, text field meanings, state
  semantics, prompt rendering, and three-frame helper remain open. The direct
  callers of `0x45C2C0` were inspected for writes to its `+0x18` selector; none
  were observed, but an indirect or external source is still unresolved.
- Followed the NCAR event payload through `0x4393B0`, `0x49E210`, and
  `0x49DF60`: documented the packed 24-byte intermediate, expanded 62-byte
  record offsets and conversions in `metadata/car_update_branches.md`, and
  extended `NcarSampleRecord` to expose every currently established record
  field while preserving its raw bytes. Added producer and consumer models
  `encode_ncar_packed_payload24` and `decode_ncar_packed_payload24`, plus the
  quaternion rotation quantizer. The encoder reports scratch bytes the original
  leaves unassigned; the decoder preserves destination fields the binary does
  not touch. Field names and gameplay meanings remain unresolved; exact x87
  rounding and Pentium FDIV behavior remain approximate.
- Resolved the NCAR timed-FIFO delay source: `0x49C590` reads a global set by
  `0x49C5A0`; the observed chat latency handler at `0x4A3470` accepts parsed
  values in `[0,1000)` and stores them without scaling. Added
  `TimedEventLatency` and `apply_timed_event_latency_command` to the portable
  queue model. The offset is added directly to the queue's clock-domain value.
- Recovered the exact `0x49E1D0` low-word tick expansion and the
  `0x420A90` epoch-to-seconds conversion, plus the `0x49E090` unsigned-short
  axis-angle to quaternion mapping, including its identity threshold. Added
  `expand_ncar_wrapped_tick`, `ncar_seconds_from_tick`, and
  `decode_ncar_rotation16`; corrected the scale to the observed
  `40/32768 - 20` mapping (range `[-20,60)`). x87 rounding and the Pentium
  FDIV compatibility correction remain approximate.

- Corrected the nearby procedure inventory: the observed constructors are
  `0x438430` and `0x439040`; the earlier next-target values `0x4384D0` and
  `0x439200` are not procedure starts in the current Ghidra database. Recovered
  NCAR timing predicates and the `0x439160`/`0x439190` timer and first-entry
  field behavior, then added the offset-preserving subset in
  `include/viper/ncar_state.hpp` and `src/ncar_state.cpp`.
- Recovered scene callback rising/falling masks for its sixteen 16-bit words,
  the exact wheel basis-position and previous-position delta equations, the
  mirrored transform copy, and no-op matrix-helper alias behavior. Added the
  numeric edge and transform helpers to `viper_scene_model`; traced effect
  wrappers through their resource-row gates and recorded remaining opaque
  resource operations in `metadata/scene_car_update.md`.
- Corrected the scene edge model to zero the saved words before comparison on
  state reset, matching the callback's clear-before-compare order; added the
  exact rising-bit count that controls repeated resource-event calls.
- Recovered the NCAR three-slot record stride and directly consumed offsets in
  `0x439490`/`0x439520`; added a raw-bit decoder for the time, interpolation,
  vector, and gate fields while preserving the opaque record bytes.
- Corrected the first-entry NCAR field initializer to preserve exact bits
  `0x414C0C4A` (`12.753000259...`) and aligned its comparison documentation
  with the unsigned test in the disassembly.
- Reduced both `0x439520` vector output triplets to quadratic derivatives,
  modeled the `96.23611` magnitude gate/`9.81` scale and `+0xF24/+0xF28` ramp,
  and traced `DAT_004E9754` to MSVC's Pentium FDIV compatibility check. The
  portable equations use double intermediates; exact 80-bit rounding and the
  FDIV-emulation branch remain open.
- Confirmed the overlapping record shift in `0x439490` is handled backward by
  the CRT `_memcpy`/`_memmove` implementation at `0x4BD4F0`; added the exact
  record acceptance gate, three-slot shift, timestamp write, and mode result
  to `viper_ncar_model`.
- Traced `0x439110` through the car-ordinal lookup to `0x43AD70`; added its
  changed/nonzero byte gate, two-second target throttle, sample-age gate,
  "plane" branch, vector writes, and raw twelve-dword copy to the NCAR model.
- Traced the second `0x43AD70` caller at `0x434A80`, including its two-global
  enable gate, mode-11 global-state/zero-vector selection, and unconditional
  callback-byte cache write; added `run_ncar_linked_callback` and documented
  its inputs in `metadata/car_update_branches.md`.
- Traced the `0x43A130` timed callback gates and the numeric ramp in
  `0x43A190`; added `advance_ncar_timed_interpolation` for its four-field ramp
  and output-vector scale. Its indirect setter and exact x87/FDIV behavior
  remain unresolved.
- Traced the `0x439A10` prediction/collision branch and `0x438D30`'s
  per-component weighted blend; added the latter as `blend_ncar_vector` and
  recorded the former's raw offsets and branch thresholds.
- Continued the race-loop callback pass: state 4's wrapper still reaches the
  shared banner/result/loading gates after skipping the main dashboard body;
  added the offset-preserving gate projection to `viper_scene_model`. State 5
  reads a row-major 3x3 matrix and converts it to a quaternion through one
  positive-trace and three largest-diagonal branches; added
  `viper_matrix_quaternion`, then modeled the acos/sine rotation-vector
  calculation, background rectangle arguments, camera label, position /
  rotation text, and the 44D6A0-to-44D780 glyph calls in
  `state5_numeric_overlay_plan`. Extracted the 96 glyphs for bytes `0x20` to
  `0x7F` and projected clipped, format-specific framebuffer writes for each
  marked pixel through `0x449E90`. Added
  cheater-banner
  placement, the paused-banner resource-generation refresh/draw call, and the
  two shared raw-flag text
  setup sequences, results heading/prompt registration, layout and draw setup,
  the nested results-record eligibility/virtual-call gates, and their
  conditional lookup/formatted-time values to the shared-overlay model;
  per-record rendering, the cheater draw-mode meaning, other renderer effects,
  state-5 x87 edge fidelity, and exact Pentium FDIV correction remain open.
- Traced all 15 direct callers of the `0x45C2C0` track-state accessor relevant
  to the `0x426000` factory selector. The getter returns `&DAT_0053B17C`, and
  direct address xrefs still show only that pointer return and the later read
  of its `+0x18` DWORD at `0x45D9E2`. Inspected caller bodies showed their
  observed uses of the returned state at other offsets or passed to the
  factory; no caller body observed here writes the selector. This bounds the
  result to the analyzed direct callers and does not rule out unresolved
  indirect writes or an external configuration source.
- Corrected and completed the VxD timer path: the `0x414C30` worker directly
  issues control codes 1 and 3, runs callbacks against interval deadlines,
  and reports repeated overruns; `0x414840` checks startup with a 100 ms test
  callback. Updated the earlier inaccurate note that those controls lacked
  callers. Added a portable worker-iteration scheduler in
  `include/viper/legacy_timer.hpp` and `src/legacy_timer.cpp`. The driver
  response and internal implementation remain unknown.
- Followed the active race render branch through `0x447C90`, `0x44E210`,
  `0x45C090`, and the line/pixel helpers. Added an offset-preserving render
  context model for viewport bounds, half-open pixel containment, pitch-based
  pixel offsets, and tag-selected pixel conversion. Higher-level rendering
  semantics and format names remain unresolved.
- Turned the traced `0x422740` physics-task step selector into
  `viper::schedule_physics_steps`, including its mode-0 divisor/20 ms guard,
  mode-2 byte toggle, `__ftol` rounding path, mode-dependent catch-up caps, and
  anchor recapture. Exact x87 extended precision and unusual counter-overflow
  cases remain approximated.
- Traced the `0x416080` `\\.\vexed.vxd` open through `DeviceIoControl` uses and
  the allocator fallback. Confirmed the caller discards the open result and
  ordinary blocks fall back to `HeapAlloc` when the VxD path is unavailable;
  added a portable allocation-plan model and `metadata/legacy_vxd.md`.

- Confirmed the extracted x86 PE can be opened with the available REA/Ghidra
  12.1.4 analysis provider.
- Analyzed the engine entry at `0x4BD690` and followed its application
  handoff to `0x411F10`; recorded observed flow and limitations in
  `engine_application_entry.md`.
- Corrected the entry's version source to `GetVersion`; recorded the separate
  disassembled SEH callback fragment that calls `0x4C1300`.
- Corrected the exception mappings to seven observed internal runtime codes,
  rather than C signal numbers.
- Added `viper_startup_gates` for the `0x4120A0` named-semaphore/single-instance
  gate and `0x412050` canary gate. The models preserve the `ERROR_ALREADY_EXISTS`
  branches, window restore and handle-close effects, warning path, and exact
  low-byte return gate consumed by `0x411F10`.
- Extended `viper_startup_gates` with the `0x412270` loading-window plan:
  class-registration gate, cursor/background resource IDs, window styles,
  screen-centered integer coordinates, and show/update calls after successful
  creation. The indirect title pointer remains unresolved.
- Added `plan_startup_initialization` for `0x416360`, preserving its ordered
  helper calls, low-byte branch gates, heap-failure cleanup route, final
  initialized-flag write, and forced success byte. The called subsystem
  helpers remain independent reconstruction targets.
- Modeled the `0x414F80` critical-section gate through its `0x4152C0` wrapper
  and `0x4152E0` 16-slot allocator, including sequential allocation, first-free
  reuse, the record used flag, the panic/full-pool branch, and the
  `InitializeCriticalSection` call effect. Composed the pool into
  `plan_startup_initialization`, so exhaustion drives the allocator and caller
  panic sequence before cleanup. Release sites remain unresolved.
- Replaced `0x416360`'s supplied first-gate boolean with `select_clock_backend`
  from `0x413610`: CPUID/TSC and single-processor selection, TSC-path frequency
  failure, QPC fallback callback installation and timing globals, and its
  forced-success return bytes are now represented from direct inputs. The TSC
  path also preserves the sampled origin counter.
- Removed an unreachable `0x416360` failure input after `0x410AA0`: the logger
  initializer's observed return forces its low byte to one. The startup model
  now follows `0x416680`, `0x4154A0`, and `0x414110` unconditionally after that
  initializer; the heap-creation result remains the next modeled failure gate.
- Modeled the calibrated timer's `RDTSC` low-word delta, QPC overshoot ratio,
  truncating `__ftol` conversion, and `0x4D2364`/`0x4D2368` stores in
  `calibrate_tsc_clock`, then composed it into `select_clock_backend` when the
  calibrated branch passes `QueryPerformanceFrequency`. Its x87/FDIV precision
  path remains approximate.
- Traced the first startup helpers and frontend dispatch. Confirmed named
  semaphore/canary checks, the loading window, the `vexed.vxd` probe, resource
  names, and state-handler call targets; their system and screen semantics
  remain partially unresolved.
- Reconstructed `0x401280`'s command-line switch scan in
  `viper_frontend_model`: `/` and `-` prefixes, location/tri/grid options,
  programmer mode selection, the mutating `-d` directory path, `-z`, and
  `-1`'s write to `0x4D9CF0`. Directory success stays an explicit external
  observation; the path truncation and diagnostic branch are represented.
- Added `plan_startup_frontend` for `0x401460`'s mode-to-helper routing,
  including the observed mode-0 `0x401E10` loop and common `0x4172A0`
  finalizer. Loop count is supplied by the caller because it depends on UI
  interaction.
- Added `plan_frontend_initialization` for `0x4014D0`: ordered setup and
  subsystem calls, the `CDDirectory` lookup gate and literal `intro.avi`
  concatenation, the twelve-float identity-layout write at `0x5081A8`, and the
  three resource names. The resource and subsystem implementations remain
  independent targets.
- Modeled `0x412120`'s executable-path requirement and config-directory setup
  in `viper_frontend_model`: case-insensitive `\Data` validation, fatal
  installation exit code 6, `\\Config\\` output at `0x4EF0C0`, and the
  root-to-leaf `CreateDirectoryA` candidates. Directory creation outcomes are
  explicit observations to `evaluate_config_directory_creation`, including
  `ERROR_ALREADY_EXISTS` handling; the outer initializer ignores its result.
- Added `plan_startup_shutdown` for `0x416450`: it records the initialized-flag
  panic gate, fourteen ordered cleanup targets, the `0x413BC0` thunk to
  `0x4165CF`, direct return value, and successful-path clear of `0x4D2FBC`.
- Modeled the separate `0x4161A0` conditional VxD-handle close/sentinel reset
  and `0x412110` unconditional close of the `0x4D1CB8` semaphore handle.
- Confirmed `0x401E10` dispatches selectors 0, 1, 2, 3, 4, 7, and 8 to
  observed handlers/return behavior. Added this numeric dispatcher to the
  portable C++ model in `include/viper/frontend_dispatch.hpp` and
  `src/frontend_dispatch.cpp`; the mapping from visible menu labels to these
  selector values remains unresolved.
- Confirmed `0x40A410` enters a pre-race option loop that calls the option
  reader/parser helpers and repeats until its completion condition; detailed
  field meanings remain unresolved.
- Analyzed `0x40EED0`: recorded its pre-race interface labels, resource names,
  and raw return classification in `metadata/frontend_dispatch.md`; added a
  small portable classification model while leaving button semantics open.
- Followed the return values into `0x40A410`: recorded the refresh, initialize,
  copied-entry initialize, and fallback branches, and added their routing model.
- Traced the first race-loop branch (`0x401EA0` to `0x4020E0` or `0x41AB00`)
  and recovered the escape-menu numeric event mapping in `0x4022F0`; added the
  mapping to the portable frontend model without assigning unverified labels.
- Recovered the car-record stride and wheel-lock bit bounds in `0x45C5D0`,
  plus unit conversion constants and tire-file fallback behavior in
  `0x441C10`; added an initial vehicle-setup module and evidence note.
- Reconstructed the RST0 bundle directory and chunk layout from `0x417430`,
  `0x417550`, `0x417CB0`, and 18 shipped `.car`/`.res` files; added a portable
  bundle reader and kept payload type decoding as the next step.
- Recovered the 120-byte TIRE payload shape from `0x435B50` and bundled tire
  resources; added a label-plus-22-floats decoder and CLI inspection action.
- Traced the tire initializer at `0x435BF0` into its property setters and
  documented every consumed float index and observed scaling/trigonometric
  transform in `metadata/vehicle_setup.md`; recovered the 0x40-byte tire
  curve-object field writes and two slope/intercept calculations, while
  keeping their physical meanings unresolved.
- Followed car construction (`0x42FF90` → `0x433C00`) into four wheel
  subobjects, then traced the active entity callback (`0x45BD20` → `0x431150`)
  into its four wheel updates (`0x442530`). Confirmed the tire response record
  is evaluated there and resulting vectors/moments are accumulated by the
  rigid-body update routines; added portable linear and quadratic evaluators.
- Traced the wheel's terrain-plane query (`0x45F760` / `0x4672B0`) and normal
  contact-response calculation (`0x42F100`), including the effective-mass
  denominator and capped scalar response. Added a portable implementation of
  that normal impulse calculation using the observed matrix ordering.
- Recovered the four-coefficient, piecewise wheel response at `0x4405B0` and
  the two normalized velocity expressions used before TIRE curve evaluation
  in `0x442530`; added portable evaluators and documented their precise
  arithmetic without assigning unsupported field names.
- Reconstructed the TIRE response coefficient pairs and their three stored
  multipliers from payload indices 9–19. Traced the final forward/lateral
  response scalar equations, wheel-basis projection, and unadjusted response
  vector through `0x43E6F0`; added portable coefficient, scalar, and vector
  helpers.
- Confirmed `0x431150` updates all four wheels before entering `0x43ECE0`.
  Documented the body's gravity contribution, six-value normalized accumulator
  scaling, per-record virtual callback, velocity/angular updates, and running
  accumulator reset. The outer frame scheduler and field units remain open.
- Traced `0x4020E0` into the `0x45BD20` entity dispatcher. Documented its
  per-object callback order, sorted second callback phase, and the car's
  wheel-then-body update path. Added `entity_update_callback_order`, which
  reproduces the direct/deferred virtual call sequence and insertion behavior
  for equal or unordered float keys. The scene-car `+0x18` target at `0x4648A0`
  was traced in the shipped disassembly to its selected-ordinal skip,
  camera-distance/predicate gates, and raw record/object draw inputs; label
  semantics remain unknown. Recovered the six nonempty `0x4020E0` callback
  rows for states 0 through 5, including call timing and the context-taking
  slot; added a raw address lookup model. Added the key-`0x31` state-0/state-4
  toggle, dirty-flag write, and loop re-entry to that callback model. Callback
  target meanings remain open. Also modeled the adjacent event-to-numeric-page
  writes at `0x4CF5B4`, including the guarded `0x17B` no-op and event `0x37B`
  value-11 branch. The dispatcher measures clock deltas, but their connection
  to the body's `0.016` factors is not established.
  Modeled page 11's `0x401930` camera controls: four speed modes, key-driven
  translation/rotation, damped persistent velocities, orientation/position
  updates, and the rising edge that requests camera-preset capture through
  `0x4203E0`. Added the seven-DWORD preset record projection, unconditional
  position writes, and conditional angle fields at `+0x10`/`+0x14` when the
  record's first DWORD is zero. Exact x87 rounding and the remaining record
  semantics are open.
- Continued through state-1 callback row `0x4CF5D4`: its `+0x04` slot resolves
  to render callback `0x41A690`, whose numeric page switch draws Help/Info,
  eight selectable row records, a distance/speed line, and two save prompts.
  Added `scene_overlay_text_plan` for those observed text calls and documented
  the additional key-event callback at `0x41A370`; page-2 geometry, event
  meanings, and the rest of the state rows remain open.
- Traced state-2 renderer `0x423380` and key handler `0x4236B0`: recovered the
  Physics/COLLIDE/WALLS labels, selected-car and global-coordinate formats,
  four wheel-record helper calls, and the `c`/`w` flag-byte transitions.
  Added `scene_physics_overlay_text_plan`, the exact flag updater, and ordered
  wheel-bar wrapper operations for fill, frame, indicator line, and text.
- Traced state-3 renderer `0x40D570`, key callback `0x40D650`, and toggle
  wrapper `0x40D740`: recovered the LOD value display, input codes and step,
  raw-bit clamp, and the `0x40`/`1` action states displayed as FOG/LIGHT.
  Added a portable input/state model and text plan in
  `include/viper/scene_overlay.hpp` and `src/scene_overlay.cpp`. The underlying
  renderer/device effects and exact x87 rounding remain open.
- Added `plan_multiplayer_frontend` for the `0x40A570` state-1 loop: it captures
  the `0xCD4` record initialization, initial poll, repeated race-setup and
  post-step calls, and the terminal or unobserved next-poll condition. The
  network/lobby internals of `0x40A230` remain unresolved.
- Added `plan_career_frontend` for `0x4AA460`: observed nonnegative menu
  selections route through `0x4AA490`; a negative result exits and restores
  `0x4D10FC` from `0x4D10F8`. Create/load/delete screen effects remain open.
- Classified the state-0 dashboard row: `0x4052D0` dispatches its context to
  `0x404970`, whose `0x405100` path applies dashboard rectangles and whose
  conditional overlays include pause, cheater, wrong-way, results, and loading
  resources. The slot `+0x08` callback `0x4052E0` copies six state bytes then
  draws conditional geometry. The `0x4035E0` block also exposes Position,
  RaceTime, Lap, LapTime, TestTrack, Best, and Last resource rows; field
  meanings and most gates remain unresolved. Added `scene_dashboard_viewport_plan`
  for the exact `0x405100` base and mask-selected rectangle viewports. Dashboard
  text tracing now has direct call coordinates, format sources, flags, and color
  globals for Position, RaceTime, Lap, and LapTime rows. Added
  `scene_dashboard_text_plan` to project eight such calls from caller-supplied
  gate outcomes, plus `update_scene_dashboard_position` for selected-ordinal,
  context/reset, clamped-position, count-adjustment, cached-value, and dirty-mask
  transitions and common Position/RaceTime resource bits; the ordinary Lap
  path also registers its resource bit. Added `update_scene_dashboard_lap_time` for the type-0 LapTime
  branch, including its signed raw-float gates, resource registration, redraw
  transition, label/time calls, color selection, and fallback asymmetry where
  the time row draws each visit but the label needs a redraw/state change.
  The nearby `-%03.1f` and `+%03.1f` formats belong to supplemental rows, not
  LapTime. Added `update_scene_dashboard_test_track` for the opening TestTrack
  path, including kind change, Try/Time/Distance resource registration, Try
  label gating, displayed-position cache, and dirty rectangle, plus the
  type-selected Time/Distance label refresh, fixed-width time formatting, and
  scaled Distance string composition. CRT/x87 edge behavior and upstream
  resource/global values remain inputs. Added
  `update_scene_dashboard_lap` for the ordinary type-0 path,
  and `update_scene_dashboard_final_time` for the adjacent type-0 final-time
  branch. Both preserve the supplied virtual-call results, dashboard-kind
  transitions, call-site masks, and rectangle invalidation. Also modeled its
  ordinal/validity-scanned result-time lookup, including the flagged-row
  candidate behavior, and fixed-width time formatter. Added the supplemental
  type-0 `+0x24`/`+0x1C` row gates, raw signed-word color thresholds, and both
  associated draw-call pairs without assigning field meanings. Added the `+0x10`
  and `+0x14` metric cache, dirty-bit, threshold, and type-selected draw-call
  projections, retaining their `-1.0F` initial cache bytes and x87 unordered
  comparison behavior. Added their type-3/type-5/default intermediate string
  formats, time marker substitution, and composed displayed text with external
  resource strings preserved as inputs. The `+0x10`/`+0x14` cache-update path
  now also tracks Best/Last resource registration bits and generation refresh
  gates on the above-threshold non-type-3 display paths. Added
  `scene_dashboard_auxiliary_readout_plan` for the following raw `+0x4C` scaled
  integer readout, its three-character `999` substitution, glyph-based y
  positioning, and the two full-redraw-only string calls. Gameplay semantics
  and external strings remain unknown/input data.
  Other text gates and bitmap/device effects remain open.
- Recovered the visible main-menu selectors by tracing type-2 descriptors
  through `0x472130` and `0x475B40`, then through the generic result callback
  to `0x479DA0`. Confirmed Single Race 0, Multiplayer 1, Career 2, Options 3,
  Replay 4, and Exit -1 normalized to 8. Added the mapping and normalization to
  the portable frontend model; selector 7 still has no verified visible item.
- Audited the selector-7 dispatch target and startup-menu descriptors: the
  target `0x41C870` returns zero and has only the switch as a verified caller;
  none of the visible type-2 entries uses ID 7, and the player-name type-9
  control installs no selection ID. The origin of selector 7 remains open.
- Traced body scalar initialization through `0x42FF90` to `0x43E490`: body
  index `0x7C` is set from a converted configuration scalar and index `0x80`
  to its reciprocal. Since `0x43ECE0` uses the pair for gravity and force
  integration, added a linear-velocity step helper with the observed fixed
  per-call `0.016` factor. Its relationship to outer-loop pacing remains open.
- Recovered a separate physics-task substep schedule: `0x425540` registers
  callback block `0x425B60`; its active states call `0x4207C0`, which computes
  due steps from elapsed clock time against `DAT_00508774 * 0.016`, caps catch-up
  at 2 or 4 steps, and advances the simulation counter per substep. Continued
  tracing confirmed `0x420480` fills the task's `DAT_00507D0C` object array via
  `0x4224D0` and factory `0x422580`; `NCAR` and `PCAR` objects are constructed
  into those slots. Their virtual `+8` callbacks conditionally call
  `0x431150`, which updates wheels and body integration. Documented the loader,
  vtable targets, and call edges in `metadata/physics_task_objects.md` and
  corrected the scheduler note in `metadata/simulation_loop.md`.
- Traced the separate world-record parser at `0x45CAE0`: numeric car variants
  map to `PCAR`, `ACAR`, `NCAR`, and `GCAR` records; the resulting scene wrapper
  is appended to the main entity registry through `0x45C320`/`0x45C2E0`. Its
  vtable `+8` target is `0x463EB0`, distinct from the physics car callback.
  Confirmed the parser writes the current zero-based car ordinal into its
  generated `0x1E4`-byte construction record at `+0x194`; `0x463920` copies
  that value to scene-object `+0x1C8` and uses it to register the two source
  strings in the shared 16-row table. Added the portable
  `WorldCarSceneRecordIdentity` pair for the verified variant tag and ordinal.
  Added the tag mapping helper `world_car_object_tag` and documented the
  distinct scene and physics object paths. Traced `0x463920` → `0x463510` →
  `0x425720`: the exact car record is written into `DAT_00508754`, which the
  physics task later reads through `0x420480`/`0x4224D0`. This establishes the
  shared car record and ordinal, while indirect effects from scene updates
  remain open.
- Added a CMake/C++20 cross-platform source target and an external asset
  catalog utility. The source tree does not include original game assets.
- Followed the scene update callback through change notifier `0x463530` and
  vtable slot `+0x24` to `0x465800`. Recovered its selected-car state gates,
  prior-state comparison, 99-float pose copy, four-entry wheel transform loop,
  and conditional effect call edges. Added `metadata/scene_car_update.md` and
  portable models for the verified numeric state classifier, pose copy, and
  wheel transforms in `include/viper/scene_car_update.hpp` and
  `src/scene_car_update.cpp`; wired the module into CMake. The vtable `+8`
  wrapper's camera-distance threshold search and adjusted indices are
  now modeled too. A later vtable audit established that wrapper slot `+0x28`
  targets a false-return stub, while the update method is slot `+0x24`.
- Expanded the static `PCAR`/`NCAR` callback notes with numeric gates, timestamp
  paths, backend calls, the unconditional PCAR join to `0x431150`, and NCAR's
  two separate integration branches. Recorded unresolved fields and helper
  semantics in `metadata/car_update_branches.md`; no unverified meanings were
  added to the source model.
- Inspected the shared `PCAR` setters and `0x4330F0`: recovered four direct
  scalar destinations and the finite-value delta/accumulation clamp, including
  the mode-selected output pairs. Added the offset-preserving implementation
  in `include/viper/pcar_control_update.hpp` and
  `src/pcar_control_update.cpp`, and wired its library into CMake.
- Traced the adjacent embedded helper object at parent `+0xC98`: established
  the exact `0x43FAD0` store, the `0x4331E0`/`0x433210`/`0x433230` wrapper
  copies to `+0xD64`, and the raw-index loop in `0x43FDD0`. The numeric
  candidate scoring is now modeled; table entry meanings and virtual-refresh
  side effects remain unresolved in `metadata/car_update_branches.md`.
- Analyzed the embedded object's score helper chain `0x4404A0` → `0x4404D0` and
  `0x43FA20`, recovering its cutoff, quadratic/linear expression, optional
  constant-80 branch, and final subtracted term. Added the offset-preserving
  evaluator in `include/viper/update_curve.hpp` and `src/update_curve.cpp`,
  wired it into CMake, and documented its unresolved field semantics.
- Reconstructed the remaining `0x43FDD0` gates and candidate loop: epsilon and
  index guards, step adjustment, threshold rejection, max-score/tie behavior,
  and the preserve-current condition. Added the numeric selector to the same
  portable module. Traced its virtual `+0` slot to `0x43F970`, a getter for
  field `+0x10` whose discarded return does not mutate selector state.
- Traced curve allocation/configuration through `0x440100` and `0x4401D0`,
  including vtable `0x4C8780`, default selector inputs, six coefficient
  stores, the cutoff/gate parameters, and the derived `+0x30` value. Added
  `configure_update_curve` to the portable curve module and corrected the
  wrapper evidence: `0x4331E0`/`0x433210`/`0x433230` copy parent `+0xCBC` to
  parent `+0xD64` after operating on the subobject at `+0xC98`.

## Current state

The repository is not a playable reconstruction. It still contains preliminary
Windows-specific C fragments and has no simulation, renderer, input, audio,
save, replay, or network implementation. Existing decompilation notes need an
evidence audit before their claims are reused.

The local environment includes Visual Studio 18 Build Tools, MSVC 19.51, and
CMake 4.2. The complete `Release` build now succeeds. This verifies compilation
and linkage of the current reconstruction tools, not game behavior or
original-runtime fidelity.

## Next evidence targets

1. Recover remaining helper effects and record layouts used by the PCAR and
   NCAR callbacks. The procedure inventory confirms the nearby addresses are
   `0x438430` and `0x439040` rather than `0x4384D0` and `0x439200`.
   Implemented NCAR time predicates, timer decay, first-entry rotation writes,
   a raw decoder, and the sample-window acceptance/shift in `viper_ncar_model`.
   PCAR helper analysis confirmed the nearby gate, scalar, and cache helpers
   are direct accessors for globals; documented their addresses and added an
   offset-preserving shared-global state, three-store helper, and the
   bit-pattern gate and scalar transform in `0x43B8C0`, plus the clamped
   global-derived value from `0x43B950`. Recovered the three selector branches
   in `0x433250` and its force/moment accumulator writes through `0x43E6F0`,
   adding a raw-offset effect accumulator model. Also modeled the PCAR status
   dword's signed-bit tests and its timestamp latch, including its backend
   request edge and expired sentinel. Recovered the PCAR callback's 16-object
   neighborhood scan, two projection windows, status transitions, and target
   notification mapping; constructor strings identify the transitions as
   clear/left/right/both SFX cues. Added an offset-preserving scan model.
   Exact `0x439520` x87 rounding and exceptional Pentium FDIV handling still
   need recovery; its ordinary-path equations, common finite-normal FDIV
   correction, history shift, derivative, gravity cap, and ramp writes are
   composed in `advance_ncar_sample_dynamics`. The CRT flag initializer and
   affinity probe order are modeled in `viper_legacy_fdiv`. NCAR
   callback input/event effects have queue and accepted-sample models. The FDIV branch is
   now mapped through `0x4BC437`, `0x4BC320`, and `__adj_fdivr_m32` at
   `0x4BC9FC`; exact emulator reconstruction remains open. The NCAR event
   path's generic timed FIFO behavior, 62-byte decoded record projection,
   wrapped-tick/seconds conversion, packed-rotation codec, 24-byte-payload
   producer/consumer, and chat-configured queue latency offset are documented
   and modeled; exact x87 rounding and FDIV correction remain open.
2. Recover the scene callback's individual 16-bit bit meanings and remaining
   effect paths. Recovered rising/falling edge masks, basis-position equation,
   and the no-op wheel rotation helper calls; added the edge, basis, position,
   delta, and mirrored transform arithmetic to `viper_scene_model`. Bit labels
   and effect calls remain open.
3. Follow frontend transitions into race setup, simulation updates, and render
   submission; confirm function bodies and call edges at each step.
4. Trace the VxD timer interface: codes 1 and 3 are direct calls from the
   `0x414C30` worker, and code 2 follows the
   `0x425880`/`0x4C5A80`/`0x4C5830` timeout path. The software timer test and
   worker lifecycle are documented in `legacy_vxd.md`; the external driver
   implementation and response semantics remain unavailable. The core
   allocator already has a direct heap fallback.
5. Establish an original-game behavioral capture before fixing fidelity
   tolerances for simulation and timing.

- Corrected every recovered `0x44DC70` state-0 coordinate and flag by mapping
  caller pushes (y, x, flags) to semantic x/y/flags. The state-0 metric,
  auxiliary, and supplemental rows now use the decoded values.
- Extended state-4 results from row selection/time formatting to the three
  per-row text calls, including ordinal/string/time formats, helper-selected
  color source, vertical row stepping, and explicit horizontal stack seeds.
  The `0x45C230` return is now modeled from raw global `0x4DC19C`; record text at `+5` remains observation data.
- Extended `initialize_ncar_rotation` to model the normal-path zero stores
  through setters at `+0xDE4`, `+0xDE0`, and `+0xDEC`, while leaving them
  untouched on the observed unsigned `+0x238 > 0x80000000` early return.
- Added `advance_ncar_callback_queue` to connect the `0x439200` strict
  1/15-second producer cadence, event enqueue/poll, and successful 62-byte
  sample handoff into `update_ncar_sample_buffer`. Payload generation,
  queue-clock inputs, and post-queue integrator effects remain explicit.
- Corrected the `0x439200` post-integrator gate-byte mapping: target bytes at
  `+0x574 + n*0x1A4` are normalized copies of sample bytes `+0x3A+n` (nonzero
  becomes 1). Added `apply_ncar_callback_gate_bytes` for this exact effect.
- Added `advance_ncar_callback_main` to compose the `0x439200` input-gate,
  mode-3, half-second timer, rotation initialization, and final 0.8 assignment
  branches, including its direct offset setter values, while exposing the
  unreconstructed integrators as reached effects.
- Modeled the shared `0x431150` entry reset under dword `+0x4FC == 0` and
  applied it at each reached integrator call in the NCAR frame model. The
  remaining wheel/body integrator stays open.
- Extended the `0x43ECE0` linear body slice through its strict squared-speed
  cap (`> 90000`, target magnitude 300) and position advance (`+0x17..+0x19`
  by velocity times 0.016) in `advance_body_linear_step`. Also modeled the
  `0x422F30` row-vector matrix transform from indices `0x96..0x98` and matrix
  `+0x264`, followed by the `+0x90..+0x92` velocity increment and strict
  squared-speed cap (`> 100`, target magnitude 10). Added the angular
  rotation-vector threshold, transposed Rodrigues matrix, and right-multiplying
  update of the orientation matrix at `+0x38`. Modeled the following two
  row-major basis-matrix products through `+0x210` and `+0x264`, given the local
  matrix at the call site, and added the final six accumulator clears and
  16-bit update-counter increment. Construction of that local matrix remains
  unresolved because the assembly reads two stack words before initializing
  them in this function; x87 rounding is approximate.
- Traced `0x508758` into `0x421120` and `0x434A80`, resolving it as the camera
  mode selector. Added `store_race_camera_mode` for `0x4259B0` and modeled the
  mode-11 transform copy and zero-velocity setup from `0x421120`; subsequent
  entries extend the mode coverage and add the shared `0x46B920` postprocess.
- Added mode-8/mode-9 camera output plans for the quarter-turn X rotation,
  anchor-relative vertical offsets, and retained X/Z plus cleared Y velocity
  components. These model the branch inputs before the shared `0x46B920` call.
- Added mode-1/mode-2 camera output plans with the copied vehicle orientation,
  transformed local offsets `(0, 0.5, -0.1)` and `(0, 0.8, -0.5)`, accumulated
  camera position, and vehicle velocity values from the common preamble.
- Added mode-3 output for the vehicle orientation left-multiplied by the
  `3.1415927f` Y rotation, the transformed `(0, 0.8, 0)` offset added to the
  prior camera position, and unchanged vehicle velocity.
- Modeled the shared `0x4222A0`/`0x422C90` look-at basis rebuild, including its
  distance/horizontal-delta thresholds and normalized basis vectors; external
  refresh calls remain unresolved.
- Added the modes-4-through-7 follow-camera transition, including target-height
  and distance selection, optional velocity-directed position adjustment,
  cached-transform update, and the mode-7/vehicle-flag mirrored-camera path.
- Modeled the common `0x46B920` -> `0x46DD20` handoff into the shared camera
  object: update flag sequence, selected car index, 12-float transform, and
  three velocity stores in binary order.
- Added mode-10/mode-12 camera preset selection, nearest-target search, typed
  preset axis-angle/look-at construction, distance adjustments, velocity clear,
  selected-index write, and the count-at-most-one fallback. Reclassified the
  third preset scalar as consumed data rather than an opaque tail.
- Added mode-0 camera reconstruction from `0x421120`: persistent target and
  cosine initialization, terrain-sample blend, target smoothing, basis rebuild,
  local camera offset, and trailing vehicle-axis offset. The surface query and
  common velocity/postprocess inputs remain external.
- Extended the type-0 dashboard supplemental-row model with the observed
  30.0-floor floating-point selection, signed one-decimal formats, decimal
  marker substitution, and the `0x45DFB0` indexed-string results at `+5`.
  The runtime table layout and safe valid-index setter/lookup are now modeled;
  constructor record text and the indexed virtual method result remain inputs;
  the mode-2 presence/bound gates are modeled.
- Added `scene_dashboard_render_plan` to preserve `0x405100`'s full-surface,
  bitmap, viewport, and restore call order, with the `0x44AFD0` bitmap
  dimensions and renderer mode exposed as raw inputs. The renderer/device
  branches inside `0x44A730` are represented by the blit plan, per-pixel
  conversion helper, and clipped source-buffer write projection; device writes
  remain outside the plan.
- Extended `scene_shared_text_effect_plan` with the observed state-4 draw sites
  `0x404D6C` and `0x404DC1`, including their y=67 baseline, x placement,
  resource-text pointer, font, color source, and flags. Text measurement and
  the gate/resource outcomes remain inputs.
- Traced the dashboard auxiliary readout's `0x4F04F4` pointer initialization
  to `0x418370` and identified the `+0x18` runtime string source. Corrected the
  second full-redraw string input to document its raw `[esp+0x18]+5` source;
  the callback body has no observed write to that stack slot.
- Extended the state-4 paused-banner model through `0x40C930`/`0x40C790` with
  centered resource placement, nested fallback boxes, text placement, and
  resource release; lookup and renderer measurements remain explicit inputs.
- Added the raw `0x404920` shared-resource draw placement for handle
  `0x4EB46C`, centered horizontally and offset `0x82` above the vertical
  midpoint. Its predicate meaning and measured resource width remain inputs.
- Traced `track.obt` loading through `0x45C630` and its case-insensitive object
  directive dispatcher at `0x45D270`. Recovered checkpoint parsing at
  `0x45D950`: the `CHKP`/`0x20` record stores two X/terrain-height/Z endpoints,
  with `0x45F600` providing terrain heights; raw `DAT_0053B194` selects visual
  object creation or serialization through `0x425720`. Added a portable
  checkpoint record and route plan while keeping terrain outputs and the
  global's meaning explicit. Also recovered `0x45D5D0`'s case-insensitive
  `pole`/`flap` mapping and raw `WOBL` record constants, and added a portable
  record model that leaves those constants semantically unassigned. Recovered
  the `0x45D6E0` static `box` record layout (`STAT`, 0x48 bytes), its
  rotation-vector matrix conversion and the zeroed payload word; added the
  portable record plan with the `0x40D2F0` dispatch gate and an explicit
  floating-point fidelity limitation. Obstacle construction and the remaining
  track loader are still open.
- Recovered the `0x45D390` obstacle grammar, case-insensitive `ball`/`cube`/
  `prism` IDs, terrain-adjusted placement, model-bound differences, and the
  three `extent^2 * 10.749798 * scale` terms. Traced `0x509B0`'s vertex-block
  min/max walk (eight-float stride) and added a bounds helper. Tracing
  `0x463590` -> `0x463510` -> `0x425720` resolved the full 0x38-byte `OBST`
  record and model-handle/record-offset stores; the source model now emits the
  exact fields and appends the constructed object. The full track loader
  remains open. Materialized each modeled track record's header words in its
  C++ struct and added compile-time size/offset assertions for CHKP, WOBL, STAT,
  and OBST layouts.
- Recovered the BATS `track.obt` header and indexed-line address formula from
  `0x418F20`/`0x418FA0`/`0x418FB0`, then checked it against all eight extracted
  track bundles. Added a fixed-stride payload parser and bounded line accessor.
  Further traced `0x45C630`'s top-level line handling: `end` stops the loop,
  semicolon-first tokens are comments, `map` and `ai` are accepted without an
  observed state effect there, `obj` is dispatched, and other tokens take the
  bad-token error route. Added a classifier preserving object lines; full map,
  AI, terrain-query, and track traversal effects remain unresolved.
- Added `plan_car_track_line` from the recovered `0x45CAB0`/`0x45CAE0` flow:
  configured-index gating and increment order, PCAR/ACAR/NCAR/GCAR tags,
  subtype-specific constructor choice, and both default and precomputed pose
  routes. Added a raw-word CARF v6 payload parser and confirmed attribute 6 and
  552-byte payloads in all five extracted `.car` bundles. Added ordered
  `0x45E4B0` runtime record writes for the complete observed store set, including
  interpolation, scale, setup-count and mode branches, while leaving unassigned
  bytes out of the patch list. Added an ordered patch applier that preserves
  untouched destination bytes. Host arithmetic approximates the Pentium FDIV
  correction path; field semantics, terrain querying, loaded
  `0x45CE10` geometry values, and subtype side effects remain open.
- Recovered the ACAR helper's raw `+0x2C` and `+0x30` thresholds, its writes to
  car-record offsets `+0x08`, `+0x150`, and `+0x154`, its configuration-index
  remap and provider lookup, and the `WorldGameOptions`/`WorldGetTrackNumber`
  calls; added an observation-driven adjustment plan. Confirmed the
  GCAR timer getter returns `DAT_004D1214`. Modeled `0x45D1C0`'s configured
  BALL record, `(0,0)` terrain-height input, `ball.mod` constructor path, and
  append call, with compile-time record-layout assertions.
- Recovered `0x45C860`'s first-checkpoint scan and the track-basis setup it
  performs before the `track.obt` directive loop. Added a portable plan for
  its identity route and centered 180-degree Y rotation/translation, including
  the fatal no-checkpoint path and the accepted three-coordinate scan whose
  fourth coordinate is uninitialized. Matrix/trigonometric rounding remains
  approximate; the precomputed helper's loaded geometry contents remain inputs.
- Reconstructed `0x41E380`'s circular-list segment selection and unclamped
  fraction, then modeled `0x45CE10`'s pose math: four-sample cubic
  interpolation, normalized direction interpolation, ordinal-parity lateral
  offset, terrain lift, and 3x3 orientation. Integrated this into
  `plan_car_track_line`, including fallback when the segment search fails.
  Loaded `track.ild`/`dlatlong` geometry contents and terrain query remain
  observations; x87/Pentium FDIV rounding remains approximate.
- Recovered the `ILIN`/version-3 resource envelope (`0xFFFFFFFE` marker,
  `0x44` record stride) and `0x41CF60`'s orientation-dependent row ordering,
  duration/key normalization, direction sign changes, category remap, and
  circular links. Added raw-preserving parse/normalize functions and a
  projection from normalized records into the spline selector's key/duration
  nodes. Actual `.ild`/`.ili` records and the terrain query are still inputs.
- Modeled the ILIN point-to-track projection path: `0x41D290`'s orientation-
  bracketed nearest-endpoint segment choice, and `0x41D3A0`'s 10-step tangent
  correction, forward segment traversal, 10000-key cap, negative-fraction
  clamp, and same-segment backward invalidation. Extended normalized row
  projections with X/Z position and direction fields. The projection start
  point and exact x87/Pentium division behavior remain inputs/approximations.
- Added the observed `0x41E6C0` projection-key formula and the separate
  `0x45CE10` placement-base offset as portable plans. Stack layout confirms
  the placement base aliases geometry `+0x5C`, set by `0x41E070`; the
  projection key at `+0x54` is distinct. Host float rounding remains
  approximate.
- Added `plan_car_spline_track_placement` to compose the placement-base key,
  `0x41E380` segment selection, adjacent ILIN row endpoint/tangent mapping,
  and the `0x45CE10` pose plan. Terrain height and decoded ILIN records remain
  observations.
- Recovered `0x45CAE0`'s placement-ordinal source: global car index minus the
  GCAR counter, after the counter increment for subtype 3. Added a 32-bit
  ordinal helper and documented the callsite evidence.
- Connected the composed spline-placement observation to
  `plan_car_track_line`. The line plan now derives the ordinal from its car
  index/subtype and supplied pre-increment GCAR count, and retains the selected
  segment/pose plan alongside the transform. Runtime geometry and terrain
  values remain inputs.
- Traced `0x45CAE0`'s stack alias: the helper output buffer begins at runtime
  car-record offset `+0x18` and contains the 12 transform floats. Added
  `apply_car_track_placement` to write those bytes into a complete `0x1E4`
  record while preserving the CARF-mapped fields outside the transform.
- Recovered `0x438E40`'s direct NCAR initialization writes, four wheel-slot
  marker bytes, three-slot sample-window allocation size/clearing, and initial
  interpolation values. Added `initialize_ncar_constructor_storage`; allocator,
  base-constructor, and global registration effects remain external inputs.
- Reconstructed the matching `0x439040` teardown order: conditional global
  delay-queue destruction, active-manager unregister, indexed slot release,
  sample-window release/pointer clears, and base-car cleanup. Added an ordered
  action plan and the direct vtable/pointer writes; indirect cleanup effects
  remain represented as actions.
- Composed accepted NCAR sample updates with `0x439110`'s mode and endpoint
  flag gate and the existing `0x43AD70` linked-target math. The target lookup,
  source object fields, type string, and terrain/world state remain observations.
- Resolved the PCAR callback's late helper effects: `0x4402D0` sets parent
  byte `+0xC3C`, the mode-3 branch resets `+0xDE0` and floors `+0xDE4` at 0.2,
  and `0x434B20` stores the `0x43BA10` value at `+0x508`. Added
  `advance_pcar_callback_tail` with the signed-bit threshold and evidence.
- Traced the PCAR post-integrator record to global buffer `0x507DB0`: `0x422890`
  returns the same address after two no-op helpers, and the callback writes
  five consecutive raw DWORDs. Added `write_pcar_step_record` for the exact
  0x14-byte layout; backend virtual results remain observations.
- Connected PCAR nearby-cue state transitions to their selected target's memory
  effect. Added `apply_pcar_nearby_notification` for the observed ordered
  stores `+0x29 = 1`, `+0x2B = 0`, `+0x2A = 0`, guarded by state change,
  target presence, and target-buffer size; other bytes remain untouched.
- Added the `0x43A130` NCAR timed-callback gate model. It reports the three
  ordered main-chain calls under the strict half-second gate, independently
  reports `0x439D30` when sample-window byte `+0x39` is set, derives its two
  rotation-based callback components, exposes the sample vector argument,
  writes the `+0xF2C` time and `+0xF20` gate, then clears `+0x39`. The virtual
  `+0x40` method's effect remains unknown.
- Reconstructed the `0x439DB0` rotation path: time-based sample-pair fractions,
  shortest-arc quaternion interpolation, the four-slot `0x43A0E0` weight, and
  matrix-to-quaternion-to-matrix stores. Added shared slerp and quaternion-to-
  matrix operations plus `advance_ncar_rotation_blend`; sample data and exact
  Pentium-FDIV/x87 transcendental fidelity remain inputs or approximations.
- Implemented the `0x439A10` position/velocity callback. It extrapolates the
  newest position with the observed quadratic, evaluates the three-sample
  position derivative at `2*now`, applies the reset threshold and shared
  `0x439D30` latch/callback arguments, or blends position and velocity with
  the observed per-axis weights. The virtual target effect and FDIV correction
  remain external.
- Composed the NCAR timed callback stages in `advance_ncar_timed_frame`:
  `0x43A190` ramp, `0x439A10` prediction/reset-or-blend, `0x439DB0` rotation,
  then the independent `+0x39` callback and clear. It propagates updated
  interpolation values into the position gate and preserves the possible
  second `0x439D30` call when the flag remains set.
- Added the shared `0x43E6F0` force/moment accumulator to the NCAR model as
  `accumulate_ncar_force_moment`. It preserves the strict near-zero gate,
  rounds the application arm through the observed float stores, and adds the
  vector and cross product into offset-based force/moment state. The helper's
  callers still supply the force vector, point, and center; full `0x431150`
  integration remains unreconstructed. `0x431130` is the adjacent forwarding
  wrapper. Evidence: `ev_049bb4485cc19293f077224e678d7b1b099b4715a61c0ee476057a2030461ca1`
  and `ev_8db8a1d5658a18c75d6ac745ac3cd8262d27a5255c639c9c24deeb0f3dbdb392`.
- Recovered the bookkeeping window in `0x431150` following the four
  `0x442530` slot updates. `advance_ncar_integrator_bookkeeping` models the
  signed counter reset at threshold 8, reports the `0x4323E0(0.128)` helper call,
  and applies the observed `+0xD70`, `+0xEB0`, `+0xD64`, and `+0xD68` stores.
  The indirect time callback and helper effects are inputs/observations.
  Evidence: `ev_2534fc83d0f6e0f4cea9397149195b61a7e7383fec5e886f81c33c3d9bc331f2`.
- Modeled the earlier `0x431150` embedded-child setup with
  `setup_ncar_integrator_child_control`: direct stores to child `+0x64` and
  `+0x5C` from car `+0xDE0` and `+0xDEC`, followed by the observed
  `0x43FAE0` call carrying car byte `+0x534`. Its controller is now modeled by
  `advance_ncar_animation_controller`, including six state branches, index and
  override transitions, the 0.128 ramp, ratio clamp, and indirect call
  argument stores. The `0x4402C0` and `0x43F8E0` targets are now applied as
  direct writes at target `+0x2C` and `+0x00`; exact Pentium FDIV remains
  unresolved.
  Evidence:
  `ev_1128dc7f406320fc5ef2f1ed4675611a89fdc97effbfc2c22b8811268e06d4ea`,
  `ev_498d3e18c8202f1635d22ed6fdab875a921c74ff081aee95b6a8369055328bf2`,
  `ev_e3e3d51d09604ce173a3a18906c819e7484977c6a91f49f3edb6d87a451e26bf`,
  `ev_755fef1cdeed6baa5aed78d2c0b845c438520258e465c0fdecdfbe67eaf5fe76`,
  `ev_067b6043dabac8d052f56b5bf9ca2171b08aa6ac561f4576487a76d38d304ad0`.
- Added three more physics primitives called by `0x442530`: point-relative
  linear/angular velocity (`0x43E9B0`), unconditional moment accumulation
  (`0x43E7F0`), and duration-scaled point impulse/moment accumulation
  (`0x43E830`). `accumulate_ncar_point_impulse` preserves the observed
  impulse-cross-arm sign order, which differs from `0x43E6F0`. The per-wheel
  state machine and ground query remain outstanding. Evidence includes the
  `0x442530` dossier plus pseudocode and instruction listings for each helper;
  full IDs are recorded in `metadata/car_update_branches.md`.
- Connected the already recovered tire-response stages in
  `compose_wheel_tire_response`: component equations, contact-normal cross
  product, uncorrected response vector, and basis projection. Its input keeps
  earlier wheel/contact and curve computations explicit, and force accumulator
  mutation remains delegated to the shared helper. Evidence:
  `ev_7031c5908694232f0bcb6e0042e737b8f91c507c42431b064e587de9795c5e95`.
- **2026-10-22:** Added the shared terrain surface query (`0x4672B0`/`0x467260`) with 0.001-unit quantization, partition traversal, plane height solve and feature-field extraction. Connected its successful result to the wheel-contact state machine (`0x45F760`/`0x442530`). Plane and partition data remain caller-supplied; x87/compatibility-division precision is not bit-exact. Evidence IDs are in `metadata/vehicle_setup.md`.
- **2026-10-22:** Composed the ordinary-path `0x439520` NCAR sample dynamics into one ordered state update: shift derivative history, evaluate both derivative vectors, apply the strict 9.81 magnitude cap, and update the interpolation ramp. The Pentium-FDIV compatibility branch remains unreconstructed. Evidence is recorded in `metadata/car_update_branches.md`.
- Corrected `escape_menu_selection_for_event`: `0x170..0x17A` select 0..10, `0x17B` does not store a selection, and `0x37B` selects 11. The prior model incorrectly treated `0x17B` as selection 11; the corrected branch split follows the `0x4022F0` dossier.
- Added `plan_race_loop_active_frame` for the observed `0x4020E0` active-frame call order, including state-indexed callbacks, state-4 render gating, context acquisition/submission, state-0 viewport reset, and the post-submit context check. Renderer and helper outcomes stay explicit inputs. Evidence: `ev_b26e9aad81c9f9a23aa344cd2b8cd64b866103eca50f4f93b9f2e93add7bc791`.
- Implemented `handle_state1_input_event` for the `0x41A370` callback: page-selection table cases, page-0/page-2 event routing, all recognized snapshot/restore/increment/decrement/reset stores, the temporary controller update call sequence, page-3 key translation gate, callback return, and conditional input-byte clear. Pointer-backed controller state and `0x4BDE40` translation output remain caller observations. Evidence IDs and raw field mappings are in `metadata/simulation_loop.md`.
- Added `scene_physics_wheel_bar_plans` for the four `0x4236F0` calls in the state-2 overlay. It models caller-derived positions, truncating fill width, fill/frame bounds, the observed 1.2 vector-normalization gate and indicator endpoint, and the numeric format argument. Render backend, colors, caller stack values, and exact x87/FDIV behavior remain explicit inputs or limits. Evidence is recorded in `metadata/simulation_loop.md`.
- **2026-10-22:** Added `classify_wheel_contact_feature` for the post-suspension branch in `0x442530`: raw contact-feature dispatch, animation-byte thresholds, and response factors. `apply_wheel_contact_feature_profile` now applies the observed animation-byte stores and clears `+0xEC`, `+0xF0`, and `+0x11C` on reset/inactive contact. Feature semantics and later force-accumulation paths remain open; no build or tests were run.
- **2026-10-22:** Connected the accepted event-`0x31` overlay toggle to the next `0x4020E0` active-frame plan in `plan_race_overlay_toggle_following_frame`. The toggled state now selects the callback row and feeds its state-specific render gates and context/submission path; all backend outcomes remain observations.
- **2026-10-22:** Added `scene_shared_paused_banner_plan`, composing `0x404970`'s raw paused predicate with the resource-generation refresh gate and `0x40C930`/`0x40C790` inner layout. Resource lookup, helper measurements, and backend submission remain observations; no build or tests were run.
- **2026-10-22:** Added `scene_shared_results_branch_plan` to compose the raw `0x4EB2F8` results gate with heading registration/refresh, nested ordinal-row selection, optional time lookup/formatting, and row draw arguments. Record/time buffers and measured values remain explicit observations; renderer submission is still external.
- **2026-10-22:** Composed both shared text-effect branches behind their exact common-suppressed raw predicates: `0x4048E0`/`0x4EB2FA` and `0x404900`/`0x4EB2F9`. Each retains its own resource-generation input and existing draw plan; the `0x404920`/`0x4EB2FB` resource branch uses a separate composer.
- **2026-10-22:** Added `scene_shared_centered_resource_plan`, deriving the `0x404920` call gate from common results/loading suppression and raw `0x4EB2FB`, then projecting the `0x4EB46C` centered resource draw. Surface dimensions and measured width remain observations.
- **2026-10-22:** Added `scene_shared_cheater_banner_plan`, composing the suppressed shared-overlay gate, either raw cheater byte, and the `<12` world-index limit with the recovered centered banner placement and draw-mode projection.
- **2026-10-22:** Added `scene_shared_loading_banner_plan` for the final `0x404970` splash branch, including `Splash:Loading` registration bit, generation refresh, and `0x40C930` draw. Added `scene_shared_overlay_frame_plan` to order and compose all seven post-dashboard shared predicates from one raw-global observation; callback setup and backend effects remain external.
- **2026-10-22:** Added the `viper_race_scene_flow` integration layer. `plan_race_loop_shared_overlay` carries the event-0x31 state transition through the active-frame callback list, identifies state-4 `0x404960` or state-0 `0x4052D0`, includes the state-0 `0x405100` geometry subplan when applicable, records wrapper mode, and only emits the ordered shared-tail plan when the callback is reached.
- **2026-10-22:** Extended `plan_race_loop_shared_overlay` to resolve the shared `0x4052E0` geometry callback from either state 0 slot `+0x08` or state 4 slot `+0x04`, and apply its existing callback geometry plan only when dispatch reaches that address. The active-frame call vector preserves each state's order relative to the `0x404970` wrapper.
- **2026-10-22:** Added `scene_dashboard_core_callback_plan` for the known common Position and mutually exclusive ordinary-Lap/final-time portions of `0x4035E0`. `plan_race_loop_shared_overlay` now recognizes state-0 callback slot `+0x00`, applies this composition only when dispatched, and projects its reached text-site descriptors. Descriptor order is not asserted as native execution order; LapTime and later dashboard rows remain separate. Evidence `ev_9aa46bc6450ec1572ea865ac9148625242d6d5ef645c945de5d2100e78982c22`; no build or tests were run.
- **2026-10-22:** Extended the core callback composition through the type-0/global-`+0x40`-false LapTime block at `0x40399A`. The composer now uses the common redraw result and incoming dashboard-kind cache to supply the LapTime redraw and prior-state-change gates and returns the helper's call plan and updated state. TestTrack and subsequent callback blocks remain outside the composition; no build or tests were run.
- **2026-10-22:** Connected the nonzero selected-type branch of the dispatched `0x4035E0` callback into the existing TestTrack plan at `0x403CA8`. It passes the common redraw state, derives the kind-change gate from the incoming cache, preserves the distinct record-`+0x00` displayed-position input, and applies TestTrack state changes to the callback's state snapshot. Supplemental and later metric rows remain uncomposed; no build or tests were run.
- **2026-10-22:** Extended the dispatched state-0 callback flow through its type-0 supplemental time pairs, then through both converged metric slots and the `0x404313` auxiliary readout. The two metric plans and auxiliary plan now update/project from the same ordered callback state snapshot; value and presentation dependencies remain observations. No build or tests were run.
- **2026-10-22:** Connected the shared wrapper dispatch to `scene_dashboard_gear_display_plan` ahead of the ordered shared-overlay tail. The `0x404970` second argument now determines its dashboard-path gate; state 0's wrapper enables the gear branch and state 4's disables it. Device/resource inputs remain observations; no build or tests were run.
- **2026-10-22:** Added the existing clipped full-surface fill write projection to the reached state-0 `0x405100` render path, so the integrated frame result exposes both its ordered render operations and projected fill writes when the redraw-all fill gate passes. No framebuffer mutation or build/test was performed.
- **2026-10-22:** Paired every reached state-0 `0x44AFD0` bitmap render operation with its clipping/mode blit projection and optional per-pixel write list, preserving the render-operation index. Source pixel bytes remain supplied data; destination memory is not mutated. No build or tests were run.
- **2026-10-22:** Added an ordered NCAR callback composition that applies the existing `0x431150` post-wheel bookkeeping only when the outer callback reaches the main integrator, before its timer tail. Wheel helper effects and bookkeeping inputs remain caller-supplied; no build or tests were run.
- **2026-10-22:** Composed the PCAR post-integrator record write and nearby-event scan behind the observed `0x431150` reachability gate, retaining their disassembly order and supplied backend/candidate inputs. No build or tests were run.
- **2026-10-22:** Connected the NCAR timed-queue prefix to the main `0x439200` input/mode and timer path through `advance_ncar_frame_callback`, sharing the top-level timestamp and event-processor result while keeping record identity and wheel outputs explicit. No build or tests were run.
- **2026-10-22:** Completed the PCAR post-integrator nearby cue path by applying the observed target bytes after the scan when a valid selected target span is supplied. No build or tests were run.
- **2026-10-22:** Resolved the wheel-resource render method boundary from vtable `0x4C9270` slot `+0x14`, then connected it through the scene-car constructor, global object registration at `0x45C2E0`, and generic renderer loop `0x45BEB0`. The method's original caller path is now established despite the missing Ghidra function entry. Evidence and gate details are in `metadata/scene_car_update.md`; no build or tests were run.
- **2026-10-22:** Recovered the scene-car constructor's wheel-resource pool sizing and added `scene_wheel_resource_pool_slot_count`: disabled at mode 0, otherwise one/four wheel groups times 16 slots, or 40 for the selected PCAR. Mode/selector globals and evidence are documented in `metadata/scene_car_update.md`; no build or tests were run.
- **2026-10-22:** Traced the pool mode to the parsed `skids` setting and confirmed the larger pool selector tracks the current PCAR ordinal. Evidence and sources are documented in `metadata/scene_car_update.md`; no build or tests were run.
- **2026-10-22:** Closed the wheel-resource slot lifecycle: releases write state 2 and clear the owner pointer; rendering includes state 2, allocation may reuse it, and manager reset clears it while preserving state 1. Replaced the lifecycle magic numbers with named state constants. Evidence is recorded in `metadata/scene_car_update.md`; no build or tests were run.
- **2026-10-22:** Corrected the scene-car vtable map: `0x465800` is slot `+0x24` and is called from change notifier `0x463530`; `0x463EB0` uses slot `+0x28`, whose target `0x4668D0` returns false. Removed the formerly caller-supplied callback result from the index model and made the observed false result authoritative. Evidence is recorded in `metadata/scene_car_update.md`; no build or tests were run.
- **2026-10-22:** Established the operational role of wheel-record `+0x34` bit 0: it selects the auxiliary resource update path when set and the release path when clear; false world gates and active suppression remain separate early-return conditions. Its high-level domain meaning remains unknown. No build or tests were run.
- **2026-10-22:** Added `should_dispatch_scene_car_update` for the `0x463530` changed-16-bit-field or forced-refresh gate before its virtual `+0x24` call. The update pointer source through `0x425880` is documented in `metadata/scene_car_update.md`. No build or tests were run.
- **2026-10-22:** Added `layout_scene_dashboard_text` for the font-backed `0x44DC70` stage. It computes byte-advance width, horizontal/vertical alignment, per-character glyph selection and x offsets for the dashboard's reached text calls. Captured wrapper and helper evidence in `metadata/scene_dashboard_text_layout.evidence.json`; raster backend effects remain open. No build or tests were run.
- **2026-10-22:** Added `raster_scene_dashboard_text` for `0x44D0C0`/`0x449D70`: atlas row clipping, transparent/literal runs, renderer-mode palette base, and projected 16-bit framebuffer writes. Missing or malformed atlas observations are surfaced in the result. Evidence is in `metadata/scene_dashboard_text_raster.evidence.json`; no build or tests were run.
- **2026-10-22:** Corrected the multiplayer post-step call graph (`0x40A400` → `0x40A600` → ordered `0x40A760`/`0x40A800`) and added `plan_multiplayer_post_step` for per-entrant `.car` resource releases, difficulty and track resource teardown, `race.res` release, and `ui.res` load. Thirteen evidence records are in `metadata/multiplayer_post_step_resources.evidence.json`; connection/lobby effects remain open. No build or tests were run.
- **2026-10-22:** Added `plan_multiplayer_frontend_detailed` to compose those resource operations with each active `0x40A230` poll iteration. Missing per-iteration inputs produce explicit null plan slots rather than fabricated resource actions. No build or tests were run.
- **2026-10-22:** Added `plan_multiplayer_telephony_initialization` for `0x49AEA0`: the 260 `CreateEventA` outcomes, `lineInitialize`, per-device API version `0x10004` negotiation, failure logging, and `0x5632CC` transitions are now explicit observations. Evidence is in `metadata/multiplayer_telephony_initialization.evidence.json`; TAPI/thread runtime effects remain external. No build or tests were run.
- **2026-10-22:** Added `plan_multiplayer_connection_object` for `0x49C620`/`0x49C670`/`0x49C950`: allocation, `LiveMultiInfo` registration request, worker enable, nonzero thread-ID acceptance, and failure cleanup/global clearing are represented. Communication setup via `0x485940` remains open. Evidence is in `metadata/multiplayer_connection_object.evidence.json`; no build or tests were run.


- **2026-10-22:** Added the `0x485940` direct/line communication setup branches, transport-object/UI gate, and caller retry cleanup projection. Also modeled `0x49AD40`/`0x49B080` telephony slot waits, shutdown, and handle closes from 260 slot observations. Evidence is in `frontend_dispatch.md` and `multiplayer_communication_setup.evidence.json`; socket/lobby behavior remains open. No build or tests were run.
- **2026-10-22:** Recovered the 123-byte worker callback fragment passed at `0x49BD40` and added `plan_multiplayer_worker` for its 250 ms sleep cadence, object checks, raw thread-table flags, and priority failure path. Helper bodies confirm `+0x18` is cleared before `SuspendThread`, while `+0x19` gates return; the higher-level meanings remain unknown. Traced the separate `0x49C2E0` status supervisor and its indirect `+0x84` callback path. Evidence is in `frontend_dispatch.md` and `multiplayer_worker.evidence.json`. No build or tests were run.
- **2026-10-22:** Added `plan_multiplayer_status_supervisor` for `0x49C2E0`'s identity-cleanup outcome, path-sensitive `5..16` and `17` loops, communication-ready polling, terminal return-byte branches, and status-`0x0F` string-copy size/capacity projection. State names and callback effects remain unresolved. Evidence is in `multiplayer_worker.evidence.json`; no build or tests were run.
- **2026-10-22:** Recovered all 22 `Race's ChatLine` status labels from `0x4A1680` and added a stable label-ID mapper. Connected virtual `+0x84` to its `0x1C8 + state * 0xD9` status-byte lookup; status 15 is `PGS_RACE_BEGIN`, and its virtual `+0x50` transition writes state 16 (`PGS_SYNCHRONIZING`). Other protocol transitions remain open. Evidence is in `multiplayer_protocol_states.evidence.json`; no build or tests were run.
- **2026-10-22:** Reconstructed the shared panic reporter `0x411000`/`0x411020`: prefixed variadic message, 255-byte local buffer with adjacent logging gate byte, module logger route, last-resort MessageBox route, and the `0x416020` panic-flag/null-write tail. Source-shaped notes are in `src/engine_panic_reconstruction.c`; 18 evidence records are in `metadata/engine_panic.evidence.json`. Process-level handling of the null write remains unknown. No build or tests were run.
- **2026-10-22:** Modeled the `0x40A850` single-race participant builder and `0x40A5D0` post-read setup stage: configured row count, driver-order player-slot search, human/AI row sources, ordered resource operations, and per-row initializer routing. Added `build_single_race_entrants` and `plan_single_race_setup_post_read` in `race_participants.hpp/.cpp`. The random-track selection and lower-level per-car effects remain unmodeled. Evidence is in `metadata/single_race_entrant_plan.evidence.json`; no build or tests were run.
- **2026-10-22:** Added `plan_single_race_setup_screen` for `0x480C80`: accepted car/track outputs, case-insensitive `random` track selection using the observed timer value modulo track-count minus two, arithmetic fault cases, and ordered typed setting writes. The platform timer backend and selected track contents remain inputs. Evidence is in `metadata/single_race_setup_screen.evidence.json`; no build or tests were run.
- **2026-10-22:** Added `plan_vehicle_initializer_wrapper` for `0x45EFD0` and `0x41A000`, capturing the human 0x8C-byte helper/copy and AI fallback-record, lookup, initialization, and scalar-write order. Configuration contents and lower-level vehicle effects remain open; no build or tests were run.
- **2026-10-22:** Extended the single-race vehicle setup model into `0x41A0C0`: resource operation order, seven fallback coefficient bit patterns, count-dependent table selection and loop, and the `0x4E9754` division-helper branch. Runtime coefficient operands remain inputs; no build or tests were run.
- **2026-10-23:** Decompiled `0x41A0C0` directly from the shipped `race.bin` and extended `plan_ai_vehicle_configuration` to calculate coefficient outputs from recovered stack locals and embedded the two observed binary32 tables. Added the source-derived formula, first-entry `39.0` divisor, exact-five table selection, and the existing VC98 FDIV approximation for both guarded divisions. Exact x87/helper operation fidelity remains open. Evidence is in `metadata/single_race_ai_config.evidence.json`; no build or tests were run.
- **2026-10-23:** Traced the coefficient operands to the `0x45E2F0` car-file parser: it accepts version 6, copies 0x228 bytes, and the `0x41A0C0` call disables optional upgrade overlays. Converted Ghidra stack locals to record offsets relative to the copied buffer (`local_228`); the count is at +0x108, and formula fields are at +0x1B4, +0x1B0, +0x1AC, +0xCC, +0xC8, and +0x4C. `plan_ai_vehicle_configuration` now consumes the existing `CarFilePayload` projection; resource/chunk outcomes are represented by `plan_car_file_read`, while resource-manager internals and UPGS source bytes remain inputs. No build or tests were run.
- **2026-10-23:** Corrected `parse_car_file_payload` to match the observed fixed 0x228-byte copy after the no-op `0x45E470` size check: version-6 payloads with surplus bytes are accepted and retain a size-mismatch observation; undersized spans remain safely unparseable. Updated the resource-path notes with the shared UPGS loader/application behavior. No build or tests were run.
- **2026-10-23:** Added `plan_car_file_read` for the native `0x45E2F0` path, projecting resource-open failure, chunk lookup, wrong-version reporting, no-op size-check ordering, fixed copy, optional UPGS branch, and release reachability from supplied outcomes. Loader behavior and upgrade values remain observations. No build or tests were run.
- **2026-10-23:** Added `plan_car_file_upgrades` for `0x45ED10`/`0x45E480`: version and panic gates, relocation-branch observation, per-set selection bytes, and ordered `(offset, value)` writes. Out-of-payload destinations are preserved as observed write intents and flagged. Added `apply_car_file_upgrade_plan` for complete in-bounds payload writes. UPGS resource bytes and panic continuation remain unresolved. No build or tests were run.
- **2026-10-23:** Added `parse_car_file_upgrade_payload` for the bounded relative UPGS representation: the set count/pointer table, `+0x404` data base, per-set `+0x6C` count, and `+0x70` offset/value pairs. It produces inputs for `plan_car_file_upgrades`; absolute-pointer resources remain unavailable from a portable raw span. No build or tests were run.
- **2026-10-23:** Added `make_car_file_resource_names` for the `.car`/`.cf`/`.ugs` naming observed in `0x41A0C0` and `0x45E2F0`. Inspected the extracted car bundles: only `viper.car` contains UPGS (version 1, 5,948 bytes, 33 sets), and its relative set pointers resolve within the payload using the recovered `+0x404` base. Added the sample inventory and SHA-256 to the metadata; no game assets were copied into the source repo.
- **2026-10-23:** Connected RST0 bundle lookup to the CARF/UPGS parsers with `parse_car_file_resources`, then added `--inspect-car BUNDLE STEM` for a direct summary of a car resource and its upgrade sets. The CLI uses the recovered relative UPGS pointer mode. No build or tests were run.
- **2026-10-23:** Added `prepare_car_file_runtime_mapping` to carry bundle-parsed CARF through an optional bounded UPGS overlay and into the `0x45E4B0` runtime-field mapper. It preserves the source payload size observation and leaves missing resources, incomplete selections, unsupported versions, and unsafe writes incomplete. No build or tests were run.
- **2026-10-23:** Added `apply_car_track_runtime_record` to compose CARF mapping with the `0x45CAE0` subtype tag, final record-size store, and placement transform while preserving bytes outside observed stores. No build or tests were run.
- **2026-10-23:** Added `prepare_car_file_runtime_mapping_from_configured_row` to connect a bounded `0xC8` participant row directly to CARF/UPGS runtime preparation, including extraction of the 35 words at `+0x34`. This removes manual adapter wiring while retaining resource and environment inputs as observations. No build or tests were run.
- **2026-10-23:** Recovered the terrain resource path from `0x4630B0` through `0x467060`/`0x467130`: `track.bpp`, TPPB version 2, a 20-byte header, 0x38-byte planes, and 0x1C-byte partition nodes with child-index fixups. Added a bounded payload parser, bundle lookup, and sampler-input adapter in `terrain_surface`. The extracted `uptown.trk` resource has 12,092 planes and 43,078 nodes. Runtime comparison and exact x87 behavior remain open; no build or tests were run.
- **2026-10-23:** Added `--inspect-terrain BUNDLE` to expose parsed TPPB plane/node counts and the root index from an extracted track bundle; no build or runtime validation has been performed.
- **2026-10-23:** Traced terrain consumers from `0x45F600` into vehicle placement, checkpoint/obstacle setup, and camera transform `0x4282E0`; confirmed camera mode `0x421120` and wheel update `0x442530` use the full `0x45F760` point/normal/feature path. Added `sample_terrain_resource` and `terrain_height_or_zero` to connect parsed TPPB data to those call patterns. The caller-computed query coordinates remain explicit inputs.
- **2026-10-23:** Composed the accepted `0x442530` wheel collision response through its conditional `+0x20` damage flag and `0x43E830` point-impulse accumulator writes in `apply_wheel_collision_impulse`. Inputs remain the recovered normal-response operands and raw query gate; host arithmetic limitations remain unchanged. Evidence: `0x442530` `ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`.
- **2026-10-23:** Added `advance_body_integration_kinematics` to compose the recovered `0x43ECE0` linear velocity/position step, angular transform and cap, orientation update, basis synchronization, and accumulator/counter tail. Earlier force collection, virtual record callbacks, and two stack-dependent matrix words remain observations; score unchanged.
- **2026-10-23:** Added `consume_body_point_impulse_accumulator` to connect wheel collision point impulses to the six force/moment accumulators consumed by `0x43ECE0`, including duration epsilon, scale `62.5`, and source clears. The embedded `0x4473D0` record callbacks and the separate seven-float virtual-record path are still outside the reconstruction.
- **2026-10-23:** Added `materialize_body_integration_virtual_record_calls` for bounded extraction of the separate seven-float callback records at `0xAC`, gated and normalized by the same duration. It models the arguments to vtable `+0x38`, not the virtual implementation; embedded `0x4473D0` behavior remains outstanding.
- **2026-10-23:** Implemented `transform_body_local_point` for the `0x437BA0` basis-and-center mapping used inside embedded record handler `0x4473D0`. The existing `transform_ncar_vector_at_point` covers its point-velocity stage at `0x43E9B0`; terrain contact and the virtual response remain to be composed.
- **2026-10-23:** Added `prepare_embedded_record_contact`, reconstructing `0x4473D0`'s penetration-based spring term, feature-`0x0E` speed damping, and other-feature tangent-friction cap before the virtual car callback. Query wiring and callback side effects remain unresolved.
- **2026-10-23:** Captured the subsequent large-force effect gate in the embedded-contact result: non-`0x0E` friction magnitude strictly above `50000` requests `0x4361E0`; global rate limiting and event output are not modeled.
- **2026-10-23:** Connected successful `TerrainSurfaceQueryResult` data to embedded-record contact inputs with `make_embedded_record_contact_input`; the exact query coordinates and record transforms still come from caller observations.
- **2026-10-23:** Added `finish_embedded_record_update` for `0x4473D0`'s optional body-center copy into local record position, cached transformed point, and hit/miss penetration-field writes.
- **2026-10-23:** Added `prepare_body_integration_force_collection` for the observed gravity, embedded-record accumulator effects, duration-gated point-impulse transfer, normalized virtual-record call arguments, and count/duration clears before kinematics. Corrected the impulse transfer to clear its six sums only when the duration epsilon gate passes.
- **2026-10-23:** Resolved PCAR/base and NCAR physics-car virtual slot `+0x34` targets for `0x4473D0`; both lead to `0x430560`, which updates the speed-gated energy accumulator and calls `0x43E6F0`. Added `apply_embedded_record_force_to_body` to model those effects and confirmed the input force vector remains unchanged.
- **2026-10-23:** Added `update_embedded_record_contact` to join the recovered `0x4473D0` point, terrain result, point-velocity, contact-force, virtual accumulator, and record-tail stages. The original terrain-query stack-coordinate setup remains a caller-provided result.
- **2026-10-23:** Configured and completed the full CMake `Release` build with the locally installed Visual Studio 18 toolchain. Fixed the missing PCAR `+0xDE0` tail field and added the vehicle-to-NCAR library dependency required by the newly composed force path. The executable is a source-analysis tool; no game runtime behavior was tested.
- **2026-10-23:** Reconstructed the segment query used by `0x45F700`: start-side-first partition traversal, split-crossing alternate branch, plane intersection, triangle containment, and feature/normal output. Added `update_embedded_record_contact_from_terrain`, which forms the callsite segment from record `+0x20` with Y+10 to transformed record `+0x00`, queries parsed track terrain, then composes contact forces and state writes. Runtime predicate and edge behavior remain unvalidated. Full Release build succeeded.
- **2026-10-23:** Refined the terrain segment query against direct dossiers for `0x466EB0`, `0x466C10`, and `0x466DB0`: corrected zero-side traversal order, restored the native edge-cross-product triangle predicate, and removed intersection clamping while matching the observed range gate. Exported the nine-record bundle as `terrain_segment_query.evidence.json`; the full Release build and `git diff --check` succeed. Runtime behavior remains unvalidated.
- **2026-10-23:** Replaced the arbitrary fixed-capacity candidate buffer with recursive start-first traversal matching `0x466EB0`'s primary-then-crossing-side fallback. Routed segment-plane division through the shared VC98/Pentium FDIV compatibility approximation using the observed compatibility flag. Updated the build documentation; CMake reconfiguration, full Release build, and `git diff --check` all succeed.
- **2026-10-23:** Added `plan_embedded_record_auxiliary_effect` for `0x436030`'s type-1 `0x4D2` payload, copied vector, force-magnitude scale cap, and optional `+0x0C` callback gate. Kept its vector arguments neutral because the `0x4473D0` call pushes a null first argument while `0x4361E0` reads `param_1 + 4`; the outer effect queue remains unresolved pending ABI/data-flow resolution.
- **2026-10-23:** Modeled the optional `0x435FE0` follow-up state writes in `apply_embedded_record_auxiliary_effect`, including vector cache, changed-scale gate, and three flag writes. Exported its direct dossier in `embedded_record_effect_followup.evidence.json`.
- **2026-10-23:** Added `append_embedded_record_effect_event` for `0x4273C0`: serializes the type byte plus `0x4D2` payload, reproduces the 0x600-byte stream capacity gate and used-byte advance, and models overflow warning throttling using caller-supplied tick observations. `0x426BB0` was confirmed as a no-op; the active ring buffer selection remains explicit. Direct evidence is in `embedded_record_event_stream.evidence.json`; Release build and `git diff --check` succeed.
- **2026-10-23:** Expanded the race-loop model with `plan_race_loop_simulation_step` for the nested `0x425920` sequence: bracketing calls, clock read, `[0.01, 0.25]` delta clamp, `0x43B290` update, time-anchor store, and `0x46B620` timer-object update. The two opaque objects and current time remain inputs. Evidence is in `race_loop_simulation_step.evidence.json`.
- **2026-10-23:** Added `plan_race_loop_periodic_update_dispatch` for the front/tail of `0x43B290`: `0x509424` accumulation, 1/18-second reset gate, unconditional `0x444E10` tick-flag call, and conditional `0x4170A0` three-DWORD dispatch. Composed it with the outer time clamp in `plan_race_loop_simulation_frame`. The central global-state updates remain open. Evidence is in `race_loop_update_43b290.evidence.json`.
- **2026-10-23:** Recovered the `0x446980` input-binding mappings and `0x446F40` activation flags (finite values above `0.2`; unordered values follow the native true path), then modeled the steering accumulator and both adjacent analog channels in `0x43B290`. Corrected the prior tail model to use signed bitwise float-word checks and the integer counter bound seen in assembly. Direct evidence: `race_loop_update_43b290.evidence.json` and `race_input_binding_accessors.evidence.json`; details: `simulation_loop.md` and `race_input_bindings.md`. Full Release build and `git diff --check` succeed; no tests were run.
- **2026-10-23:** Added `update_race_loop_43b290` to compose the cadence gate and observed calls with the recovered steering, analog-channel, and control-tail transitions, sharing the simulation delta through the frame update. Input samples, binding record contents, and platform effects remain explicit observation boundaries. Full Release build and `git diff --check` succeed; no tests were run.
- **2026-10-23:** Added `sample_race_loop_bindings` for the 15 records consumed by `0x43B290`, including selector-indexed type-1 button samples and a shared value/active/ramped-path snapshot. Renamed type-gate fields to describe the observed ramped-update branch without assigning an unsupported input-domain meaning. Full Release build succeeds; no tests were run.
- **2026-10-23:** Wired the 15 binding records into `update_race_loop_43b290`; it now chooses the supplied post-`0x444E10` snapshot according to the cadence result, derives binding values/type/active flags, and applies them to steering, both analog channels, and the control tail. Full Release build and `git diff --check` succeed; platform input polling remains an explicit boundary.
- **2026-10-23:** Added `plan_race_loop_outer_iteration` for the outer `0x4020E0` loop, preserving its two counters, timeout/startup branches, exit-page route, optional side branch, page dispatch, frame plan, second active-state check, and return gate. Modeled direct global effects from `0x49BEA0`, `0x425800`, `0x4035D0`, `0x425710`, `0x4029C0`, and `0x4259B0`; opaque callback outcomes remain observations. Evidence: `race_loop_outer_iteration.evidence.json`; full Release build and `git diff --check` succeed. No tests were run.
- **2026-10-23:** Added `plan_escape_menu_initialization` for `0x4022F0`'s four label/cleanup registrations, eight one-time initialization bits, saved-page store, and per-button-group localization refresh gates. The outer-loop plan now includes this subplan on its timeout-menu branch; descriptor writes and localization return values remain open. The Release build and `git diff --check` pass.
- **2026-10-23:** Added `plan_escape_menu_event` and wired it into the `0x4022F0` timeout branch. It preserves input/render/callback/key-handler order; models page events, key `0x31`, player operations `0x121`/`0x122`/`0x124`, and single-/multiplayer Escape routes; and carries the dirty flag to the same iteration's `0x4029C0` gate. Callback and platform outcomes remain observations. Full Release build succeeds; no tests were run.
- **2026-10-23:** Connected timeout-menu event results to the current iteration's page dispatch and frame callback state, including the event-`0x31` overlay dirty flag and state-row change. Opaque callback page/state outputs remain optional observations. Full Release build and `git diff --check` succeed; no tests were run.
- **2026-10-23:** Tightened the escape-menu callback model against helper evidence: `0x4018F0` consumes low-word events `0x50`/`0x70`; `0x402960` sets the startup byte through `0x4035D0` and refreshes resources through `0x402060` without setting `0x4EB20C`; Escape action `1` sets the exit latch through `0x402A40`. Corrected the key-`0x31` dirty-flag assumption and retained the helper dossiers in `escape_menu_callback_helpers.evidence.json`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Recovered all four once-only escape-menu button descriptor rows in `0x4022F0`: 13 DWORDs per row at `0x4CF640`, `0x4CF678`, `0x4CF6B0`, and `0x4CF6E8`, with localized text handles kept as explicit inputs and callback/style words modeled from observed writes. Menu initialization now returns these descriptor projections in addition to localization calls and guard bits. Binary evidence is in `race_loop_outer_iteration.evidence.json`; the compact multiplayer-interface follow-up is in `multiplayer_interface_followup.evidence.json`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Resolved the `Race's ChatLine` vtable `+0x84` target: table `0x4CB5A0` points to callback bytes at `0x4A1B70`, which returns `this[0x1C8 + status * 0xD9]`; its `0x4A1AF0` helper is a `RET` stub. `plan_multiplayer_status_callback` now reads the indexed byte directly from a bounded caller-supplied object span instead of accepting the already-decoded byte. Evidence: `multiplayer_chatline_status.evidence.json`. Release build and `git diff --check` pass without warnings; no tests were run.
- **2026-10-23:** Extended advance_four_wheel_body_step to apply each wheel update supplied 0x43E7F0 vector to the shared body moment accumulator before that wheel later collision impulse, preserving 0x1A4-stride order. The wheel-local vector derivation remains an explicit observation. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Modeled the state-15 send gate from bounded `0x1C`-byte connection rows: the selected row's `+0x19` bit 0 controls the sender, and `+0x18` supplies message byte 2. The plan records the known message prefix, 0x34-byte length, target at row `+0x0C`, and nested sender chain through `0x4A8BE0`, `0x4A8D90`, and `0x4A9C40`; socket allocation and queue outcomes remain unknown. Evidence: `multiplayer_status_send.evidence.json` and `multiplayer_status_transition.evidence.json`. Release build and `git diff --check` pass without warnings; no tests were run.
- **2026-10-23:** Extended advance_four_wheel_body_step to apply each wheel update supplied 0x43E7F0 vector to the shared body moment accumulator before that wheel later collision impulse, preserving 0x1A4-stride order. The wheel-local vector derivation remains an explicit observation. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Made the packet-delivery branch executable in the portable model. The plan now accepts allocator success, pending-queue presence, and the observed sender sequence bytes, then selects immediate dispatch exactly when the queue is empty and the 8-bit sequence window is open; otherwise it records the queue route. Missing sender-state observations remain explicitly incomplete. The packet bundle retains the helper evidence. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Composed the four wheel contact/suspension projections in the `0x431150` order, retaining the observed `0x1A4`-byte slot stride and post-step contact state. Remaining wheel dynamics and body-accumulator side effects stay separate. Release build succeeds; no tests were run.
- **2026-10-23:** Added a four-wheel collision-impulse composition that carries per-wheel damage flags and accumulates accepted impulses into the shared body point-impulse state in slot order. This joins `0x442530` collision gates to `0x43E830` accumulation and the existing `0x43ECE0` consumer. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `advance_four_wheel_body_step` to connect the four-wheel collision inputs, body force collection, explicit virtual-record effects, and `0x43ECE0` kinematics/tail in observed order. This links the reconstructed impulse accumulator to the body step while preserving unknown callback outputs as input. Release build succeeds; no tests were run.
- **2026-10-23:** Added a bounded `0x43ECE0` embedded-record batch pass: it updates the observed count of 0x30-byte records in order, queries parsed terrain per record, and accumulates force and energy effects across the batch. Short state/point spans report unprocessed rows without out-of-bounds reads. Release build succeeds; no tests were run.
- **2026-10-23:** Wired that embedded-record batch into `advance_four_wheel_body_step`: preexisting wheel force/moment sums and each record's terrain response now feed the same `+0x24C/+0x258` accumulator, which is transferred into `0x43ECE0` force collection before virtual callback observations and kinematics. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected the `+0xD78` eighth-call gate to `0x4323E0` inside `advance_four_wheel_body_step`, carrying four wheel animation timers and two vehicle animation channels through the periodic update before embedded-record and body integration. Other interleaved post-wheel effects remain open. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended advance_four_wheel_body_step to apply each wheel update supplied 0x43E7F0 vector to the shared body moment accumulator before that wheel later collision impulse, preserving 0x1A4-stride order. The wheel-local vector derivation remains an explicit observation. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Added advance_ncar_post_body_tail for the numeric/direct tail after 0x43ECE0: clears +0x37F..+0x381, applies the observed +0x382 vector offset, updates the three +0x3A9 angular accumulators through the strict axis gate, and models the contact-gated +0x38C speed maximum. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Added the mode-0/mode-1 force branch from 0x431150 to the composed body step. It preserves raw wear gates, four-contact count, leading wheel-damage gates, forward-projection thresholds, and 0x43E6F0 accumulation; call-site force inputs remain explicit. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Resolved the post-body 0x433A00 gate as a four-bit signed-DWORD mask over wear fields +0x670/+0x814/+0x9B8/+0xB5C at threshold 0x3FB9999A, then wired its result into the +0x38C maximum-speed store. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Composed the 0x431150 two-second timeout gate into the post-body stage and applied 0x433920 direct field resets and four wheel-damage-byte clears; resource-group copying and backend effects are still reported as opaque. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Reconstructed the five 0x433920 resource-group passes: bounded 0x20-stride copies of each row first three DWORDs and the 0x44F9C0 flag-4 descriptor/byte stores. Short spans report missing records. Release build and git diff --check pass; no tests were run.
- **2026-10-23:** Added the post-mode paired wheel differentials from 0x431150, including the two pairwise contact-byte gates and the signed stores at +0x850/+0x6AC and +0xB98/+0x9F4. Release build and git diff --check pass; no tests were run.
# 2026-10-23 continuation: reconstructed the next 0x431150 force block at 0x431A08..0x431C28. The 2+ contact path applies component-wise quadratic resistance in transformed velocity space and rotates it back; the lower-contact path projects the velocity delta onto three body axes and computes the weighted resistance magnitude. The resulting force is accumulated at the recovered application point. Matrix/vector helper outputs and raw scale fields remain explicit inputs. Full Release build and git diff --check pass; no tests were run. Remaining force/energy blocks after 0x431C28 are still open.

- **2026-10-23:** Added the two velocity/projection force applications after the common resistance call in `0x431150` (`0x431C2D..0x431DDD`). Each combines speed-scaled damping with the squared projection onto the body axis, accumulates at its own transformed point, and contributes its Y force to the per-step `+0xEB4` energy value. Transform outputs and raw field scales remain explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reconstructed the conditional four-object update before `0x43ECE0`: four `+0x38` target stores are calculated from `+0x500/+0x508`, and `0x440B40`'s capped slip/resistance force is evaluated per object and added to the body force/moment accumulator. Relative-velocity and basis helper outputs remain explicit. Evidence: `car_update_branches.md`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Wired the two consecutive tire-force accumulator calls from each `0x442530` update into `advance_four_wheel_body_step` after the same wheel's collision impulse. This uses supplied composed tire responses, application points, and secondary-force parameters. The mode-zero conditional third force call and upstream wheel inputs remain unresolved. Evidence: `vehicle_setup.md`, `ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the third conditional `0x43E6F0` force from each `0x442530` call to the four-wheel composition. It follows the two unconditional calls and preserves the wheel `+0x168` and car-mode `+0x4F4` gates, bounded tire-scalar weight, corrected-vector projection, speed gate, and center application point. Remaining upstream scalar/basis derivation is explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Modeled the `0x442530` direct tire-state tail after the third force: ORs the conditional `+0x120` flag bit, increments `+0x124` from explicit intermediate response values, and derives `+0x128` using 9.549296. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Consolidated the per-wheel path: `advance_four_wheel_body_step` now computes the observed basis update, feeds its corrected vector into tire-force calls, and passes its updated response scalar plus collision-updated damage flag to the existing `finalize_wheel_rate_state`. This removes duplicate tail inputs and preserves caller observations for the unresolved upstream response derivation. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended the integrated tire-vector stage to combine recovered forward/lateral response factors and build the lateral vector from the contact-normal/forward-basis cross product before applying the 1e8/10000 normalization cap. Factor inputs and forward/contact bases remain observations. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected `advance_wheel_tire_dynamics` inside each per-wheel body update and used its trigonometric response as the corresponding tire-vector factor, matching the observed call before tire-force accumulation. Its instantaneous inputs and remaining response factors remain explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Documented the remaining `TireWheelResponseFactors` against their exact `0x442530` decompiler locals (`0x74`, `0x9C`, `0xE8`, `0xD4`, `0xE0`, `0x70`, `0x98`, `0xC4`, `0x50`, `0x4C`), clarifying which inputs still need upstream composition. No behavior changed.
- **2026-10-23:** Derived the tire-curve response factors inside the per-wheel path, including the signed/clamped `0x4356D0` slip correction on the forward curve term, plus the observed `+0x98`, `+0x94`, and mode scales. Geometric ratios and direction terms remain explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected the two wheel-relative velocity ratios to `derive_wheel_kinematic_ratios` inside tire-factor construction, using explicit projected velocities and clamped reference speed. The recovered arithmetic is wired; semantic factor labels remain provisional. Release build verification follows.
- **2026-10-23:** Derived the tire reference-speed clamp from the longitudinal projection and mode scale within response-factor construction, then reused the clamped value and low-speed weight for both ratios and curve response. The mode and velocity inputs remain explicit. Release build verification follows.
- **2026-10-23:** Derived the bounded lateral direction adjustment from the scaled quadratic correction and signed slip response, then applied the sign of the forward ratio/adjusted response as in `0x442530`. A zero divisor preserves the supplied observation. Release build verification follows.
- **2026-10-23:** The wheel-ratio, reference-speed clamp, tire-curve factor, and bounded direction-adjustment composition now compiles in the Release build; no tests were run.
- **2026-10-23:** Replaced the supplied tire curve input scalar with the source's three-term contact-normal projection, `max(1, projection)`, and division by wheel `+0x90`. The component terms and divisor remain explicit. Release build verification follows.
- **2026-10-23:** The `+0xC4` curve-input projection change builds successfully in Release; no tests were run.
- **2026-10-23:** Added an optional per-wheel contact/suspension pipeline to `advance_four_wheel_body_step` between moment accumulation and collision impulse. Valid query contact normal/depth and finalized suspension force now feed the corresponding collision and `+0xC4` curve inputs. Release build succeeds; no tests were run.
- **2026-10-23:** Connected `advance_wheel_animation_basis` after each collision and before tire-response assembly. Its collision-updated damage flag and contact-pipeline depth are used, and its computed `+0x110..+0x118` basis feeds the response projection. Remaining orientation/body-basis inputs stay explicit. Release build verification follows.
- **2026-10-23:** The `0x443F60` basis integration compiles with the four-wheel composition in the Release build; no tests were run.
- **2026-10-23:** Connected the finalized suspension vector to the `0x431130`/`0x43E6F0` force-moment accumulator after `0x443F60`, using the transformed wheel `+0xC8` point; the same point now feeds tire forces. The accumulator's vector-length gate is retained. Release build verification follows.
- **2026-10-23:** The suspension force-at-point integration builds successfully in Release; no tests were run.
- **2026-10-23:** Extended the `0x442530` tire response derivation to produce pre-smoothing wheel-dynamics terms `+0x2C` and `+0x30` from the decoded curve/ratio intermediates, and wired them before `advance_wheel_tire_dynamics`. Follow-up analysis of `0x4356B0`/`0x4356D0` corrected the forward curve factor and denominator, derived/clamped the lateral correction at `+0x70`, used it to form the `+0xE0` direction factor, and derived `+0xE4` as `+0x30/+0xE0`. Exact signed-division flag behavior, unusual NaN/zero paths, and the physical source mapping of the slip input remain unresolved. Release build verification follows; no tests were run.
- **2026-10-23:** Added `derive_body_point_velocity_0x43e9b0` from the exact `0x43E9B0` helper pseudocode and connected it to the optional four-wheel contact path. That path uses the updated wheel `+0x130` point and pre-integration body kinematics, projects point velocity onto the supplied forward direction and contact-normal cross direction, and derives longitudinal, lateral, and rolling slip inputs. Release build verification follows; no tests were run.
- **2026-10-23:** Added optional raw wheel spin `+0x124` and scalar `+0x5C` fields to the tire-curve inputs; the composed contact path now derives rolling surface speed from their product, matching the ratio numerator in `0x442530`. Callers must supply the spin value after the earlier in-function spin update; otherwise the existing rolling-speed observation is retained. Release build verification follows; no tests were run.
- **2026-10-23:** Reconstructed the pre-contact spin block in `0x442530` as `update_wheel_spin_preprojection`: applies damage-sensitive response scaling, low-speed capping, the `+0x17C` half-step filter, `+0x12C` auxiliary store, sub-9 spin damping of `+0x124`, and the gated `+0xE4/+0x5C` response contribution to `local_90`. The four-wheel composition derives the optional body-speed attenuation projection from body state; later tire-vector projection and x87 behavior remain open. Release build verification follows; no tests were run.
- **2026-10-23:** Corrected the wheel-rate tail model after confirming the final `+0x124` increment consumes evolved `local_90`, not the tire-basis `+0x7C` result. Renamed the finalizer input and removed the four-wheel `+0x7C` substitution; when spin preprojection is enabled, its reconstructed `local_90` now feeds the tail. Release build verification follows; no tests were run.
- **2026-10-23:** Reconstructed the tire-vector portion of the final `local_90` update: after the response vector's observed magnitude cap, subtract its dot product with the wheel-forward axis before the `+0x124` rate increment. The intermediate `local_90` entering this projection remains an explicit observed input; the postprojection value is returned. Release build verification follows; no tests were run.
- **2026-10-23:** Corrected the wheel-rate blend field to source local `+0x28` and connected the preprojection spin block's computed blend to the final `+0x124` increment whenever that block is enabled. This removes an independent blend observation in the composed path. Release build verification follows; no tests were run.
- **2026-10-23:** Derived the gated spin block's high-speed projection from body basis `+0x50..+0x58` dotted with linear velocity `+0x234..+0x23C`, replacing its raw observation in the four-wheel composition. Routed the wheel point-velocity wrapper through the shared `transform_ncar_vector_at_point` implementation to keep `0x43E9B0` behavior centralized. Release build verification follows; no tests were run.
- **2026-10-23:** Split the spin filter's two similarly named source values: raw wheel `+0xE0` scales its base response, while `max(abs(+0x178), 444.99997)` feeds the `+0x54/+0x58` filter terms. Added the `+0x164` selector override with its positive-`+0xE0`, clear-damage, and initial `+0xA8 > 1` gates. Release build verification follows; no tests were run.
- **2026-10-23:** Wired the preprojection spin block's prior-frame `+0x170`, `+0x174`, and `+0x178` values from the existing `WheelTireDynamicsState`, removing duplicate caller observations for these source fields in the composed path. Release build verification follows; no tests were run.
- **2026-10-23:** Completed preprojection state flow: read/write the `+0x17C` half-step filter through `WheelTireDynamicsState`, write the `+0x12C` auxiliary value into wheel animation state, and use the damped `+0x124` spin as the final rate increment's base. Release build verification follows; no tests were run.
- **2026-10-23:** Added `derive_wheel_moment_vector_0x43e7f0` for the early `0x442530` moment vector. It composes `0x422F30` body-basis transformation, wheel-axis cross car-axis, current/prior spin filtering, and the `+0x34 * 12.5` scale. The optional four-wheel path runs it after spin preprojection and accumulates the resulting vector before contact processing. Wheel-local `+0xB0`, `+0x13C`, and `+0x140..+0x148` remain raw inputs. Release build verification follows; no tests were run.
- **2026-10-23:** Preserved the `0x443F60`-updated wheel `+0xB0/+0xB4/+0xB8` basis as `wheel_moment_local_axes_next_0xb0` in the four-wheel result so callers can feed the correct prior-frame axis into the next pre-contact `0x43E7F0` calculation. Release build verification follows; no tests were run.
- **2026-10-23:** Exposed `0x442530`'s `+0x13C = current +0x124` store as `WheelMomentVectorResult.next_spin_rate_0x13c`, alongside the computed early moment vector, so callers can preserve the wheel's prior-spin state across steps. Release build verification follows; no tests were run.
- **2026-10-23:** Connected the adjusted suspension response to wheel state `+0x178` after preprojection and before tire-dynamics reset, preserving the observed prior-frame read/current-frame write order. The existing clear-filter branch can still zero the field afterward. Full Release build and `git diff --check` pass. No tests were run.
- **2026-10-23:** Wired `advance_wheel_tire_accumulators` into the four-wheel step after tire dynamics and before response-vector/body-force work. It now consumes current `+0x11C` and, when contact updates are enabled, current `+0x168`; accumulator snapshots, response locals and global scales remain explicit. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Composed the optional `0x442530` entry prefix before spin preprojection: its `+0xA8` drives the selector gate, its wrapped `+0xAC` feeds the wheel animation-basis update, and its distance gate can update damage state before collision handling. Raw query geometry and thresholds remain explicit. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reused the contact transform's referenced point and wheel position for the entry-prefix distance gate when both stages are enabled, avoiding a duplicate geometry observation. The query gate and threshold remain explicit. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reconstructed the tire accumulator's `local_0x38` and `local_0x2C` from the evolved response factors after tire-dynamics smoothing. The four-wheel accumulator stage no longer needs those two duplicate raw observations; prior accumulator values and global coefficients remain inputs. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `WheelTireAccumulatorState` to carry `+0xA4/+0x10C/+0xEC/+0xF0` between composed four-wheel steps, removing manual field-by-field state copying. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected the contact-feature dispatch to persistent `+0xEC/+0xF0` and prior `+0x11C` state, then propagated its reset results into the subsequent tire-dynamics/accumulator stages. Its `+0x120` flags now feed the wheel-rate tail before the damage bit is ORed. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Routed contact-feature `response_scale` (`ESP+0x1C`) into the `+0xEC` accumulator divisor and `secondary_response` (`ESP+0x78`) into the second tire body-force scale, matching their later stack-local consumers in `0x442530`. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reused contact-feature `ESP+0x1C` as the scale for the longitudinal reference-speed limit (`scale * 10`) in the composed tire response path, keeping it distinct from the separate curve mode multiplier. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Corrected the longitudinal reference-speed sign gate to use the source's signed raw-float-bit comparison (`cmp [local], 0`) instead of converting the float to an integer. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reused `adjust_wheel_rate_blend` for spin preprojection's `<9.0` branch and corrected its threshold field from a mistaken 16.0 label to raw gate `0x41100000`. This removes duplicated blending logic and applies the source's `-0.2` rate coefficient in one place. Full Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** In the shared `0x431150` post-body tail, derive the `+0x382` store from the `+0xD88` source vector and updated `+0x38` orientation matrix; expose the always-reached global-interface `+0x38` callback and its vehicle ordinal. Release build succeeds; callback effects and original-runtime comparison remain open. No tests were run.
- **2026-10-23:** Reconstructed the shared factory-installed vtable `+0x38` method at `0x43BF00`: matrix-transformed position, bounded `+0x64` adjustment steps, four-state timer/message transitions, and the state-3 result-helper dispatch. Query/display/virtual return values remain observations. Evidence: vtable data and method bytes `ev_6eaa26f360d1ef80114e74300fa6f1df68ec8b46da35e99d0006eff692b2544c`. Release build succeeds; no tests were run.
- **2026-10-23:** Derived the 0x43BF00 state query from the 0x43A880 X/Z region predicate using the observed +0x6C/+0x74/+0x78/+0x80 bounds, and recorded the invalid-state 0x411000 panic route. Region bounds can still be supplied independently for each external target. Evidence: `ev_06979066b49eac1dc764d60124407aa440f208537c112e84a09fd1554cda86c6`. Release build succeeds; no tests were run.
- **2026-10-23:** Replaced the two state-3 callback observations in `update_alternate_mode_interface` with their exact virtual implementations: slot `+0x64` returns 60.0 seconds, and slot `+0x68` compares velocity squared against raw constant `0x4431C71C`. Evidence: vtable pointer and method/constant bytes. The display and result-helper side effects remain unresolved. No tests were run.
- **2026-10-23:** Composed the `0x43BF00` message events with `0x46BFE0` target flag writes and `0x46E4F0` queue insertion. Active target lookup results remain inputs; queue order/count, the 32-entry capacity, lock-pair boundary, and overflow panic are now represented. Evidence: `ev_24f37a961525a35d4675ac58a31284a5f9540e00294afdfae6e305daa0cba65e`, `ev_df2f6279277fd284aa039694c3bc521e14b550f164bbf749d55a173af6689713`, and `ev_9b4531dbb5371cdeda301bda772de57a3d6d8dcd6f9d4f44a68228280fc97c8f`. No tests were run.
- **2026-10-23:** Composed `0x411F10` in `plan_application_startup_handoff`: dedicated switch/port, initial and canary low-byte gates, non-dedicated loading/config/parser/frontend branch, initialization success gate, shutdown and both handle-finalizer paths. External OS/config/parser results remain observations. Evidence `ev_1dfe423eb4dea801ca131c6ee81df3a348a4718c84b1adc88b40741dda97339a`. Release build verification follows; no tests were run.
- **2026-10-23:** Extended the startup composition through `0x416080`: modeled the VxD `CreateFileA` parameters, stored-handle validity gate, and conditional `0x4160C0` import-section scan. The scan recognizes `.text`, `.rdata`, and `.data`; original memory patch effects and OS behavior remain outside the portable model. Evidence `ev_bc505c04e68af48095b20e9f33bfd2d2c046e2257197f0620aeed430fe7df582`, `ev_4412c28011e4dfa27b0396e1d044c460b28b82aaebbbe52fdf658b8ecf46a9b4`. Release build verification follows; no tests were run.
- **2026-10-23:** Connected `0x411F10` to the existing `0x412120` config-path model. Handoff inputs now supply the observed module filename/API result and directory creation outcomes; results include derived Config paths, creation helper status, and fatal exit-code-6 path. Directory creation failure does not suppress parser flow because the original caller ignores that helper's return. Release build verification follows; no tests were run.
- **2026-10-23:** Composed the existing `0x401280` switch parser into the `0x411F10` handoff. Startup command state and `-d` directory-change outcome are explicit inputs; the handoff now returns the parsed options and only reaches `0x401460` when the parser's modeled low byte is set. Release build verification follows; no tests were run.
- **2026-10-23:** Reconciled `0x416360`'s memory-subsystem branch with `0x410AA0`: the callee forces its low-byte success result after logger setup, so the caller's zero-result cleanup branch is syntactically present but unreachable through the observed callee. Removed the unsupported injected failure input. Evidence `ev_ac5a552b3d8fa6002ade17479626397ad2c9e110105697b013c1bd83ebae79ac`, `ev_ae19eca17c27a122dc3cd91004d429fb6622232725618a620c3d517d96298536`. Release build verification follows; no tests were run.
- **2026-10-23:** Expanded the `0x414110` heap branch in startup initialization: report `HeapCreate(1, 0x1400000, 0)`, secondary allocator setup, counter clears, and the conditional VxD allocator status log as distinct effects. Evidence `ev_108db34feb7562c8b6ce7636aae5f5f67bf8329302dd0896c61f1dcf61e74310`. Release build verification follows; no tests were run.
- **2026-10-23:** Added the `0x414840` startup timer probe to the initialization result: 100 ms timer cadence, at most 30 sleeps of 200 ms, timeout panic, timer stop, and completion-byte resets. Callback scheduling and thread/timer API outcomes remain runtime observations. Evidence `ev_7338b0a9717f74448ff5a1ac8958bd3b6b51eddc4295e69366f9dcb871ca276e`, `ev_03feeed66afda725b5400ede1ba208981da65cabdf9212eb5bef45e0306ca745`, `ev_c25acb9bac08cd931ed87043fd63eba24394f23a24b201cd20b334472080206e`. Release build verification follows; no tests were run.
- **2026-10-23:** Corrected `0x4111F0` from a task-table label to a 32-entry file-handle registry: `0x104` stride, active byte cleared, and handle field set to `0xFFFFFFFF`; registry count and record state now reset together. Added the observed `0x4154A0`/`0x4112A0` exception-log `CreateFileA` parameters and successful-handle registration, preserving unknown versus failed-open observations. Evidence `ev_0952de40d83f81e75e4ac2bb342f3789018867ad9407ba3ee860b675e05bdfb3`, `ev_6582f2a6143199d40504baa8937d4624735915c728003664618df73ab1987e93`, `ev_393bc9c63e1f718bab082546fac91e560af03acffd0b2a2cf0148987c1485fd2`, `ev_8d53b14702b607021698802d2e01aff0bdb1cd065a15541c4f701ca421067f98`. Release build succeeds; no tests were run.
- **2026-10-23:** Added `register_named_critical_section` for `0x4150C0`: manager-lock boundaries, case-insensitive duplicate detection, 16-entry count/reuse behavior, underlying pool allocation, panic conditions, and invalid full-registry index attempts are represented. Evidence `ev_0b14f12122d4b002313b409ba3e5744723b355b465bbee9767a4740b8eee92fb`, `ev_e4e68c82dc0f62e5aa3bdad1ee38a720532ad2b8a71714ea32157752012d154b`. Release build verification follows; no tests were run.
- **2026-10-23:** Wired named critical-section registration into `0x416360` in order: `task`, `File`, `Log`, `Mono`, and `Mem` after heap success. One registry and allocator state now flow through the startup call chain, with registration/panic outcomes retained. Names were confirmed from target bytes at the five global addresses; address-map evidence IDs are listed in `engine_application_entry.md`. Release build succeeds; no tests were run.
- **2026-10-23:** Added `0x416680` background-worker setup to startup initialization: `BG` named lock, `BGTask` callback/parameter `0x10`, thread/task observations, and writes to globals `0x4EFFF0`, `0x4EFFF4`, and `0x4D32A8`. Evidence `ev_b7e437fd564e292789739383c428f4da7507d2db5e0351880af30a9e297e8c23`; target bytes for `BG` mapped at `0x4D32B0`. Release build verification follows; no tests were run.
- **2026-10-23:** Extended startup exception setup with the `0x415680` filter and `0x415C60` floating-point control mask (`prior | 0x7F` under `0x8001F`); composed `0x4154E0` executable overlay mapping, deriving the max section raw end, overlay pointer/length, and failure/no-overlay cleanup lifetime from explicit API observations. Evidence `ev_6582f2a6143199d40504baa8937d4624735915c728003664618df73ab1987e93`, `ev_bdff513e5431f6b18116119e4e744b6ca1f3c44147ef0ab01555380b47025b98`, `ev_229ab9287b7ad22e09f11e6a2a3ba3390272f0d8faab61cddb9671f6de5206c0`. Release build succeeds; no tests were run.
- **2026-10-23:** Resolved the NCAR sample callback virtual slot: constructor vtable `0x4C8238` maps `+0x40` to `0x433620`. The callback model now applies its sample position, normalized orientation basis, velocity/timer resets, embedded-field copy, callback-child clears, and event type 7 payload, while leaving downstream resource/device methods as explicit reached effects. Ghidra evidence: `ev_6e32a6817044acefa8764aeeb4fe1e050abcb8b63cbc9a5ef2dc9aa9528da753`, `ev_32889c6ea19b675c584a2a82897777c119f9e26f45a391f109b8b4ab608a610a`, `ev_4035b1285ed30a04f1640a355504319d1da692aaf75b76639be736a56a2514f4`, `ev_4e20ce0d4acd4d4639db7448c6fb997dd001d622a1c425c488e0481e7c0f8c26`, `ev_f974ae5591cab735d624fdac9decd3aa90e5f4ea3ea6eae34bb3c5e127a277bf`, `ev_84bbec06835985531a2b09bdad49d78002149029985f83121d821528d234097a`, `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`, `ev_49606e49141a3606cd6e046bc10936586662df1207ab8af724502df1c10b8ade`. Release build succeeds; no tests were run.
- **2026-10-23:** Routed both reciprocal-length operations in `0x433620` through the existing Pentium FDIV compatibility approximation when the recovered global flag is enabled; the timed-frame and position-reset paths now pass that flag into the virtual callback model. Added explicit coverage for the four `0x441B90` slot-reset calls. Release build succeeds with no new warnings; no tests were run.
- **2026-10-23:** Traced the `0x441B90` helper called on four `0x1A4`-stride NCAR child slots. The callback now clears its eight observed DWORD fields and two byte fields per slot, with separate state records preserving the helper-relative offsets. Evidence `ev_4a2b61d33988864abf28dc3967396235c19b9e10`; Release build verification follows; no tests were run.
- **2026-10-23:** Connected the `0x433920` five-group reset to the `0x433620` callback and moved the bounded resource-copy projection into shared NCAR code used by both sample callbacks and post-body timeout resets. Both paths now copy destination-counted first-three-DWORD rows, preserve descriptor pointers, clear `+0x15` flags, and report short spans consistently. Release build verification follows; no tests were run.
- **2026-10-23:** Recovered the two `+0x478` group virtual calls in `0x433620`/`0x4338C0`. The group walks its `+0x4C` children and invokes child slot `+0x14`; child target `0x42CC10` copies vector bits `+0x64..+0x6C` to `+0x20..+0x28` and clears dirty byte `+0x88`. The NCAR sample callback now applies both passes and reports child call count or an invalid missing-group dereference. Evidence: group construction/traversal `ev_7884fb79acfb26d4c7b0004cd2a1679669a9b097e9fd4e49a6650ec607b8731e`, `ev_5d28c6b28248475019e59e99da0c1ada50c94b768203aac20dca9e4f7e84e876`, `ev_b568de17134e6a04d94f9cb497f37ad3da071d14295b40e6078752b74ec1cade`; child target `ev_464f63788b649460ec644f40b8d24e71ad3c91ececfd`. Release build verification follows; no tests were run.
- **2026-10-23:** Expanded state-0 geometry rectangles through `0x4484C0` and `0x452730`: each now carries four transformed-vertex records, the observed six-index triangle list, and raw primitive/vertex type/count/flags parameters. Evidence `ev_2907d66994b17fff6cc1d0590a34c77e00d5a7bee3bca0fb84c8875726166a6`, `ev_281aadb3ba99e83351908edcc9e32fd92cfe1828af04f7aa7036d083b31d0f0d`. Release build verification follows; no tests were run.
- **2026-10-23:** Resolved the rectangle API boundary: `0x451FE0`/`0x4588B0` establish an `IDirect3D2` wrapper; `0x4588F0`/`0x458990` create its `IDirect3DDevice2` at wrapper `+4`; and `0x452730` calls `DrawIndexedPrimitive` at device vtable `+0x78`. The model now names the triangle-list and TL-vertex values and records the ignored HRESULT. The GPU/driver output remains external. Evidence `ev_4d03f10be057a55ba3900ca8373e9c9468c90caf428b5c2d04bc1d6e097bfd4e`, `ev_35b36627000bf1aebc9abf313793369c85c7a58026ddbc09e56b97b7ff5de88`, `ev_0c3d18680d44c617bb5f4ffb7fcde88a6fb706a5f9a6017ef50b63db4af4e6a9`, `ev_55e85dcc1688acacf93aba7d357531a5713097b8eeadc74fbaeab33bc228cef5`, `ev_281aadb3ba99e83351908edcc9e32fd92cfe1828af04f7aa7036d083b31d0f0d`. Release build verification follows; no tests were run.
- **2026-10-23:** Corrected the `0x49C5C0` callback projection: its signed threshold field at `((global + 8) + 4)` is distinct from the original argument passed to virtual `+0x84`. The ChatLine byte lookup now uses the callback argument, not the gate field. Release build and whitespace check passed; no tests were run.
- **2026-10-23:** Added `plan_replay_screen` for selector 4 (`0x406950`): record-present versus empty setup, the 0x335-DWORD record copy, resource load/unload order, repeated menu result handling, Enter callback return/retry behavior, post-action call, and cleanup flag clear. Menu results and callback bytes remain runtime inputs; control construction and replay parsing remain open. Evidence `ev_f78ea88766e340634ae8e120f2a324caff257e1184cf11c4d474944136cb6bab`. Release build and whitespace check passed; no tests were run.
- **2026-10-23:** Extended the replay-screen projection with all six observed `0x408F50` constructor calls, preserving EBP-relative descriptor locations, raw argument words, the two runtime binding values, and constructor order. One-time button registration and three cache comparisons remain outside the call projection. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added the replay screen one-time button registration gate (`0x4EC704` mask), all five object/key/atexit triples, and the three cache-dependent `0x418CC0` calls. Missing mask, binding values, or cache comparisons now mark the projection incomplete rather than silently assuming state. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added `plan_replay_load` for the `0x407760` callback: modal guard, existing-path versus file-dialog route, parser-open/read/record-validation/replay-start branches, existing replay teardown, failure dialogs, handle close, and renderer restore. Fixed record size `0xCD4` is preserved; all dialog/parser/read/validation/start outcomes stay explicit observations. Evidence `ev_e82713137b5dcc99f51a5506e98764583d2bfcff55a87bcd891019721e74f29a`. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Resolved the replay screen Enter callback `0x408160`: it returns zero immediately when no replay is visible; otherwise it constructs the analysis page and returns zero only for nested menu result `0x42`. Added that return rule to `plan_replay_analysis_screen` and wired its menu observation into the outer retry loop. The fixed x/y scale bits and 25 constructor count are recorded; detailed widget fields and cache/translation effects remain open. Evidence `ev_d950a3a9d0ef73510a5640de9d23356bdaa5168a35503818982b830b04f30df2`. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Expanded `plan_replay_analysis_screen` to preserve all 25 `0x408F50` argument blocks from `0x408160`, including stack descriptor offsets, stack-local pointer arguments, dynamic global words, fixed metrics, and the `0x42` exit return. Release build passes; no tests were run.
- **2026-10-23:** Added all eight one-time Replay Analysis localized-label registrations at `0x408160`, preserving `0x4EC788` mask bits, destination objects, resource keys, atexit callbacks, and registration order. Missing mask observations mark the analysis-page projection incomplete; nested page results now remain attached to the outer replay-screen plan. Release build verification follows; no tests were run.
- The analysis-label mask now carries forward across repeated `0x408160` visits, preventing the outer loop projection from repeating one-time registration calls after their bits have been set.
- **2026-10-23:** Added eight ordered analysis-page `0x418CC0` cache refresh gates at `0x408160`, recording each refresh object and interleaving conditional calls before its matching `0x408F50` constructor. Missing comparison outcomes mark the plan incomplete. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Reconstructed the `0x407C60` replay save callback through output creation: four one-time label registrations, two pre-dialog cache gates, Save dialog acceptance, `CREATE_ALWAYS` open, fixed `0xCD4` write, serializer `0x426960` header/variable payload writes, open-failure cache refreshes/dialog, and file close. Dialog/file-open/cache outcomes and serializer lengths derived from runtime globals remain explicit inputs. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_replay_file_load` for `0x426790` and composed it into `plan_replay_load`: 28-byte header gates, cookie/version/word-count/payload/frame/packet checks, exact frame-table and payload reads, error-string selection, replay-loaded flag, and `0x426580` success call. Native read outcomes and header fields remain observations. Release build passed; whitespace check passed; no tests were run.
- **2026-10-23:** Completed the missing-key player-name copy bound in `build_single_race_entrants`: `0x46B020` copies up to 12 characters for an existing key, while `0x46B080` zero-fills a 64-byte fallback buffer, so the row receives at most 63 characters. Evidence adds the `0x46B080` path and `0x4BD490` bounded-copy helper. Release build verification follows; no tests were run.
- **2026-10-23:** Added `plan_human_car_setup_load` for the `.csu` reader `0x45EE40`: exact `0xD4` file gate, read-result-ignored behavior, version warning without failure, ordered `.ccs` resource fallbacks, their version/size checks, numeric reset writes, generic `Default Setup` record, and handle-close boundary. The wrapper plan links `0x45EFD0` to the loader and its `0x8C`-byte active-record copy. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_get` for `0x417CB0`: special-name search routing, loaded-set/resolver outcomes, type mismatch panic, used-bit and first-use state, payload/owner results, refcount condition, missing-resource log suppression, and lock/gate cleanup. Inputs preserve runtime bundle-search and resolver results. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_file_load` for `0x417EF0`: tilde versus two-prefix path attempts and path concatenation, 12-byte `RES0` validation, descriptor/name/data allocation sizes, basename wildcard entry, exact file-size arithmetic, data-read cleanup, loaded-list insertion, and close boundary. File, allocation, and read results remain observations; long basenames and null-allocation crash exposure are reported without unsafe writes. Evidence `ev_88e35c1dea12881f52f3f76daee5da30ca7df02c8f218cd0bd0701ccca4beb92`; `%s%s` path format bytes `ev_a6be7006dfff7191cdf99cad3e62f85bca695798c55b7c3d190b3ed9ad5dcf59`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `project_race_setup_result_buffer` for the `param_2` block of `0x401EA0`: 49-DWORD zero-fill, signed count, byte-sized lookup selectors, conditional associated-row loads, and the three-DWORD row layout. It flags possible null lookup dereferences and count-driven writes past the fixed cleared region. Evidence `ev_4775651abe34ed567a78a4030c64dbdba73fb905c070b04426e91ce688cd65b3`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_race_setup` to compose `0x401EA0` setting and control flow: guarded `race_camera` read, default value 5, 12-to-4 normalization, writes to both globals only on the read branch, independent first/second mode bytes, callback selection, optional result-buffer projection, and final persisted setting input. Callee-owned state mutations remain observations. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_mapping` for `0x411BE0`/`0x411B10`: read-only mapping flags, CreateFileMappingA/MapViewOfFile success and error routes, stored-path fallback open, whole-file allocation/read, ignored fallback read status, missing-file panic, and the returned fake-map ownership flag. Win32 outcomes remain explicit inputs. Evidence `ev_82aeff87c78227f70449855021a17aa75053a7e818c3ef815fe869427746dbc7` and `ev_08e233805ab7e0c509a149f5c964ac451e7c51275d887aacfcf11be8d5a7ccff`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_rst0_bundle_load` for `0x417550`: 16-byte signature gate, set-node and initial-region allocation sizes, ignored read result, composition with mapping fallback, buffered-versus-mapped chunk offsets, and the initial-chunk header backpointer writes. It deliberately preserves the shipped loader's unchecked count/boundary behavior instead of applying offline-parser validation. Evidence `ev_aaa4f2d3f975613baf65ceff82c4c8015dac5da03762aaebf211f26bcf581183`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_set_acquire` for `0x417430`: case-insensitive loaded-set reuse and reference increment, ordered path opens, composition with `0x417550`, failed-load handle close, and successful handle retention. Prefixes, file API results, and handle slot remain inputs. Evidence `ev_fad4d024c643d67e86345e24f637ffe868bc3ef34eae7508981899247b624f90`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_set_request` for the `0x417740` lock/panic/unlock wrapper and `plan_resource_release` for `0x418160`: owner lookup from chunk-header backpointer, reference decrement/underflow behavior, wildcard-set lookup at zero, linked-list removal, and freeing the dynamic resource buffer/descriptor/set node. The `0x417870`/`0x4178B0` sweep callbacks remain open. Evidence `ev_33f06f0c2ab2a0355eacac31501a31573e46e7ef343e762e807edae09049075e`, `ev_19bb56f3601c354484189057db4ef33b81f572230b3c9ed82f0106ed95deadbf`, `ev_95abdfc504f67f9ea1381f0879a562b9c116a5ef814f4aadf3ec5f5bd0691bd4`, and `ev_4b90f67402273605610db531514346efd28b4db5f387d073306a17b88de694ee`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_resource_set_sweep` for `0x417870`/`0x4178B0` and shared iterator `0x417880`: preserve loaded-list order, skip wildcard (`*`-suffixed) entries, and dispatch the respective callback addresses for remaining nodes. Callback bodies at `0x417790` and `0x417820` remain open. Evidence `ev_e364f14f99f41318a23f9353cbdd57f82df9bf96409d66287a669a36404de008`, `ev_b0df93edd5b5a6f226529a6b4f4bcd9f2110e0e8e63fef7be2509a14f02570cc`, `ev_6fe560d009cc0a2ca10d108b7f3dbdc4ba0d674e41aed758647ffdac361fa515`, and `ev_86e17d81e89805093da4b37a4444121dd3eeff3875f8ebe8d305163a439c2477`. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Extended sweep callbacks: `0x417820` now projects fake-map buffer free versus real UnmapViewOfFile/CloseHandle cleanup, while `0x417790` projects mapping recreation and retargeting of buffered chunk pointers using index attributes. Evidence includes instruction-byte captures for `0x417790`/`0x417820`, plus `0x4172D0`, `0x417300`, `0x411CB0`, and `0x411BC0` dossiers. Release build and whitespace checks pass; no tests were run.
- **2026-10-23:** Added `plan_legacy_file_read` for `0x411560`, covering one-based file slots, exact-byte success, short-read panic, and API-failure `GetLastError`/panic behavior. Traced the adjacent open/register/close/size helpers (`0x411430`, `0x4112E0`, `0x411500`, `0x411540`) and documented their observed 32-slot table and `0x104`-byte entries. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_legacy_file_register` and `plan_legacy_file_close` for `0x4112E0`/`0x411500`: lock coverage, append-or-first-free selection, one-based slot conversion, full-table panic, path/handle registration, native handle close, slot invalidation, and caller slot clearing. Runtime handles and OS outcomes remain observed inputs. Build verification follows; no tests were run.
- **2026-10-23:** Added `plan_legacy_file_open` for `0x411430`, composing observed `CreateFileA` flags and failure handling with the table-registration path and one-based return value. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added `plan_legacy_file_size` for `0x411540`, projecting the shared one-based slot conversion and `GetFileSize` call with its null high-word pointer. The observed low DWORD remains an OS result input. Release build and whitespace verification follows; no tests were run.
- **2026-10-23:** Refined handle registration to report the exact `path.size() + 1` copy length and whether it exceeds the observed 256-byte path area before the handle field. This preserves the unchecked copy behavior visible in `0x4112E0`. Release build and whitespace check pass; no tests were run.
- **2026-10-23:** Added `viper_graphics_startup` and `plan_directdraw_startup` for `0x44E080` through `0x458D20`: conditional device enumeration, DirectDraw fallback object/capability selection, missing-device exit 4, required 640x480x16 mode exit 5, exclusive-mode exit 2, and success-state reset. COM and display results remain observations. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Refined the graphics mode gate with the byte-level `0x44EE00` callback: it recognizes five exact width/height pairs, sets the associated global flags, returns 1 for a recognized mode and 0 otherwise; startup specifically requires 640x480 and makes no color-depth comparison in this callback. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Separated fallback `0x458470` DirectDrawCreate failure and DirectDraw2 QueryInterface failure (each logs its HRESULT and exits 2) from the no-object allocation route, which falls through to the no-device exit 4. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `0x458D60` to the graphics startup projection: the `0x6C` mode description (`flags=0x1001`, `caps=0x2000`), EnumDisplayModes result input, HRESULT logging, and callback dispatch are now represented. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Refined fallback ownership in `0x458470`: base DirectDraw release, successful DirectDraw2 acquisition, four-byte wrapper allocation/constructor, and the observed missing interface release on wrapper-allocation failure are now represented. Corrected the DirectDrawCreate error logger to `0x410EA0` (message only); QueryInterface errors use `0x452770` with HRESULT. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_startup_mode_entry` for `0x401460` selectors 1-4: mode 1/4 globals, record initialization, observed string copies, mode-4 12-word global copy, common routine ordering, mode-2 no-op, and mode-3 init/cleanup. Runtime contents remain inputs and record semantics unresolved. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Resolved the mode-2 `+0x84` virtual predicate target through the `Race's ChatLine` vtable: `0x4A1B70` reads the indexed row byte at object offset `0x1C8 + index*0xD9`. Added bounded object-memory evaluation to the dashboard predicate and updated the priority/docs; helper side effects remained under analysis. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Traced the ChatLine state gate upstream through `0x4A1E80`: a `0x49FA00` result of `-1` stores state 3; any other result stores state 6. The predicate model now derives the `>4` gate from this lookup when supplied. Also established that the callback requests one-time PGS status labels through `0x4A1680`, while `0x4A1AF0` is a no-op; label-registration side effects remain open. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended `plan_multiplayer_status_callback` to report the stable 22-entry label ID obtained from `0x4A1680`, plus the conditional `0x4A1AF0` no-op call for status values >= 15. This closes the callback's observable helper-call projection; dynamic string refresh and one-time label registration remain open. The combined Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_career_season_parse` for `0x4ADB00`: bounded event-count/header/prize indexing, 22-DWORD record stride, `Career:Money:<id>` lookup outcomes, and the observed panic call on missing labels. It leaves payload semantics and file-format validation open. Release build and whitespace verification pass; no tests were run. Evidence is cited in `metadata/frontend_dispatch.md`.
- **2026-10-23:** Added `plan_career_testing_race` for `0x4AEC30`: bounded active-payload reads, track copy length, Reversed-byte preservation, `0x40EED0` result routing to race modes 0/1, and cleanup/season reload after loop termination. Runtime race results and meaning of career fields remain open. Release build and `git diff --check` pass; no tests were run. Evidence `ev_b9898af2025c9ce90e8c35090b58dd4b32173ab1800a17f32c500f4d6c2a8e`.
- **2026-10-23:** Added `decode_career_season_data` for the observed nine-DWORD header and 22-DWORD event stride, plus `--inspect-season` to expose raw records from an RST0 bundle. Confirmed exact `9 + count*22` lengths and extracted all four shipped `SEAS` chunks. Traced the first eight event bytes to the track name, DWORDs 6–13 to placement payouts (`0x4AC290`), and DWORDs 14–21 to placement points (`0x4AA5E0`); `make_career_event_ranking_observation` now feeds the decoded payout/point tables into the existing ranking projection. Event DWORDs 2–5 and runtime race results remain open. Evidence and samples are in `metadata/frontend_dispatch.md` and `metadata/career_season_samples.md`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Confirmed all 24 shipped event rows expose a NUL-terminated track name and the same placement-point schedule; added complete track order to the season sample notes. The `--inspect-season` command reports complete headers/records and zero trailing words across all four resources. Career ranking now takes payouts and points directly from a decoded season event while entrant order and result rows remain observed runtime inputs. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Refined the season record decoder with the two additional `0x4AA5E0` reads: record byte offset `0x10` and DWORD offset `0x14` are forwarded into its race context, so the raw values are exposed without speculative labels. Recorded all shipped values; DWORDs 2–3 have no direct field-specific read in this function body. Evidence `ev_3a3ac7ccfd379e1ae77b3aca9288354dd126a9ae4e079333599a2cde88a0e810`. Release build and `git diff --check` pass; runtime race results remain open.
- **2026-10-23:** Connected the eight-row result buffer projected from `0x401EA0` to the career payout/standings observation using `make_career_event_ranking_observation_from_race_setup`. The composition requires complete input and exactly eight rows; season payouts/points come from the decoded event, while entrant order and event cursor remain observations. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the `0x4224D0` record-load projection on top of the factory table: end-of-records, bounded payload completeness, panic non-return, unconditional virtual `+0x2C` attempt (including the binary's null-dereference path), conditional `0x426F20`, and output-slot/success behavior. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Updated the physics-task priority entry to reflect the newly projected per-record loader; the bulk array loop and track-object construction remain the next slice. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_physics_task_array_population` for the `0x420480` record loop: 0x800-byte allocation observation, unconditional initial read, success-count increments, false-result termination, observation exhaustion, unchecked allocation use, and potential overflow beyond 512 pointer slots. Evidence `ev_c7a8122ef79fb8e7d7e9e166e4bee5d0461c382879d95c4f0a01ddde1748d040`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_physics_track_service` for `0x426000`, including selectors 0/1/3/5, allocations, constructors and vtables, unknown-selector panic, and the unconditional virtual `+0x14` path (including null-object fault cases). Corrected the metadata to include selectors 0 and 5. Evidence `ev_2b5d1097e10c597fa911959cea8832d4fc90daf1d5a108167731b95dc8c346ae`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_task_table_init` for `0x426EB0`: 2 MiB primary allocation, unsigned stride increment/division, signed maximum clamp, sentinel-array byte count and `0xFFFFFFFF` fill, zero-trip byte loop, and divide-by-zero/allocation-fault observations. Evidence `ev_8ad67858bad8d450cc1f946396d635333ab6ff9fb0bab2e840908a23c7fd763f`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Modeled `0x4C59B0` resource registration in `resource_bundle`: 0x40-byte allocation, constructor/reporting outcomes, observed object-index registration, and one-based handle return. Connected `0x426000` selectors to their observed virtual `+0x0C` targets and documented the `0x420480` MIL lookup/callback/final flag sequence; selector callback bodies remain open. Evidence includes `ev_e930a70e159405d16b4cd7c9a7a3b1de2fa678ee7a4e1adcb188594195d84263`, `ev_9df77eb507cefea6dc26ef5ad85565a10ece271069287790c58fb47988f77a23`, and the four vtable byte observations in `physics_task_objects.md`. Release build and `git diff --check` pass; no tests were run.

- Build note: the full Release link succeeded with one C4996 warning at `src/main.cpp:40` (`getenv`); that file was not changed in this slice. `git diff --check` passes.
- **2026-10-23:** Added `physics_task_initialization_global_stores`, preserving the 61 literal DWORD reset/finalization stores in `0x420480` order, including zeroed ranges and three identity matrices; dynamic allocator/resource results are excluded. Evidence `ev_c7a8122ef79fb8e7d7e9e166e4bee5d0461c382879d95c4f0a01ddde1748d040`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_track_post_load` for the recovered selector-1/3 three-field predicate, selector-5 unconditional true callback, and null-receiver fault; selector 0 remains unresolved at the non-boundary address `0x43BE20`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Corrected the selector-0 virtual callback address: vtable `0x4C84D8` points to `0x43CB20`, not `0x43BE20`. Recovered the 16-iteration loop, its 32 ordered object writes, and true return; `plan_physics_track_post_load` now includes them. Evidence spans `ev_17ed512966134d275b260207e6c5219462aa132c6e505191fd47597bd573294d` through `ev_9945f2492c4aa9d15123120706d9d8b2c1a674a7fbcfa6a81d639e10c8752acb`; the vtable bytes are `ev_4a3b3471c71109794a217ce08c7d76dcc872daec61a4b5cebf0b58bca7848b56`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `physics_task_fixed_step_dispatch_order` for `0x421050`: normal per-object `+0x2C`/`+8`, `0x427080` helper position, ordered distinct auxiliary-slot pairs through `0x432300`, alternate `0x426A10` branch, and final `+0x0C` calls. Evidence `ev_e91e243d2a75bd1f17a1d1dc64841e3b5122e94779806c54ebe0bc4421566982`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_time_slice` for `0x427080`: signed time-slot calculation, transition-vs-stable branch, observed post-transition offset, unchecked per-object `+0x2C`/conditional `+0x20` callbacks, null dereference on that path, and final `0x427150` capture call. Ring update internals in `0x4272C0` remain an observed input. Evidence `ev_6732cebdd4b39b1ea951510e30ba3ce6e56d0c30512aeb5defcba46dc3d7ed80` and `ev_8b70fb7e330efcb44e638651c35a76e28791191e1770bcf9fcf2a4d7c1c61a28`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_time_ring_transition` for `0x4272C0`, including old-slot marker write/cursor advance, signed wrapped overflow threshold, slot-zero generation increment, high-water mark, overflow generation marker, and new-slot offset/reset branches. Prior ring-global values remain inputs. Evidence `ev_035843b8b1092b7f7814d274836d5394dd4862d9389888999d8ae2769c45f047` and `ev_699b74a5bf099f4cc59484e6ea3e7174a999ef165480917bba79771416b12f0b`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_time_capture` for `0x427150` and composed it into `plan_physics_time_slice`: exact 11-byte record layout, constant-derived clamping/normalization, truncating `_ftol`, and the existing Pentium-FDIV compatibility approximation. Linked the physics scheduler target to `viper_legacy_fdiv`. x87 extended precision remains approximate. Evidence `ev_f19a690178deb5d6c46c4981b20f796b1352c6316228f5b438448c6b89ecb7a9`, five constant reads, and `ev_d537319812fd9d0a2da1c7a557df65490d4e37caf0006017ec0c6e5f0137b127`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_alternate_time_path` for `0x426A10`: signed slot/fraction selection, next-record selection, per-object `+0x2C`/conditional `+0x1C` arguments, `0x426C10` sample interpolation, bounded indirect command-parser observations, prior-slot update, and same-slot sync flag. Indirect handler implementations and exact x87 precision remain open. Evidence `ev_c62985bb31746e21e9610af329debb7cc3081adcc7f6f468c5addf80fcb2f0e5`, `ev_19e343c839ec1bb7045905ccfc4e9bd7c5674ed4fbe98dee29e33be5f03fe192`, and `ev_c96e8668f387991b8e27677bbd50ff6fbfa984a0a76cc81a6d1793c9501391c1`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_command_handler_registry`: fixed eight-slot state, observed registrations for IDs 1/3/5/7 and first constructed object registration at ID 4, exact first-match removal, and missing-handler reporter path. Ghidra exposes the callbacks as unowned code fragments, leaving their actual command semantics open. Evidence `ev_f88622415ae97c159571f7ac6910b18ee12f140e1c995a7cd05e1989de3a4a7a`, `ev_0f228205c6358b981b2e03eb65dbcd340d41bdf1cee40cc52500c390ab44e945`, `ev_63ff2becf352a2b81ae06544d220ebcb9045eca84dedc5fa64eb89bf7fb7ac48`, `ev_4c48bb7817e4d2f6e653dedd1aa07dcbf82079f99d461e7ee9b8d3dbd5675fa5`, `ev_576efd2d3278724c54a566ca1ed90b015dac9a8b004fdebc71de16b8ac988112`, and `ev_4b8fff18e11f37b984b900aa1ad1a058de057c0e4955647e0a31c6782d76016e`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Used raw instruction-flow probes to recover fixed payload lengths for registered alternate-time handlers 1/3/4/5/7 (20/28/44/14/2 bytes). `plan_physics_alternate_time_path` now derives those lengths from the handler ID; other handler IDs still require observations. Traced handler effects through bounded RST0 task lookup/reset, event-buffer append, the eight-entry record ring, and virtual `+0x3C`, leaving runtime object and queue mutations as inputs. Instruction evidence IDs and helper dossiers are cited in `physics_task_objects.md`. Both Release targets build and `git diff --check` passes; no tests were run.

- **2026-10-23:** Added `plan_physics_time_command_payload` for the known alternate-time handlers: command-3 event-buffer strict-capacity append, command-4 DWORD validation/panic gate, signed task-index lookup and reset/update callback selection for commands 5/7, and command-1 eight-slot ring arguments. Unsafe bounds and null dereferences are reported without emulating memory corruption; actual queue/object writes remain explicit runtime inputs. Source evidence and instruction probes are cited in `physics_task_objects.md`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Recovered command-5's two packed vectors from raw x86/x87 flow and constants: three bytes map by `b*10/128 - 5`; three unaligned little-endian words map by `w*30000/32768 - 15000`. `plan_physics_time_command_payload` now exposes both float vectors for its virtual `+0x3C` callback branch; labels and physical units remain unknown. Evidence is recorded in `physics_task_objects.md`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Added `plan_physics_auxiliary_pair` for `0x432300`: target activity gate, strict squared-distance/radius gate, legacy-FDIV proximity weight, and force-vector accumulation from the second object into the receiver. Stored binary32 squares are preserved; x87 intermediate precision remains approximate. Evidence `ev_093a726660d0859203c394c4b6a10b413bfe3b6bc47d73582e1b7837d83f1bf4` and `ev_04cfbeb623acd512909a581ee47905e7788fe86c1597666f2a7076b924c0d6d4`. Release build and `git diff --check` pass; no tests were run.


- **2026-10-23:** Implemented `update_ncar_interpolated_wheel` for the five-byte wheel payload in `0x444240`, including the four interpolated fields, current-record flag propagation, and copied parent velocity. Documented the helper calls left external and distinguished its 0x3E-byte parent records from the existing sample-history format in `metadata/physics_interpolated_object_update.md`. Evidence `ev_20a7335a3ddc3113b52072caa357d8f447779c1dca79f17854b811d17e5af898` and parent evidence `ev_5414f1bab7fbe8468b429851feddd8e526f06a75951618dba0a23d22e5f02da1`. Release build and `git diff --check` pass; no tests were run.

- **2026-10-23:** Expanded `update_ncar_interpolated_object` from fresh `0x432420` pseudocode with the `+0x50C` branch, direction-dependent scalar writes, velocity copies, initialization clears, and correction that both tail-byte writes use record one. The second-record `0x200` flag drives rotation endpoint selection and byte `0x3A` interpolation; the parent plan composes all four wheel projections. Release build and `git diff --check` pass; no tests were run. Evidence `ev_f7bfe367bb4c43cdee7f145954ceec34cc08f5675dc0f1f3daceac6ac97b4528` and `ev_3cb650cf5fecd98cc704b82040c8d6034a199a76786fa9b918df8b116c322960`.
- **2026-10-23:** Confirmed `0x432420`'s four `0x444240` calls are composed into the parent update; cross-referenced `0x444240`'s `0x443F60` and `0x437BA0` calls to existing `advance_wheel_animation_basis` and `transform_body_local_point` models. The wheel-specific input state for those helpers remains explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Composed caller `0x427B10` as `update_ncar_interpolated_state`, preserving its save/restore of object flags and interpolation of endpoint speed magnitudes into `+0xED0`. Evidence `ev_4056ddfc4f12323d76158052c3dfd33d1ed4cdb27387a46500af0aae76dfb767`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Reconstructed the `0x437650`/`0x437CF0` path in `update_ncar_interpolated_object`: packed axis-angle record conversion, endpoint selection, 12-float interpolation, three-row normalization/cross-product rebuild, and record-derived trailing vector. Legacy FDIV compatibility is applied to normalizations; x87/libm precision remains approximate. Release build and `git diff --check` pass; no tests were run. Evidence `ev_9afebfd1baa1cad8101c71f4ac20399b0c4ef1b8ef7f6fca2ca7cb5147dadd12`, `ev_9908ffa0e67a6e59d1e3de0f410d952d88805616c367519a42a0d1bf4024856e`, `ev_e8ed20d85f950e46855973603f0e96e9f313df3c2c611318b39daa5958f7d632`, and `ev_19ba195243bd9450229ee5b69f82713490cc5c875592c92e4aa7d599b264b27c`.
- **2026-10-23:** Extended the conditional `0x433920` branch in the interpolated-object plan to include the known direct field clears, four wheel-damage clears, five resource-group passes, and backend virtual call attempt. Resource payload copying remains delegated to the shared bounded reset projection. Evidence `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Resolved the reset backend call at object `+0x478` to `0x42CCE0` and traced its sphere children to `0x42BEB0`, which transforms each local `+0x20` point through the parent basis/translation into `+0x2C`. `plan_ncar_reset_backend_group` now computes those ordered positions and reports incomplete spans and null-pointer fault paths. Evidence includes `ev_514b6fa842f491d8b5dbf5501d24b00bdf300a85b95442fe058a447d66bd4279`, `ev_b019ddbfe2f563e2ebf314797f920fdf745610827e5aeb33e0c03e387ff57abd`, `ev_d9537c7f3a1e3382945b1a9b4302c86c7ecb8165af2eb6a59d72b06488c4509c`, `ev_9643dd2fa27fbb6f9cd07a255baf362c9c9be3618494bc912af88d28c0c7dc97`, `ev_b7111322c7466b221124e121d8cab88868bba9c08172a082bac2b328eff60544`, `ev_335fb86f44d864d6e55a6429d95223162f4e9611db8cac9834f7b3ad05ee98f9`, `ev_c1d6507e02539da1c66183b9a8e281a1c02ac71baa5ca424f08e9b76123db77b`, `ev_a85d856838f6b80e0b2072601beae8b93a48d13b1723a85331744fa717352b10`, and `ev_b66a737cbdb951b62024d64c01713cf560bbec332f50271801727399f2c24055`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Connected constructor geometry to the reset-child projection: `derive_ncar_reset_sphere_geometry` scans the observed eight-float model vertices for six bounds, applies the constructor's car-record shifts and `+2.0` maximum-X cap, then produces the exact 12 local sphere points created by `0x433C00`. Bounded input coverage and the null-extent error path remain explicit. Evidence `ev_fbd640d5fbdc72a165c76c2dee2639411e71c3beaf9416d13c51fc2bd9d7ee93` and `ev_f3e69307c431f783075d86dbeed730ee32736a6159e31f8b9633cd65593d0208`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the `0x42CD90` child-radius accumulator to the reset group projection: maximum of `sqrt(x²+y²+z²)+radius`, with constructor radii 0.4/0.6/0.5 across its three observed point bands. Caller-supplied children without observed radii report incomplete bound inputs. The group starts at zero (`0x42CC60`); x87 rounding remains approximate. Evidence `ev_dd332da171bfd341a47517194c444647a9d0e5c33e934c709db81942b19d120f`, `ev_7b0433b3ba1bb352def91e7726dd2c36abda7a8fb9ede2d11123a1c73f90a2b1`, and `ev_f2bb786a74070a51b02f0626e35bf0593a3b38ab0f124dd60bb1a06abe4d65dd`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended each `0x433920` resource pass to capture the wrapper's unchecked resource-entry pointer write at `+0x10` alongside the existing `+0x15` flag clear and bounded row copy. Pointer values remain optional runtime inputs. Evidence `ev_ae086326d3d80f4eb2d8606a0bf9df800b98782b106605629bb331998281af4c`, `ev_caf16edf9d5a790cf3c58c81f9d5ff085a19a55dc8656c4c46eb65f3599c2e48`, and `ev_b20f5f32088b215e5828d9bd6f313682a2f55deafe67c0cc32995b1e250ad56a`.
- **2026-10-23:** Added the constructor child-tag side effect: its `+0x1C` virtual setter stores `0x6D` at child `+0x18`, and the generated 12-child projection records all tag writes while explicitly assuming successful child allocation. Evidence `ev_8acf1ec6b270eee028640de6232795e9cb4514dc185de471fa9408a4c1755a32` plus `0x433C00` constructor evidence `ev_f2bb786a74070a51b02f0626e35bf0593a3b38ab0f124dd60bb1a06abe4d65dd`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Traced the group `+0x1C` callback at `0x42FC90`: after setting group `+0x18` to `0x6D`, it propagates the value through every child's `+0x1C` setter. The generated constructor projection now reports the group tag and the second 12-child propagation pass (24 child-tag writes total across insertion and propagation). Evidence `ev_79088dcbbdc750d8dd4b9af1c312a439df19f7d1296ea864ce0b276044210fe9`, `ev_251c0d417d75951173f6db0cac2e213eb6a43f9ae7e3892d7b61bbb111a336ca`, and `ev_8acf1ec6b270eee028640de6232795e9cb4514dc185de471fa9408a4c1755a32`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added the distinct `0x433AA0` initialization-time backend refresh path. Group virtual `+0x14` calls each child's `0x42CC10`, which clears `+0x38` via `0x42BF20`, clears `+0x88`, and restores `+0x20..+0x28` from the saved `+0x64..+0x6C` constructor position. The new planner keeps missing saved positions explicit. Evidence `ev_61ffe05c018e4f962129e9e15238a38cda80b3f8f565bd7d66bba903a47db603`, `ev_a3edc180d35423e1da66e11a8136e73fdcc626dd5eeb7c6fcdf6f7cb659b56f2`, `ev_2346d3e4b93bc951df62e61e529ddbc932a33987a9cb5e8cac6d183b41adcf22`, and `ev_91ff43123834f5d91d1bb523fad7a7bf365bc2f74b1a70c5d7698af942a9b037`.
- **2026-10-23:** Expanded `plan_ncar_object_initialization` through both nested helpers. It now includes `0x4331E0`'s `+0xCBC/+0xD64` writes and `0x4338C0` child flags, four ordered `0x441B90` slot resets, the `0x433920` reset/resource/backend call, `+0xC0C` clear, and its second backend refresh. Bounded child-flag coverage and separate group calls remain visible. Evidence `ev_ef3f6561fe17c3e1792508c7b1e36db310029321c68de866642a978194a7b883`, `ev_4d766d96cc096b5309ab9cc09bd834cb5ebb54c48351ef07b01746dd1b022393`, `ev_fb90814359e3f3a42e262dd8130e8c47646bbba841228f159107d0d9f6fc4b91`, `ev_64947ff4e28fbdd3ac7435354d31fbc7ae4077144462f6e1393822fc720240ab`, and `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Corrected backend refresh composition when only constructor geometry is observed: it now generates the 12 child records with saved `+0x64` positions and projects their `+0x14` writes instead of treating all children as missing. Constructor allocation success remains an explicit assumption. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_ncar_vertex_maps` for the five resource meshes constructed by `0x42FF90`: group 0 identity indices and strict nearest-XYZ mapping from groups 1–4, including the `1e8` initial distance limit and 16-bit index truncation. Short spans and allocation assumptions are reported. Evidence `ev_fb568c133bd592fd606067f6b5921d7536ad52e13ad301ad1358df8c8efafbb4` and caller relationship `ev_2efcba011b644dcb156d6ab309061ada93575e542234ef4dce9ed2c82d7d2452`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Expanded `plan_ncar_body_constructor` with the inherited `0x4375C0` field initialization, eight ordered `0x43F460` writes, the 16 no-write `0x43F4A0` calls, and `0x43E650` finalizer clears and 16-bit increment. Evidence `ev_0b042fb4f6d77c50d28d57ecd3312bc6061e230cae69e8d682b71b1aa2b17a26`, `ev_ac9afffb7afcffe36e6d413ee5c362325f9a87a56808788e5bd0a17432fc241f`, `ev_aca9b2f651bc44518c585436809f7442545c9924c8390f910def2f7422fb5ec3`, and `ev_52c84de9710ca279934fee09bd5d66de457cd35450593accf6789cb90ebecf58`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_ncar_constructor_subobject_defaults` for the base constructor's simple embedded initializers: five `0x43F7D0` clears, the `0x43FA30` selector/base defaults, and three `0x43F6D0` linked-state blocks. Evidence `ev_65ae89a16cc6388298e63e4567d2149cfc732cb3781bdf7034580cd50b06a266`, `ev_9a82f989670fc71e23178542fe9d9779edcd836aa7e8f7dc7a82e36d2626eff3`, `ev_73f5b6ed9af9ed2d9eff36b17ac24ee521542e92d28b3d67fef257d25cc7f571`, and `ev_baa76dcbd6acd6420ebde137d4df5a9632759769c9a2c6679053d1a1fdca16d2`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_ncar_wheel_constructor` for `0x4419A0` and its four `0x42FF90` slots. It captures ordered vtable/default writes, the 1800/290 constants, reciprocal, and optional FDIV-helper-dependent fields. Evidence `ev_acb07d5cd1487043da9c665197a9902d94a44439d68a4c5285b4feeabba1255c`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended `plan_ncar_constructor_subobject_defaults` with the parent `+0xBFC` curve object's `0x440100` base/vtable sequence and observed selector/evaluator defaults, including the 72000 reciprocal. Later coefficient configuration remains in `configure_update_curve`. Evidence `ev_ec1e823ec1db45385ba5cd968dee035b79b242bf8d0d8ee5bd383829c95628e2`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended the constructor projection through the direct `0x42FF90` state tail: root vtable, mode/default fields, eight unaligned zero DWORDs, and the duplicated source-record `+0x65` value when supplied. Missing record input is flagged. Evidence `ev_fb568c133bd592fd606067f6b5921d7536ad52e13ad301ad1358df8c8efafbb4`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added `plan_ncar_sound_constructor` for the seven sound-related factory return slots at the end of `0x42FF90`, five conditional owner writes, and the shift-sound's conditional gain/pitch defaults. Factory handles, owner handle, and existing shift fields are explicit inputs; missing inputs are reported. Evidence `ev_fb568c133bd592fd606067f6b5921d7536ad52e13ad301ad1358df8c8efafbb4`, `ev_e63422f6b69cb8cfbe5794c88c36e8f0248ae5a054a4519fe73adc16c65c73cb`, `ev_48e21344f4b98a407c1a5e6228080475bf6cf3f7b24b653b95c865d050c90e34`, and `ev_919704cbfe71304dc119f67fa5f2a8498870e9aa7e592b6e2531f21cae79ade1`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Traced sound factory internals through `0x46BE60`, `0x46BF60`, `0x46C410`, and `0x46C750`. Corrected the shift object to receive the same `+0x10` owner write and documented wrapper allocation sizes, invalid-resource release gates, and the engine sound's zero-slot rejection. Resource contents and allocation outcomes remain explicit. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Composed the existing `0x403CA8` TestTrack dashboard path into `scene_dashboard_core_callback_plan` under the observed selected-object `+0x18 != 0` branch. Callback record `+0x138` now supplies the cached position and updated dashboard state flows into the TestTrack branch. Resource/locale inputs remain explicit. Evidence is the shipped disassembly at `0x403870..0x403CA8` plus the existing TestTrack dossier. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Extended the dashboard composer through the type-0 supplemental-row count gate and both shared metric-cache paths (`+0x10`, then `+0x14`). Supplemental rows are gated by selected type 0, false global `+0x40`, and displayed count greater than one; metric observations remain explicit. Evidence is the shipped disassembly at `0x403AFB..0x404147` and the existing per-row models. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Composed the final auxiliary `+0x4C` readout after both metric rows on every state-0 callback path, and fed it the shared redraw-all gate. Record/global values, glyph advance, and unresolved stack text stay explicit. Evidence is the shipped disassembly at `0x404147..0x404418` and the existing readout model. Release build and `git diff --check` pass; no tests were run.
- **2026-10-23:** Added an aggregate ordered text-call list to the state-0 callback plan. Traced the distinct null-font fallback through `0x44D6A0`, built-in 8x8 glyph drawing, the two pixel writers, and `0x449D70` pitch/coordinate addressing. The 96-glyph table already extracted by `tools/extract_scene_font.py` is now used directly by `raster_scene_dashboard_builtin_font`, which projects signed-byte `#` replacement, clipping, and mode-specific pixel values. Shadow/foreground passes remain sequential caller operations. Evidence `ev_ccbcc893f64ac589352c9f038c356fbad734e92ae8b8dc4986d8e80125a54a7e`, `ev_530a782028b247007aab203e8ab52d18b606f9b47ea2ac4fffd1c8904092d1ba`, `ev_6e1559c698437961095f7e96979855145b53c582720b4a5e5dc8801d71e83ac4`, `ev_53609ca437c9fbc09290b9b318bc48491c39867f51ecade530a793b1c2128e48`, `ev_fbc394d4ee7c793a2f5d41a8a3cb69e18f4f8ce02173603c4ed062669dccde85`, `ev_52a6355700fc4b4eeddb773cb0d04e767683d5659a32e8fc9bb815c4668f88cd`, `ev_72b883caf50dce68e3681752d8daec12677322290498a3b2a0999ac1b683abb7`, and `ev_403930b259723ca4ddf0f8c3acfa85ffd5ccf5eb624e30cf2abdd4f3ade55245`. Release build and `git diff --check` pass; no tests were run.


- **2026-10-23:** Decoded the `0x404970` results-branch horizontal geometry from the shipped disassembly: `0x404EA0` spills `[0x509A4C]/2` (surface width/2) to the stack and `0x404FF0`, `0x405009`, and `0x40505C` re-read it, so the three row columns are `W/2-0x91`, `W/2-0x6B`, and `W/2+0x41` with no caller seeds; `0x404F71` adds `0x14` to the row baseline before the loop, so rows start at `(H-0xB4)/2+0x11`; and the two previously implicit `0x44DC70` heading texts decode to the title `0x4EB24C` at `(W/2, (H-0xB4)/2-8)` and the ESC prompt text `0x4EB414` at `(W/2, (H-0xB4)/2+0xBF)`, both flags `0x24`. Corrected the swapped `text_width_helper`/`text_height_helper` constants (`0x44D450` is width, `0x44D460` is height) in the heading plan. Argument order `(font, color, flags, x, y, format)` confirmed against the `0x4048E0` right-aligned draw and the `0x404920` height use of `[0x509A2C]`. Release build and executable smoke test pass; no tests were run.

- **2026-10-24:** Fixed the results-row raster path for the ordinal column: the `0x4CFF38` `%d` call now formats its signed integer argument before glyph layout, so ordinals emit glyphs alongside record and time strings. Font/atlas bytes and renderer state remain explicit inputs. Release target builds and `git diff --check` pass; no tests were run. The call/format evidence is from `0x404970` (`ev_76ecfc938780e22fc3861f3c54d3e690ccdd6d00a0cced2adf06bb05ec45a292`).

- **2026-10-24:** Added `raster_scene_shared_results_heading_text` to connect the state-4 title and prompt calls to font layout and atlas rasterization in draw order. It validates the two observed format handles and renderer-observation count; localized text and renderer/resource state remain explicit. Evidence: `0x404970` (`ev_76ecfc938780e22fc3861f3c54d3e690ccdd6d00a0cced2adf06bb05ec45a292`). Release target builds and `git diff --check` pass; no tests were run.

- **2026-10-24:** Recovered the state-2 wheel-bar label format from bytes at `0x4D4C54` (`%1.0f`) and the call through VS 1998 `_sprintf` at `0x4BD190`; each wheel-bar text operation now retains the formatted string alongside its source and numeric argument. Renderer text submission remains outside pixel rasterization, and legacy rounding is approximate. Release target builds and `git diff --check` pass; no tests were run. Evidence: `ev_df8f7585aecb66ed48b18aeb711c44a9c3ea21feaa77661a3660fe1c5b941138`, `ev_d511af6851b2d17d5a5a65129cda2a02ca874f07aa7a7bb722bc0df633f5698c`, and `ev_8ea0d058927243f23b31fa69d01a4bcf40c15e14d826b47e3affabc9e68dfe54`.
- **2026-10-24:** Connected the four state-2 wheel-bar formatted labels to the shared text layout and atlas rasterizer. Style/color/font/flags, font tables, atlas contents, and per-label renderer observations remain explicit inputs, so the helper projects pixel writes without pretending to recover runtime DirectDraw resources. Release target builds and `git diff --check` pass; no tests were run. Evidence: `ev_8ea0d058927243f23b31fa69d01a4bcf40c15e14d826b47e3affabc9e68dfe54` and `ev_cb05306e5d275df4cc6d96917141986b444a6bad6fee1922abc2d3b120591923`.
- **2026-10-24:** Extended `advance_four_wheel_body_step` with the leading `0x431150` effects: the zero `+0x4FC` reset clears effective X/Z linear velocity before subsequent force and collision projections; the `+0xE3D` branch derives point `(0,2,0)` and its velocity, then accumulates the observed vertical force; the nested contact sum records the `+0x50` virtual callback gate. The callback's native effects and adjacent timer/control calls remain observations. The six-record REA evidence bundle is retained at `metadata/ncar_integrator_prefix.evidence.json`. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`, `ev_4e237485e955a5ada3411b37c141b93778bcfda352b438d4c0414812405a213a`, and `ev_985b150bd698d190e02732dbb8ae7b75ef0248366c9a97b122fdf5308da4f4ce`.
- **2026-10-24:** Added `advance_ncar_integrator_timer` for the adjacent `0x433570` path and composed it into `advance_four_wheel_body_step`: it preserves the x87 6.2-second gate, signed raw-DWORD 6.0 cutoff, binary ramp constants, and ordinal-record distance gate before adding 0.016 to `+0xE44`. Missing runtime record lookup marks the step incomplete only when that lookup is reached. Seven REA evidence records are retained at `metadata/ncar_integrator_timer.evidence.json`. The Release target builds and `git diff --check` passes; no tests were run. Evidence: `ev_b33d1d148f26718f4435cef4a33eef66d10e0d9c58e27c577477ccb2cce4b1f8`, `ev_158aba98f08b44d3788819fdf2c99714a496b272bb50286b61cbc3dee5d00fa2`, and `ev_4394f76cfa93de76984db3bbd98bb3696a7ab8b4159da55951abcbc5f7aabaef`.
- **2026-10-24:** Composed `0x432130`'s exact raw-bit position ranges into the same pre-wheel step. It returns zero in range; the caller reaches global interface slot `+0x3C` for any out-of-range position and forwards the vehicle ordinal. This preserves strict bounds and native non-finite comparison behavior; the callback's effects remain unresolved. The Release target builds and `git diff --check` passes; no tests were run. Evidence `ev_76372b1d20911ae6c6596f9fc9987110b1639312dde8af814ff1145e92ab0d56` is retained in `metadata/ncar_integrator_timer.evidence.json`.
- **2026-10-24:** Added the next `0x431150` curve gate: signed `+0xC7C` threshold and parent `+0xC3D` select `0x4402D0`; child `+0x41` gates its `+0x40` write into parent `+0xC3C`, whose nonzero value triggers `0x4331A0(1.0)` at `+0xDE0`. Corrected `advance_pcar_callback_tail` to include the same child-byte condition instead of setting `+0xC3C` for every helper call. Evidence bundle `metadata/ncar_integrator_curve_gate.evidence.json` retains the helper and setter analyses. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_96c88cabb88c7c78ae26cb2ae250b2939ebe2a35e35c952ee28e62aee3be1e0a`, `ev_c4b47e34375fbf4bd3cb556c06f630cca74cfbc46dce80a77aed30a155152827`, and caller `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`.
- **2026-10-24:** Connected the pre-wheel `+0xC98` controller sequence in `0x431150`: direct car `+0xDE0/+0xDEC` values seed child `+0x64/+0x5C` in order, then the existing `0x43FAE0` state machine runs with its runtime callback and target inputs supplied. The current `+0xDE0` is taken after the curve-gate setter. Release build and `git diff --check` pass; no tests were run. Evidence: caller `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`, `0x43FF80` `ev_1128dc7f406320fc5ef2f1ed4675611a89fdc97effbfc2c22b8811268e06d4ea`, `0x43FF70` `ev_498d3e18c8202f1635d22ed6fdab875a921c74ff081aee95b6a8369055328bf2`, and `0x43FAE0` `ev_e3e3d51d09604ce173a3a18906c819e7484977c6a91f49f3edb6d87a451e26bf`.
- **2026-10-24:** Composed the next `0x431150` pre-wheel slice: callback-times-scale float store at `+0xDDC`, `+0xC3C` to `+0xE49`, the signed raw `+0xD70` threshold into `0x43F8E0`, and the always-called `0x43F830` clamp/smoother with opposite paired setter arguments. Then recorded the `+0x4C` four-value callback result, conditional sign reversal, and strided wheel `+0xE0` stores, feeding those values into the wheel spin-response stage that consumes `abs(+0xE0)`. Virtual returns and setter effects remain explicit. Five caller/helper/coefficient evidence records are retained at `metadata/ncar_integrator_signal.evidence.json`; `0x442530` consumption evidence is at `metadata/wheel_update_value_e0.evidence.json`. Release build and `git diff --check` pass; no tests were run. Evidence: caller `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`, `0x43F830` `ev_561bda702abc7970b5b820be76dc22cedb78968c7da2d82ede95809777fcf6c1`, setter `0x43F8E0` `ev_e1224f8c92d66a13ee1793d2d6ed81095204c66493ad2a6b1423e4e65cc60352`, coefficient bytes `ev_c3a729ee52e88cb9fda90eb175a7221995a84e0f6df71064034ccfebfdf3c791`, and `0x442530` `ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`.
- **2026-10-24:** Resolved the `0x431150` virtual `+0x4C` wheel-buffer callback to `0x432170` from the concrete vtable entries at `0x4C7F6C` and `0x4C858C`. Although Ghidra had no procedure boundary there, the 384-byte shipped instruction range terminates at `0x4322F0`; its speed-scaled tangent response, `0x4C7E2C` threshold, selector scales, signed lane gates, and max stores now replace the previously supplied callback output. The caller still supplies car fields absent from the current typed step state; body velocity and angular Y are shared from the active body state. Twelve evidence records are retained in `metadata/ncar_integrator_wheel_callback.evidence.json`. Release build and `git diff --check` pass; no tests were run. Evidence: caller/vtable xrefs `ev_dccdf77217982350f7d2c9eb2dcb8e2abb98db05b031d85ef18a10980a4a2048`, vtable bytes `ev_a34c4987793f15df84b5d682ce22fee74ca79fbcca4cab0f9d6306524f2fb6c1` and `ev_29ec14e478481eec2cfdbc84d33dccb954d330923e9e1e7401c39ec6d9c2b79e`, callback bytes `ev_8ee864625812dc2ba5d96c8328561c59a742d524d6141a10b60f2c4561d7b675`, tangent wrapper `ev_e9e77b5149542291536b8cb86cea3884a5a1b62dc7ad139d0db2b3b05aed9e63`, and threshold `ev_6d5ea23643a90a575a149f76c32b5f74be9a0a8ac88941fb6d16117dbffe7b8b`.
- **2026-10-24:** Added `dispatch_ncar_interpolated_object` around the recovered `0x427630` selector: object flag gates, indexed `0x41D1C0` target writes, runtime `0x420930` transform copy, global virtual result, signed time gate, bounded record update composition, tail virtual call, and 16-bit counter increment. A known null target stops the projection at the native dereference; runtime pointer/table contents remain explicit observations. Evidence is retained in `metadata/ncar_interpolation_dispatch.evidence.json`; Release build and `git diff --check` pass; no tests were run. Key instruction evidence: pre-gates `ev_bf261e2fa09c26135619f99442a57bf1436bcce71879c686be51293a594f413e`, `ev_89236eac738efbe4f778302b7c29d3773a46357b4c58ec0219e084141f95acaf`, indexed target `ev_9d0106572d2f4dcfab32cc11a32a832faeeb9c5da7b829dc8f1b0223712bb8c5`, transform gate `ev_8da73efc08b115b981dbdd0dfeee9aea395da752b8d2d7034c309e2922552aa5`, callback/time gate `ev_8f9cddb7ba8487593193d2d203028696d98d267b69869df8121e340b0fab8958`, `ev_b7dbd33b8e02f54c8672f21a4df2000ce080e070188aa0d4954d9ab09c6e4c61`, selector call `ev_85c6eb02fa91005b7ce530236672d8f5f3942054e7ff23c2f2b57deaa060e373`, and tail counter `ev_56d03dcd2ac7a488f7183e295c8ffd854b1dd2f99fd8d73eb87898d59907c7f8`.
- **2026-10-24:** Traced the runtime registry used by the interpolation wrapper. Added bounded projections for `0x4208F0` registration and `0x420930` lookup of the 16 pointers initialized by `0x420480`; documented that the apparent lock wrappers and lookup helper are `RET` no-ops, while native registry accesses are unchecked. Reconstructed the pairwise loop in `0x421050` and implemented `apply_ncar_registered_object_influence` for `0x432300`'s virtual gate, strict radius comparison, legacy FDIV weight and `+0xDFC` vector accumulation. Connected the registry snapshot and null/unbounded access outcomes to `dispatch_ncar_interpolated_object`. The 11-record evidence bundle is `metadata/ncar_registered_object_influence.evidence.json`, and all seven cited IDs in that section are present. Release build and `git diff --check` pass; no tests were run. Key evidence `ev_4a390a130f8316b24f5cb6541adbf054b784337d070a8b265edb576ab64fe7e6`, `ev_2c7a5e0fcc3afb15362b41c8f54797c4b9184e099a16a5ecf4fe6a78f1462f71`, `ev_aefa6a2d466f478adf238aafc416bc8b2096abc5d5a58260a34dffb29dd767a`, and `ev_093a726660d0859203c394c4b6a10b413bfe3b6bc47d73582e1b7837d83f1bf4`.
- **2026-10-24:** Consolidated the pairwise implementation into the existing physics-task module, removing the duplicate `0x432300` primitive and adding `plan_physics_auxiliary_pass`. It now derives all non-null ordered table pairs, carries receiver accumulators across each row, identifies missing/duplicate pair observations, and stops on an observed target dereference fault; the alternate-time branch delegates to the existing `0x426A10` model. Both NCAR dispatch and physics-task planning now share a common 16-pointer `RegisteredObjectTable` type. Release build and `git diff --check` are pending; no tests were run. Evidence remains in `metadata/ncar_registered_object_influence.evidence.json` and `metadata/simulation_loop.md`.
- **2026-10-24:** Corrected command IDs 5 and 7 to use the signed index into the 16-entry registry at `0x507D70`; documented ID 3 payload copying and ID 1 signed ring-cursor behavior. Followed ID 1 `0x435FE0` callback and projected its active-entry vector writes, scalar equality/change gate, and flag updates. Evidence is retained in `metadata/physics_time_handler_fragments.evidence.json` and `metadata/physics_time_handler_effects.evidence.json`. Release build and `git diff --check` pass; no tests were run.
- **2026-10-24:** Narrowed the `0x426000` physics-track selector source: the `0x420480` startup caller obtains `&DAT_0053B17C` through `0x45C2C0`/`0x45C270`, and the shipped image has zero at selector field `+0x18`. Direct references contain one read at `0x45D9E2` and no store; the checkpoint parser also uses this field as its `.mod` versus direct-registration gate. Runtime or indirect mutation remains unresolved, so the selector is not hard-coded in the general planner. Nineteen evidence records are retained in `metadata/physics_track_selector_source.evidence.json`. No score change.
- **2026-10-24:** Connected the raw `0x53B194` value across the `0x426000` physics-track service selector and `0x45D950` checkpoint visual-versus-serialized route. `CheckpointTrackLineObservation` now preserves the raw DWORD rather than collapsing it to a route-specific boolean; runtime value remains explicit. Release target build and `git diff --check` pass; no tests were run.
- **2026-10-24:** Fixed the state-0 dashboard composition so supplemental time-row eligibility and row positions both use the common Position callback result (the `+0x140` display value and adjusted entity count), matching the shared raw `0x4035E0` flow. The nested helper still supports direct caller observations; the composer now overrides the redundant aggregate inputs. Release build and `git diff --check` pass; no tests were run. Evidence: `ev_9aa46bc6450ec1572ea865ac9148625242d6d5ef645c945de5d2100e78982c22`.
- **2026-10-24:** Corrected the supplemental-row count boundary to match the native signed `JLE`: the DWORD decrement wraps first, then its bits are interpreted as `int32_t`, so zero decremented to `0xFFFFFFFF` correctly fails `count > 1`. The outer race-loop flow now mirrors the core callback's supplemental branch/result rather than recomputing it from a duplicate observation. Raw instructions `0x403680`, `0x403AFB`, and `0x403B00` are retained in `metadata/dashboard_supplemental_count.evidence.json`. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Connected the state-0 renderer's dirty-mask reads to the state updated by the core dashboard callback. `0x405100` enters its full-redraw path only when `0x4EB40C == 0xFFFF` and gates each of the nine optional rectangles by one mask bit; both the render-operation plan and clipped full-surface fill now use that same rule. `plan_race_loop_shared_overlay` feeds the updated core callback mask into the render plan. Evidence `metadata/dashboard_render_dirty_mask.evidence.json` records the native compare and mask test. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Composed the existing paused-banner fallback-box rasterizer into the shared-overlay frame result, preserving outer/inner fill order and pixel writes. The race-loop path now uses a successfully acquired context's viewport and surface base when present; the state-0 renderer supplies the shared mode, while state-4 mode/context remain explicit inputs when acquisition was not composed. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Connected the results heading and selected result-row text rasterizers to the race-loop shared-overlay result branch. Rasterization now runs only when the raw race-results predicate passes, preserving heading-before-row ordering; localized strings, font/atlas data, and per-call renderer observations remain inputs. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Added `raster_scene_shared_text_effects` for the two gated `0x44DC70` state-4/shared text calls and composed it into the race-loop overlay after the shared gates. It requires localized text only for active calls and retains wrong-way-before-secondary-flag order; font/atlas and per-call surface observations remain inputs. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Connected the state-5 `0x422A70` numeric overlay to the active-frame context-taking callback. It is planned only when that callback is reached; successful surface acquisition supplies the framebuffer base, pitch, and clip bounds used by the existing background and glyph-write projection. Matrix/camera globals, renderer mode, and colors remain observations. The Release target builds and `git diff --check` passes; no tests were run.
- **2026-10-24:** Rechecked the `0x43ECE0` basis-sync stack path against instruction-level evidence and corrected an earlier false claim that two matrix words came from prior stack contents. Accounting for the three still-pushed arguments and `0x402DD0`'s local stack frame shows the nine-float copy initializes both later reads. Added `build_body_integrator_local_basis_matrix` to reproduce the overlapping stores and use the derived local matrix in the full kinematics composition. The exact x87 rounding and virtual callback effects remain open; Release build and `git diff --check` passed, with no tests run. Evidence bundle: `body_integration_basis_sync.evidence.json`.

- **2026-10-24:** Replaced the unsupported caller-supplied virtual-record accumulator delta with a bounded PCAR/NCAR +0x38 callback model for 0x430BF0: global update gate, strict normalized-force threshold, timeout state/time writes, exact body transforms, and all 14 type-5 event payload bytes derived from the native field arithmetic. The event helper call is modeled; nested 0x430610 mesh/resource changes remain open. Release build and git diff --check passed; no tests were run. Evidence: 0x430BF0 ev_87738c6e1979745e751f9357b19ee919aadf3b5e649373d76b826a4db0238731, event helper ev_4b5569385e553dacbb0ddc508b7d9467e3208f081cc0b277ac6b27ad9a05d436, nested handler ev_1d43c9b43216ba441eb9a29e9156ce92b767363305dda9c9f0449178b15faca5.

- **2026-10-24:** Modeled the `0x430610` secondary impact handler through its two `0x430B70` UV-band mask calls, bounded main-mesh vertex deformation, five indexed XYZ group copies, and descriptor pointer/flag refreshes. Verified `0x450B40` count/data layout and `0x450B20` descriptor identity path; zero-length groups no longer require an index view. Main/group mesh views and descriptor identities remain runtime inputs, and unsafe index/mask writes are surfaced as incomplete. Release build and `git diff --check` pass; no tests were run. Evidence bundle: `metadata/ncar_integrator_virtual_record.evidence.json`.

- **2026-10-24 continuation:** Added `apply_race_loop_paused_banner_to_locked_surface`, which validates submission, callback, and unlock ordering before applying the merged state-4 banner write batch to a caller-owned locked-surface span. Release build and `git diff --check` are pending; no tests were run. Native surface lock lifetime, unlock effects, and flip remain outside the model.
- **2026-10-24 continuation:** Added ordered state-0 `0x405100` background application for the optional full-surface fill and projected bitmap draws. The race-loop helper requires the submitted `0x4052D0` callback before unlock and prevalidates every destination before mutation; source bitmap bytes and native DirectDraw lifetime remain external. Release build and `git diff --check` pass; no tests were run.
- **2026-10-24 continuation:** Routed successful acquired-surface context into the state-0 gear plan and added the mode-0 gear rectangle to the same ordered batch after `0x405100` background writes. Rotating triangle/edge writes remain projected separately; Release build and whitespace check are pending; no tests were run.
- **2026-10-24 continuation:** Added optional rasterization of the caller-resolved `0x4EB420` STMP gear resource and merged its writes before the rotating triangle fill and outline pixels. The resource handle-to-bundle-image mapping remains an explicit input; missing image data prevents partial surface mutation. Release build and `git diff --check` pass; no tests were run.
- **2026-10-24 continuation:** Resolved the gear image mapping: initialization registers the string `rpm.stp` at `0x4CFBDC` into `0x4EB420`, and `race.res` contains a complete 128x128 single-frame STMP image. Added `parse_state0_dashboard_gear_image` to decode it directly from a caller-owned `ResourceBundle`; connected rendering behavior is unchanged. Release build and `git diff --check` pass; no tests were run. Source: `scene_dashboard_initialization_plan`, `metadata/strings.md`, and `--inspect-image race.res rpm.stp`.
