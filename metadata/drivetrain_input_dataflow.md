# Bounded PCAR control-to-wheel dataflow audit

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
The checked-in `disasm/engine_.text.asm` is the instruction source for the
addresses below. This note follows the PCAR control path and distinguishes it
from the live wheel-rate producer; it does not claim the entire executable has
no engine or transmission subsystem.

## Control ramp and PCAR callback

`0x43B290` is reached from the race update at `0x425970` and also from two UI
paths at `0x47C85C` and `0x47E85E`. Its throttle-ramp path reads configured
binding record `0x509328`, updates global throttle value `0x5093F4`, and clamps
the result at `0`/`1` (`0x43B4B5..0x43B5A8`). The brake path mirrors this using
binding `0x509368`, output `0x509320`, and its own sensitivity/rate globals
(`0x43B5A8..0x43B6A4`). The clutch-like output at `0x5093B8` is read from
binding record `0x5093B0` at `0x43B6A4..0x43B6AE`. Accessors `0x43B930`,
`0x43B940`, and `0x43B990` return those globals without additional state
changes.

PCAR callback `0x4384D0` consumes the outputs at `0x43850F..0x43854D` and
passes them through setters to PCAR `+0xDE0`, `+0xDE4`, and `+0xDE8` (with the
throttle multiplier on `+0xDE0`). At `0x43872F` it calls the shared integrator
`0x431150`. That integrator copies `+0xDE0` to the embedded object at `+0xC98`
(`0x431295..0x4312A4`); constructor/setup evidence identifies this child as a
selection/animation controller, not an engine or transmission object. The
integrator passes `+0xDE4/+0xDE8` through virtual slot `+0x4C` at
`0x431317..0x43135D`; the selected entries at vtables `0x4C7F6C` and
`0x4C858C` resolve to `0x432170`, which writes four wheel `+0xE0` values.
Those values feed steering/low-speed wheel response. The reviewed path does
not carry a gear, engine RPM, clutch torque, or differential torque operand.

A literal-address search across the checked-in instruction listing finds
references to `0x5093F4`, `0x509320`, and `0x5093B8` only in global
initialization at `0x43AFBE..0x43AFD0`, their ramp/copy sequence in
`0x43B290`, and the three accessors above. This rules out additional direct
absolute references to these globals in this listing, but does not exclude
computed aliases, copied control state, or a separate input-to-powertrain path.

## Embedded selector and wheel-pair output

The `0x431150` integrator copies PCAR `+0xDE0` and `+0xDEC` into controller
`+0xC98` fields `+0x64` and `+0x5C`, then calls `0x43FAE0` with PCAR byte
`+0x534`. For PCAR construction, `0x4381E0` loads the `AutoClutch` option into
`+0xEC9`, applies the auto-shifting override, and copies the result into
`+0x534` at `0x438299..0x4382A4`. This establishes that the option gates the
controller; it does not establish clutch or gearbox actuation.

Constructor `0x4346A9..0x4347A4` configures three paired state blocks at
`+0xD04/+0xD24/+0xD44`. `0x43F6F0` stores its arguments at block `+0x14`,
`+0x1C`, and `+0x18`; `0x43F710` stores endpoint pointers at `+4/+8`.
`D24` uses runtime-record `+0x90` as `+0x1C` and
`runtime[+0xB8] * 0x4C7EF4` as `+0x18`, with wheel endpoints `+0x554/+0x6F8`.
`D44` uses the same `+0x90`, `runtime[+0xBC] * 0x4C7EF4`, and endpoints
`+0x89C/+0xA40`. `D04` uses `+0x1C = 1.0`,
`runtime[+0xC0] * 0x4C7EF4`, and endpoints `D24` then `D44`.

The constructor's raw runtime-record `+0xB4` DWORD selects the controller's
target `+0x4C`: bits below `0x3C23D70A` select `D24` (index 0), bits through
`0x3F7D70A4` select `D04` (index 1), and larger bits select `D44` (index 2).
These are unsigned raw-bit comparisons; interpreting the boundaries as float
values is only a convenience, not the comparison semantics.

The `0x4C8768` vtable maps slots 0/+4 to `0x43F730/0x43F760`. For each pair,
slot 0 returns `(endpoint1_getter + endpoint2_getter) * block[+0x1C] * 0.5`.
Slot +4 first reads both endpoints, computes `time = block[+0x1C] * argument
* 0.5` and `difference = (endpoint2 - endpoint1) * block[+0x18]`, then calls
endpoint1's setter with `difference + time` and endpoint2's setter with
`time - difference`. `D04` therefore applies one pair update recursively to
`D24` and `D44`.

This target is reached through controller vtable `0x4C8778`, whose slots 0/+4
are `0x43FF90/0x43FFC0`. The PCAR constructor selects the D-block pointer and
stores it in controller `+0x4C`. Constructor `0x4347AF..0x4347C5` links the
`+0xC7C` signal block to the curve at `+0xBFC` and controller at `+0xC98`.
`0x431150` calls `0x43F830` on `+0xC7C`; that helper calls the signal pair's
two virtual +4 setters with opposite values. When the second child is the
controller, `0x43FFC0` updates the selected D block, which dispatches to the
wheel endpoints. The selector's `+0x4C` pointer is initialized by the PCAR
constructor: `0x4347A4` calls `0x43FA70` on selector `+0xC98`, storing D24
(`+0xD24`) for mode 0, D44 (`+0xD44`) for mode 2, or D04 (`+0xD04`) otherwise.
The selector constructor clears this field at `0x43FA4C`. This resolves the
producer for PCAR `+0xCE4` (`+0xC98 + 0x4C`). Its getter at `0x43FAE0` dispatches
slot 0 on the selected D block, whose `0x43F730` getter combines endpoint
getters; exact runtime endpoint values and their semantic units remain open.
The listing's only literal `+0xCE4` displacement use is the read at `0x4312C5`.
It invokes slot 0, multiplies the result by PCAR `+0xDD0`, and stores `+0xDDC`
at `0x4312D5`. This is bounded direct-displacement coverage only; computed
aliases, copied state, and indirect writes are not excluded.

Wheel vtable `0x4C89C8` slots 0/+4 are `0x444600/0x4445F0`:
the getter reads wheel `+0x128`, and the setter writes wheel `+0xE4`. The active
`0x442530` wheel updater consumes `+0xE4` in its spin response.

`advance_ncar_selected_wheel_pair_target` now models the constructor-selected
D-block hierarchy and `0x43F760` arithmetic; its byte-backed adapter writes
only the wheel `+0xE4` fields reached by the selected block. The Windows
fixed-step does not call this adapter yet. Its controller setter argument
remains explicit because the active runtime's associated controller/curve
setter path is not fully integrated.
This establishes a source-backed path from that argument to the wheel
spin-response input; it does not yet identify engine torque, prove that
throttle supplies the argument, or establish original numeric parity.

After the `+0xC98` animation-controller update, `0x431150` calls the virtual getter at PCAR
`+0xCE4`, multiplies its result by `+0xDD0`, and stores `+0xDDC`. The selected
D-block getter identity is resolved, while its runtime result is still bounded
by endpoint values and unassigned units. Direct `+0xDDC` xrefs also include a distinct
velocity-magnitude writer at `0x4324B8`, snapshot copying at `0x432AF4`, and
per-step record copying at `0x438770`; the checked listing shows no direct
`+0xDDC` use in wheel-force calculations. Indirect consumers of copied records
are outside this bounded conclusion. This makes the selector path a concrete
next investigation target, not a source-backed torque formula.

The `0x43F830` signal block's constructor scales are now projected from the
PCAR runtime record: `0x434674..0x43468A` passes `runtime[+0x68] * 4.0` to
`0x43F7F0`, which stores that at parent `+0xC8C` and stores its product with
`0.0025` at parent `+0xC90` (equivalently `runtime[+0x68] * 0.01`). The signal
state at parent `+0xC7C` starts at zero; its accumulator at `+0xC94` also starts
at zero. The curve child at `+0xC80` is parent `+0xBFC`, and the controller
child at `+0xC84` is parent `+0xC98`. The helper's differential, clamp,
smoothing, and opposite setter arguments are modeled, and the constructor now
writes the two record-derived scales into the session-owned body bytes. The
controller getter's state inputs and both getter values' semantic units remain
unresolved, so the helper output is not yet wired into the fixed-step wheel
pair update.

## PCAR constructor and tick boundary follow-up (2026-11-04)

The full checked-in `engine_.text.asm` listing was searched for direct body
displacements `+0x5B0`, `+0x8F8`, and `+0xDD0`, and for the `0x431150` call
sequence. Constructor `0x434700..0x434756` forms the `+0xDD0` multiplier from
the selector DWORD copied from runtime-record `+0xB4` into body `+0xDC4`:

```
dd0 = (((1.0 - body[0xDC4]) * body[0x5B0]
        + body[0x8F8] * body[0xDC4]) / runtime_record[0x90])
      * 0x4C7D48
```

The two source body offsets `+0x5B0` and `+0x8F8` have no other direct
displacement references in the checked listing; `+0xDD0` is read at
`0x4312CF` after controller update and written by the constructor above.
This is bounded exact-address/displacement evidence. It does not rule out
register-based aliases, block copies, or writes through a subobject/base
pointer, so the source fields are unresolved rather than zero or constructor
constants. Do not project the `+0xDDC` product until their provenance is found.

The constructor ranges were checked for those copy/alias possibilities. The
PCAR allocator requests `0xEF0` bytes and then calls `0x4381E0 -> 0x43DFB0 ->
0x42FF90 -> 0x43E490 -> 0x4375C0`; the base initializer writes its vtable and
explicit fields, not a whole-object clear. `0x42FF90` calls `0x433C00`, whose
observed bulk copies cover `+0x514` from a string, `+0xE6C` and `+0x38` from
two 12-DWORD ranges, plus explicit nearby fields. None of those spans includes
`+0x5B0` or `+0x8F8`. The reviewed constructor chain therefore does not source
these values. Other callees, alternate base aliases and later mutation remain
outside this conclusion; their actual values and origins are still unknown.

A follow-up alias audit confirmed that `0x4381E0`, `0x43DFB0`, and `0x42FF90`
forward the same PCAR base. `0x43E490` calls `0x4375C0` on subobject `+0x6C`
and initializes another group at `+0x2B0`; its inspected direct stores remain
within those base/subobject ranges. The four wheel constructor calls beginning
at `0x42FFC7` initialize children at `+0x554 + 0x1A4*i` with `0x4419A0`.
That initializer clears known wheel fields, including wheel `+0x124`, but is
not a PCAR-body copy and does not cover `+0x5B0/+0x8F8`. This strengthens the
bounded constructor exclusion; indirect callbacks and writes through computed
aliases are still not ruled out.

The next decisive observation is a write watch on the newly allocated PCAR
body's `+0x5B0` and `+0x8F8`, armed before constructor `0x434700` and kept
through the first `0x431150` update. At each write, record the instruction,
effective destination, value, and call stack. In the same run, capture PCAR
`+0xDC4/+0xDD0/+0xDDC`, `+0xDE0/+0xDE4/+0xDE8/+0xDEC`, the `+0xC98` child
pointer and vtable, selected D-block and endpoint values, and four wheel
`+0xE4/+0x124/+0x128` states around `0x442530` and `0x443E9D`. Capture any
candidate gear/RPM child only after its pointer and vtable identity are
observed. This distinguishes untouched allocator contents from a constructor
or first-tick source without assigning semantics from offsets. Do not use
route motion to generate or interpret the input-to-motion trace.

The existing controlled probe `tools/original_pcar_controlled_wheel_capture.js`
now records the unresolved fields and the related selector, signal, controller,
pair endpoints, and wheel spin fields before/after sampled integrator and wheel
updates. It also retains its synthetic DIK_UP observation window and reports
whether the control intervention actually reached the game. This is a useful
state-correlation capture, not an exact write watch: it cannot identify which
instruction wrote a field between samples. Probe syntax was checked with
`node --check`; a new controlled original run has not yet validated the added
memory reads.

An exact-write probe, `tools/pcar_source_field_watch.js`, now arms x86 hardware
write watchpoints for both fields at PCAR constructor `0x4381E0`, using its
`ECX` body pointer, and retains them through that constructor's return. It then
selects the first callback object with PCAR vtable `0x4C8188`, verifies body
identity at `0x431150`, and arms the integrator thread if it differs from the
constructor thread. Each hit records the post-write instruction address,
field values, changed fields, and fuzzy backtrace. The script does not launch
the game or modify input. Its syntax passes `node --check`, but a live original
capture has not validated watchpoint setup, exception handling, or selection of
the intended player object. Results must be rejected unless the body matches
the selected callback object and the writer address resolves inside `race.bin`.
The deterministic mock in `tests/pcar_source_field_watch_mock.js` exercises
constructor arming, changed-field events, re-arming, selected-object tick
sampling, and cleanup. CMake registers it when Node is available; the mock
validates the agent's control flow only, not Frida's native watchpoint support.

The arithmetic is now available as
`derive_ncar_integrator_signal_multiplier`, with the two body fields, divisor,
selector and `0x4C7D48` scale all explicit. Regression covers both interpolation
endpoints, an interior selector, zero divisor, and nonfinite inputs. This model
is useful once those values are sourced; it is not called by the live PCAR tick.
The Visual Studio 18 Release build and all 69 configured Release CTests pass
with this helper and regression; the route-free replay continues to pass but
does not assert throttle propulsion.

The full `0x431150` ordering is also explicit in the listing: set selector
`+0x64/+0x5C` from PCAR `+0xDE0/+0xDEC`; invoke selector update `0x43FAE0`;
read selected pair getter `+0xCE4`, multiply by `+0xDD0`, and store `+0xDDC`;
copy parent flag `+0xC3C` to `+0xE49`; conditionally raise signal `+0xC7C`;
run `0x43F830`; pass `max(+0xDE4,+0xDE8)` to the selected pair setter;
copy the two per-wheel scalar results; then call `0x442530` four times.
The selector and signal helper models now exist, but the session-owned game tick
still does not execute this full chain. In particular, unresolved constructor
inputs and callback child state prevent a faithful end-to-end invocation. The
wheel-pair setter can affect `+0xE4`, a spin-response input, but neither the
static call sequence nor the route-free comparison establishes that this is
the engine torque producer. Powered longitudinal control remains the first
race acceptance blocker.

Next bounded source target: trace writes/copies feeding body `+0x5B0` and
`+0x8F8` during all PCAR construction paths, including helper arguments and
base-pointer changes around `0x42FF90 -> 0x43E490 -> 0x433C00`. If the listing
cannot resolve them, record the exact native runtime observation needed before
integrating this call chain. Continue to keep route motion disabled in physics
acceptance runs.

## Live wheel-rate writer and decoy writer

The live PCAR wheel objects are updated by four calls from `0x431150` to
`0x442530` at `0x4313B9..0x4313C2`. In the active-contact tail of that updater,
`0x443E9D` stores wheel `+0x124` after adding its prior value to a
contact/tire-response term; it also derives `+0x128` at `0x443EA3..0x443EAF`.
This is the identified live rate producer in the reviewed path. The expression
uses local response values, wheel `+0x38`, and the current wheel state; the
static slice does not identify a throttle/engine-torque operand.

The other instruction-shaped hit, `fstp [esi+0x124]` at `0x44435B`, is inside
`0x444240`, which is called by `0x432773` while copying adjacent five-byte
samples from `0x3C`-stride ghost/interpolated records. It is not evidence of a
live PCAR wheel writer. The PCAR wheel constructor/reset writes are
`0x441B13` and `0x441B98`; `0x44286D` is a conditional damping store.

## Runtime comparison

The route-free Viper/Bemidji diagnostic starts separate runs from the same grid
placement and compares updates 32–63, when throttle or brake is actually held,
against the same updates of a neutral run. On 2026-11-04 the matched means were:

| Input | Signed forward-speed mean | Absolute forward-speed mean |
|---|---:|---:|
| Neutral | -0.499623 | 1.528561 |
| Throttle | -0.499623 | 1.528561 |
| Brake | -0.535571 | 1.492704 |

The controlled reconstructed trace therefore reports no measurable throttle
effect and a small braking response. These measurements are diagnostic only;
they do not establish original-game parity. The focused route-free regression
continues to test deterministic playback, steering response, force pipeline,
and zero ideal-line motion, and prints the longitudinal comparison without
pretending the powered-driving acceptance condition has passed.

## Next source-reconstruction target

The bounded checked-in listing audit still does not identify an engine/gear
torque producer. Continue with a constructor/first-tick write watch for PCAR
`+0x5B0/+0x8F8` and trace the object state feeding the active contact response
at `0x443E9D`. Record caller identity, child pointers/vtables, endpoint values,
and wheel state before adapting discovered behavior into the C++20 session.
No direct throttle-to-force formula should be introduced from current traces.

## Controlled key selector evidence boundary

The controlled original-client probe currently intervenes only on DIK_UP
(`0xC8`), the throttle key used by the retained historical capture. The
disassembly for `0x413170` confirms it indexes the internal keyboard snapshot
with the selector passed by its caller and returns that byte's high bit. The
brake signal is fed from configured binding record `0x509368` through
`0x446F90` in `0x43B290`; the active selector is therefore configuration
state, not safely assumed to be DIK_DOWN (`0xD0`). No checked-in original
capture in this audit proves the selected brake key byte. Keep the probe
throttle-only until the configured record layout and active selector have
been recovered or directly observed. The controlled probe now samples the
first DWORD and selector byte at `0x509328` and `0x509368` into each PCAR event;
its Node mock verifies the emitted record fields with a simulated key binding.
This does not establish that those native record bytes are a key selector for
every binding type, or that a brake intervention reaches `0x509320`. Future
brake captures must first report a supported key-binding type, then use its
recorded selector and demonstrate matching `0x509320` response before being
accepted. Synthetic intervention remains distinct from physical keyboard
delivery.
