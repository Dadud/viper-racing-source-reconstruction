# Engine function tracing queue

The engine text section contains more than 12,000 candidate control-flow
starts, many of which are basic blocks rather than functions. Priorities here
are driven by call reachability and subsystem evidence.

| Priority | Address | Evidence / reason |
|---|---:|---|
| 1 | `0x4BD690` | PE entry point. Ghidra pseudocode shows `GetVersion`, CRT/environment setup, command-line tokenization, and a call to application handoff `0x411F10`; it does not show a performance counter. |
| 1 | `0x411F10` | Application-owned startup handoff. `plan_application_startup_handoff` now composes the dedicated/non-dedicated branch, both early gates, initialization result, frontend condition, shutdown and handle-finalizer reachability; native API outcomes remain inputs. |
| 1 | `0x4120A0`, `0x412050` | Startup gates called before the dedicated/non-dedicated branch. |
| 1 | `0x412270`, `0x416080`, `0x416360`, `0x412120` | Non-dedicated path is composed in `plan_application_startup_handoff`: loading-window geometry/gates, VxD open/overlay probe, staged initialization and cleanup, config path derivation, ancestor-directory CreateDirectory outcomes, and downstream parser/frontend reachability are modeled. Native API outcomes and helper side effects remain inputs or separate projections. |
| 2 | `0x401460` | Startup-mode switch is now modeled; the called window/frontend handlers remain unresolved. |
| 1 | `0x401E10` | Repeated state dispatcher; numeric selectors and the shared panic fallback are reconstructed, while the deep behavior of its state handlers remains open. |
| 1 | `0x4805C0` | Builds the six visible main-menu entries; selector routing, eight one-time label registrations, default-name/six-label cache refresh gates, and result normalization are modeled. Resource-manager effects, callback execution, localization contents and other UI details remain open. |
| 2 | `0x40A410`, `0x47B4E0`, `0x406950`, `0x41C870` | `0x40A410` participant rows, setup-screen outputs/settings, resource order, per-row dispatch, human `0x45EFD0` `.csu` loading and `.ccs` fallback chain, the `0x45EFD0`/`0x41A000` wrapper order, and the AI car-file coefficient path are modeled; replay-screen controls, `0x407760` load path with the `0x426790` format parser, `0x407C60` save path, and the `0x408160` analysis page's 25 control descriptors, eight one-time labels, eight conditional cache refreshes, and exit semantics are projected. Resource-set acquisition under `0x417740`, RST0 load/mapping, per-resource reference release, and sweep unmap/remap callback effects are now projected. Remaining: native replay read/write outcomes, uncovered vehicle-record effects, runtime handle-table and native OS outcomes, unmodeled callees within `0x401EA0`, and other menu/gameplay handlers. |
| 2 | `0x40A570` | The multiplayer loop, TAPI/event initialization and 260-slot shutdown, connection-object worker-ID gate, unowned worker callback fragment at `0x49BD40`, direct/line transport construction/retry branches, numeric-state supervisor, 22-entry status label table, and `+0x84` per-state flag lookup are modeled. Protocol transition effects beyond those observed branches, native transport results, lobby state, and thread runtime effects remain open. |
| 2 | `0x401930`, `0x4203E0`, `0x421120`, `0x4228C0` | Page-11 camera movement, `camera.tab` record parsing, modes 0-12, preset pose writes, nearest-target mode-10 selector and mode-12 record selection, and shared camera handoff are modeled; terrain query flow through `0x45F760` and the `0x45F600` world-height wrapper is traced. Camera file values and the selected-car transform remain runtime inputs; the near-vertical fallback now uses transform elements 6 and 8. |
| 2 | `0x420480`, `0x4224D0`, `0x422580` | The nine-tag `0x422580` dispatch, one-record `0x4224D0` reader, `0x420480` bounded array loop/global stores, track-service selector/factory, `0x426EB0` table initializer, `MIL` resource registration, `0x421050` outer callback order, `0x427080` slot selection/callback dispatch, `0x4272C0` ring transition, and the stateful `0x432300` ordered-pair accumulator pass are modeled. Selector-to-virtual-`+0x0C` targets and callbacks for selectors 0, 1, 3, and 5 are modeled. The `0x426A10` handler table's registration/removal and known command IDs are modeled; handler code fragments, resource constructor internals, serialized bytes, allocation outcomes, and exact x87 extended-precision results remain open. `0x426A10` slot interpolation and object dispatch are modeled. See `metadata/physics_task_objects.md` and `metadata/simulation_loop.md`. |
| 2 | `0x427B10`, `0x432420`, `0x437650`, `0x444240`, `0x433920`, `0x433AA0`, `0x4331E0`, `0x4338C0`, `0x441B90`, `0x44F750`, `0x44F9C0`, `0x42CCE0`, `0x42D0A0`, `0x42CC10`, `0x42BEB0`, `0x42CD90`, `0x433C00` | Per-object sampled-state interpolation, axis-angle transform reconstruction, four wheel payload updates, reset-field/resource-group effects (including resource-entry `+0x10` pointer writes and `+0x15` flag clears), reset group sphere-child transform/bound, and `0x433AA0` ordered initialization through child flags, four wheel resets, its `0x433920` reset/resource/backend call, and both backend refreshes are projected. Other child constructor payloads, wheel animation/contact inputs, and x87/libm precision remain open or approximate. See `metadata/physics_interpolated_object_update.md`. |
| 2 | `0x42FF90`, `0x43E490`, `0x4375C0`, `0x43F460`, `0x43F4A0`, `0x43E650`, `0x43F7D0`, `0x43FA30`, `0x43F6D0`, `0x4419A0` | Base NCAR constructor now has bounded five-group vertex maps; its body initializer projects inherited field writes, eight ordered subobject writes, sixteen no-write identity calls, finalizer clears/increment, four scaled dimensions/reciprocals, and copied matrix. Curve, selector, linked-state and wheel defaults plus the direct constructor tail are projected; source-record `+0x65` remains an optional input. FDIV helper values and sound allocation/resource lookup remain runtime inputs. See `metadata/vehicle_setup.md`. |
| 2 | `0x4AA5E0`, `0x4AC020` | Career completion suffix, guarded event-cursor advance, per-event payout/standings writes, season-derived payout/points tables, aggregate score ranking, and the pre-race estimated entrant ordering pass are modeled. Eight `0x401EA0` result rows compose into ranking; recover live profile values, RNG state/results, actual race results, and remaining season fields. |
| 2 | `0x4AEDB0`, `0x4AEC30`, `0x4ADB00` | Testing selection, two race modes, the `0x4AEC30` context/result loop, season reload, money-label indexing, and season track/payout/placement-point fields are modeled and connected to career ranking. `0x4AEC30` reads career-payload offsets `0x10`, `0x14`, and `0x124`; separately, `0x4AA5E0` reads the current SEAS event's `0x10` low byte and `0x14` dword. Their semantics and runtime race-result inputs remain open. |
| 2 | `0x402BE0`, `0x402A90`, `0x402A80`, `0x402AA0` | Mode 1/4 record string copies, mode globals, mode-4 12-word global copy, and ordered session calls are projected by `plan_startup_mode_entry`; modes 2/3 are modeled as no-op and init/cleanup. Intermediate field semantics and UI effects remain open. |
| 2 | `0x4BD7E3` callback fragment | Whole-text disassembly shows a separate SEH callback fragment ending with a call to `0x4C1300`; this byte range falls in a gap between Ghidra's recognized entry body ranges. |
| 2 | `0x4C1300` | Disassembled exception-dispatch code label, not a Ghidra FunctionManager entry. Maps floating-point exception codes to internal values and calls a registered category-8 callback. |
| 2 | `0x4BFA90` | VS 1998 `_sprintf` implementation; its null `%s` substitution supplies the dashboard's observed `"(null)"` text. |
| 2 | `0x45DE40`, `0x45DEB0`, `0x45DFB0` | Initialize, set, and return rows in the 16-entry `0x170`-byte string table; supplemental-row indices resolve to `+0x05` text. Constructor fields at car-record `+0x04`/`+0x11` feed the setter; the `0x49C5C0` gate resolves its `Race's ChatLine` virtual `+0x84` result through `0x4A1B70`'s bounded `+0x1C8 + index*0xD9` byte read, with state 3/6 derived from the `0x49FA00` lookup result. The callback now reports the 22-state label ID and conditional no-op helper call. Remaining gaps are one-time status-label registration effects, runtime row contents, and broader multiplayer protocol transitions. |
| 2 | `0x465800`, `0x4622F0`, `0x462380`, `0x4623B0`, `0x4623F0`, `0x462600`, `0x462590`, `0x462940`, `0x462960` | Scene-car edge events, pool-manager lifecycle, auxiliary update, circular allocation, outer gates, sibling releases, and indexed rendering are modeled. Constructor registration and generic renderer `0x45BEB0` connect virtual `+0x14` to `0x4623F0`; the generic frame's enable gate, camera-transform writes, viewport/state ordering, and bounded `+0x10`/`+0x0C`/`+0x14` object dispatch are now projected. The viewport's device `+0x44` call, 0x2C record, raw buffer-clear flags and HRESULT logs are modeled. Wheel-manager indexed draws now explicitly forward through `0x448ED0`/`0x452700` to device vtable `+0x78`; actual device execution remains open. Pool sizing is traced to `skids` and selected PCAR. The per-wheel `+0x34` bit's update/release role and state 0/1/2 lifecycle are explicit; semantic labels for the 16 input effect masks and remaining device effects remain open. |
| 3 | `0x4C2175` call site | Calls `0x4C3AF0` from an error/translation path; not the application startup handoff. |
| 1 | `0x44E080` | `plan_directdraw_startup` now projects ordered device discovery, DirectDraw fallback object/capability selection, the `0x458D60` mode-enumeration call and `0x44EE00` five-resolution callback with 640x480 requirement, exclusive-mode result, distinct fatal exit codes, and success state reset; native COM/device outcomes remain inputs. |
| 2 | `0x44E460` | Its device-discovery, fallback, required-mode and `0x458D20` exclusive-mode path is included in `plan_directdraw_startup`; COM object and display state effects remain external. |
| 2 | `0x458470` | Creates DirectDraw and queries DirectDraw2; failure messages identify exact stages. |
| 2 | `0x458940` | Queries interface with GUID at `0x4C8B40`; failure string names IDirect3D2. |
| 2 | `0x458E0D` | Surface query path with IDirectDrawSurface3 failure string at `0x4DBAE8`. |
| 3 | `0x4BAA77` | Separate movie/intro DirectDraw path; keep distinct from main renderer until call graph says otherwise. |
| 3 | `0x4C1850` | Entry calls this before command tokenization; builds environment-related state via dynamically resolved APIs. |
| 3 | `0x4C15D0`, `0x4C14F0` | Entry helpers that build environment/argument-related state. |

Recommended next tracing pass: continue state 0 by resolving remaining
dashboard call gates and renderer/device effects. The selected-type/TestTrack,
supplemental-row, two metric-cache, and final auxiliary-readout paths are now
composed into the `0x4035E0` callback model. The `0x44DC70` font-backed
layout and `0x44D0C0` atlas RLE raster projection are now modeled through
clipping and 16-bit framebuffer writes; state-0 `0x44DB10` text width and
`0x44DC50` extent now have bounded input implementations. The null-font 8x8
bitmap path, extracted glyph table, and two color writers are modeled;
actual device/surface ownership and remaining callback gates are still open.
The `0x418500` locale
initializer is now modeled, including both selected 13-DWORD tables and the
raw sources of the metric multipliers; the target formatter's null `%s`
substitution is also recovered. Then trace state-4 banner
placement and renderer effects beyond the modeled paused layout, centered
`0x404920` resource draw, cheater, two raw-flag text draws, results heading,
and result-row selection/time-format/draw paths now modeled. State 5's text and rotation
math are modeled at portable precision; continue with its renderer effects and
remaining x87/FDIV fidelity. The `0x426000` factory's `param_1[6]` selector
source remains open:
inspection of its accessor's direct callers found no observed writes, while
indirect or external writes are not ruled out. State-1/state-2/state-3
overlay text and key behavior have evidence-backed portable models. State 0 now
has viewport, Position/Lap/RaceTime/LapTime/TestTrack text transitions, type-0
supplemental rows, Best/Last metric formatting and registration, and the
record-derived auxiliary readout modeled. The 16-entry `0x45DEB0`/`0x45DFB0`
string table and its supplemental-row index path are modeled; the 200-byte
car-record string offsets and the indirect multiplayer `+0x84` method's row
byte are now traced. Runtime record values and callback timer/log effects
remain open.
Continue resolving other caller-supplied row gates, record/global
field sources, and dashboard drawing effects. State-4 result-row drawing and
paused-banner layout are modeled;
remaining banner and renderer effects remain open.
State-2 wheel geometry and wrapper-call order are now projected, while pixel
rasterization remains open; state-3 fill and text call order are projected,
while renderer/device effects remain open.
Label semantics, numeric type
meanings, and most callback bodies remain open. The `0x41AB00` lap window,
scoreboard draw-call layout, state-1-to-state-3 gate, `test_prompt` text calls,
and three-success `0x41F4C0` helper are modeled; glyph rasterization and the
remaining callback bodies are open.
The VxD timer thread and codes 1, 2, and 3 are traced in `legacy_vxd.md`; the
external driver implementation and its response semantics remain unavailable.
Treat candidate CSV boundaries as leads only; confirm each function with the
provider's body ranges and call relationships.
