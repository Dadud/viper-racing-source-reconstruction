# Race input binding evaluation

The race update at `0x43B290` evaluates many input-binding records through
`0x446980`. The records begin with a type DWORD and selector DWORD. Its
boolean wrapper `0x446F40` reports active only when the evaluated value is
strictly greater than `0.2` for ordered values; equality is inactive. The
decompiled flag expression also makes an unordered/NaN value active.
`0x446F90` is a separate record-state predicate: it returns true for type
values 1, 3, 4, and 6.

`include/viper/race_loop_callbacks.hpp` and
`src/race_loop_callbacks.cpp` implement `evaluate_race_input_binding` and
`race_input_binding_is_active`. The direct observed mappings are:

| Type | Behavior |
| --- | --- |
| 1 | Returns the sampled result of `0x413170(selector-byte)` as 0 or 1. The selector's device lookup remains an input observation. |
| 2 | Selectors 0–7 return the negative/positive half-axis values for four controller axes, floored at zero. Selectors 8–11 map axes to `(1-axis)/2`; 12–15 map to `(axis+1)/2`. |
| 3 | Selectors read the corresponding byte at `+0x5098A0`, populated by the controller state refresh from DirectInput button bytes. |
| 4 | Returns 1 when the selector equals the POV-derived direction at `+0x5098C0`, otherwise 0. |
| 5 | Selectors 0–3 return negative/positive mouse-axis values floored at zero; 4/5 map mouse X to `(1-x)/2` and `(x+1)/2`; 6/7 do the same for mouse Y. |
| 6 | Selectors 0, 1, and 2 read mouse-button bytes 0, 2, and 1, respectively. |

Unknown types and out-of-range selectors return zero. This evaluator consumes
sampled state and does not reconstruct the platform input polling that fills
it. Type-1 selectors index the supplied 256-entry sampled result table by
their low byte, matching the byte load at `param_1 + 4`.

The type-1 selector is the keyboard device's DirectInput scan-code index, not
the Win32 virtual-key number. `0x412F90` creates the system keyboard and sets
its data format with foreground/nonexclusive cooperative flags (`6`), then
calls Acquire. `0x4130D0` polls `0x100` bytes into `DAT_004EF200`; on success it
refreshes three combined key bytes at `0x004EF2FD`–`0x004EF2FF`. `0x413170`
reads the selected byte's high bit. The Windows runtime now owns a
DirectInput8 system-keyboard service that requests the same 256-byte data
format, polls it once per outer race update, and attempts reacquisition after
`DIERR_INPUTLOST` or `DIERR_NOTACQUIRED`, returning without retrying until the
next update as the original does. The original sends other polling errors
through its diagnostic/error routine; the current service reports only a
failed sample and does not reproduce that diagnostic path. The runtime converts parsed
virtual-key bindings through `MapVirtualKeyW` and maps an `E0`-prefixed scan
code into the DirectInput extended-key range. Its WM key-event snapshot is
merged as a fallback, which also supports deterministic posted-event smokes.
`game_keyboard_binding_smoke` confirms the binding/evaluator/control path and
fallback behavior; it does not prove that a physical key was observed through
the DirectInput buffer on this host. Device polling remains to be validated
with a foreground held-key capture. Race trace rows now include
`directinput_keyboard_state_read` and `directinput_up_pressed` diagnostics for
that capture. Evidence: `0x412F90`
`ev_72d88c205fb8150fa5e632f87e02b401b3395f2745cde2cf841b2eba351de1c3`,
`0x4130D0` `ev_1ef0d1ce36204eea7927dd0bde9b4e425f3d473b4ff1cd9852f6bb807c0284d6`,
and `0x413170` `ev_b9306522b3be005782bba4f9c3937d6c50a318d693aedc521d77a60d3836b011`.

`sample_race_loop_bindings` evaluates the 15 records used by `0x43B290` and
returns value, active, and ramped-path samples in this slot order:

`0x509338`, `0x5093C0`, `0x509328`, `0x509368`, `0x5093B0`, `0x509408`,
`0x509430`, `0x5093D0`, `0x509398`, `0x5093E8`, `0x509400`, `0x509378`,
`0x5093E0`, `0x5093D8`, `0x509418`.

## Runtime control update

The Windows race session now owns `RaceLoop43B290State` and calls
`update_race_loop_43b290` once per simulation frame. It supplies the live
keyboard snapshot for the four configured actions currently supported by the
runtime (left, right, throttle, and brake). Trace playback uses separate
synthetic selectors for those four recorded logical actions, so duplicate
physical key assignments do not alter a recorded input schedule.

Sensitivity, range, and rate values are read from the parsed `CONTROL` section
of the external `options.def`. Missing values use the same 1.0 initialization
observed in `0x43AF60`/`0x43B030`; `game_pad` defaults false. These values feed
the model fields corresponding to `0x509428`, `0x509390`, `0x5093A8`,
`0x50934C`, `0x509370`, `0x509350`, `0x509330`, `0x509380`, and `0x509348`.
The model's outputs at `0x509358`, `0x5093F4`, and `0x509320` are now retained
in session state and appended to race traces. The existing keyboard smoke
reaches throttle through VK-to-DIK mapping, the binding evaluator, and the
reconstructed throttle ramp; after focus loss the output decays to zero.

The additional `0x43B290` binding outputs are also retained in the live
session trace: direct value `0x5093B8`, flag `0x5093F0`, ramped value
`0x5093C8`, flags `0x5093CC`, `0x5093F8`, `0x509360`, difference `0x509344`,
flag `0x509420`, shift difference `0x509340`, edge latch `0x509354`, and
counter `0x5093BC`. The configured e-brake, reverse, clutch, and airlift keys
are smoke-tested against the binding slots assigned in `load_keyboard_bindings`.
These values come from the existing fifteen-binding evaluator and the native
callback's direct copies, active predicates, ramp, differences, and press-edge
logic. Trace visibility does not establish their downstream physical, camera,
or transmission effects.

`race_loop_control_actions_regression` checks these observed projections,
`0x5093C8` rise/decay and clamp, and one shift per press edge with re-arming
after release. The end-to-end trace regression now holds steering through the
native ramp long enough to cross the route adapter's `0.2` activity threshold.

The `CONTROL nonlinear_steering` option maps to global `0x5093FC` through the
registration at `0x43B030` (key pointer `0x4D5A40`). The native helper
`0x43BA30` clears that option when `game_pad` is true or either steering
binding kind is one of 1, 3, 4, or 6, as checked by `0x446F90` (the binding
checks occur only when `game_pad` is false). The live
callback now applies this initialization rule and passes the resulting gate to
the reconstructed `0x43B8C0` scale; its state is included in race traces.

This connects the recovered input update and its sensitivity controls to the
live session. The route-motion adapter still thresholds steering, throttle,
and braking outputs at 0.2 because it accepts digital controls; its pose
remains non-physical. The separate Reverse binding now requests negative
route speed, while braking reduces speed magnitude and cannot select reverse.
This corrects the adapter's control meanings; it does not reproduce native
gearbox or tire behavior. All 13 named `CONTROL` actions are mapped into their
corresponding slots among the native 15 binding records. The two unnamed slots
remain empty, and native DirectInput controller polling remains absent.
Evidence: `race_loop_update_43b290.evidence.json` (`0x43B290`,
`ev_db358c27101404ca5eb5166ee5eab7df3a38f2a0eca453f836021a79b14d5b46`),
`0x43B030` (`ev_9f472cb0b20c3269ba355274742e77b7b98756a1039d5d2a46f604a7fd8fa491`),
and `0x43AF60` (`ev_2614052db9f481c424405ce0829f4cdff2fa304f248eb04a85baeb8e480f310d`).

The input snapshot must represent values after the caller's input-refresh
boundary. This keeps platform polling outside the model while allowing the
recovered frame logic to consume the same sampled values for both raw and
thresholded predicates.

`refresh_race_mouse_input` models the deterministic portion of the
unconditional `0x444E10` call. Given the three values returned by `0x414680` and
the two raw extent globals, it normalizes X and Y about each integer half-span,
then stores the low three button-mask bits as bytes. The four-wheel race-loop
composition accepts this observation optionally, applies the refreshed mouse
values to the snapshot selected after cadence dispatch, and exposes the exact
snapshot consumed by the 15 binding records. `refresh_race_controller_input`
models the successful `0x416D30` projection: six centered/clamped axes, a POV
direction value, and 32 button-state bits. The `0x43B290` composition applies
that projection only on periodic ticks, after modeling the caller's four-axis
clear; a failed state read leaves the axis clear in place and preserves the
other snapshot fields. Device polling, acquisition, and DirectInput return
codes remain observations. Evidence:
`0x444E10` `ev_ece75ec5d6e78dc380cf45339872051c3ee9ef50c5aeba8c867e11f87e301ac8`
and accessor `0x414680`
`ev_5104ab81549d9f78e96475680126d5c474922acc684dded3b9bf0ebf94c5339e`;
`0x416D30` `ev_34ae3b40fde8b1e5271f7e9edee7dfe36ef8d2d0c6d5092f1f3ed0230637279d`.

Evidence is retained in `race_input_binding_accessors.evidence.json`:

- `0x446980`: `ev_8c6ecda839720b9a221229ff03045766d6b1084feef48b1744e33080303a819b`
- `0x446F40`: `ev_0ed0bfc556a0fc6f8594932dd09778e90d0262b887bf6203790e73bbc27d8bab`
- `0x446F90`: `ev_f0a17056a3cafb60b74360d323edbe598391e68745449fbaf8b3d81deb614914`
- `0x444E10`: `ev_b76748f1a984f67ef71987823b1b9346f3967d669dd1cc63a12a866644fdac84`
- `0x4170A0`: `ev_f8f3f0f0f023db57c1e59826a132d9acff7963d7ccbdc9dcedb0442aea80edf9`
