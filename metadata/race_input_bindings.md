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
its data format; `0x4130D0` polls `0x100` bytes into `DAT_004EF200`; and
`0x413170` reads the selected byte's high bit. The runtime converts parsed
virtual-key bindings and incoming Win32 key events through `MapVirtualKeyW`
and maps an `E0`-prefixed scan code into the DirectInput extended-key range.
The arrow-key conversion is exercised end to end by
`game_keyboard_binding_smoke`, which verifies the runtime trace sees throttle
while Up is held and clears it after key-up. Native device polling itself
remains unimplemented. Evidence: `0x412F90`
`ev_f1afb4146263e4c858ccb590428f2b21bdb16b8de7093427b63a76675c137001`,
`0x4130D0` `ev_1ef0d1ce36204eea7927dd0bde9b4e425f3d473b4ff1cd9852f6bb807c0284d6`,
and `0x413170` `ev_b9306522b3be005782bba4f9c3937d6c50a318d693aedc521d77a60d3836b011`.

`sample_race_loop_bindings` evaluates the 15 records used by `0x43B290` and
returns value, active, and ramped-path samples in this slot order:

`0x509338`, `0x5093C0`, `0x509328`, `0x509368`, `0x5093B0`, `0x509408`,
`0x509430`, `0x5093D0`, `0x509398`, `0x5093E8`, `0x509400`, `0x509378`,
`0x5093E0`, `0x5093D8`, `0x509418`.

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
