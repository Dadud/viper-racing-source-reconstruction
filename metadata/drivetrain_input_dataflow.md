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

The final selection written by `0x43FAE0` comes from the current previous
index at controller `+0x1C`, which can differ from the requested index at
`+0x24` while the shift state machine runs. The recovered model retains the
seven indexed values separately and keeps neutral disabled until that previous
index changes. Indices beyond the available seven-entry table are rejected.
`advance_ncar_integrator_child_control_in_body` now reads the initialized
selection, the referenced entries and the linked curve and wheel getters,
then writes the selector, curve control and signal ramp back to the body.
It leaves the body untouched if a required field is unavailable. Its caller
still owns the surrounding `0x431150` gate and signal update ordering.

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
These are signed raw-DWORD comparisons (`JL`/`JG`), not floating-point
comparisons; interpreting the boundaries as float values is only a convenience.
Negative raw values therefore select D24. The portable selector now preserves
this signed ordering.

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

**2026-11-06 update:** The player step now reads the initialized wheel `+0x128`
values, evaluates the selected D-block getter, and stores its product with
constructor `+0xDD0` to body `+0xDDC` before wheel updates. The combined
drivetrain update now also executes the D-block setters; its effect on powered
driving has not been verified. The
finite-input `0x43FFC0` projection now models its enabled branch: it stores
the scaled selected-pair getter at controller `+0x28/+0x10`, clamps the
negative damping product to `[-400, 400]`, and multiplies the sum of that
residual and the signal argument by controller `+0x18` for the pair setter.
The disabled branch requests a zero pair setter and then calls `0x43F980`.
That routine advances its `+0x10` value from the signal argument, damping
`+0x14`, inverse scale `+0x0C`, and native `0.016` step. It then multiplies
the change by `0x4C861C`, controller `+0x08`, and `0x4C8650`, scales the body
axis at `+0x50..+0x58`, and passes the resulting vector to `0x43E7F0`, which
adds it to body moment `+0x258..+0x260`. The bounded
`project_ncar_disabled_controller_integrator` models the resulting value and
moment argument, leaving accumulation and callback order to the caller.
Constructor `0x434663..0x43466F` supplies controller `+0x08` as the float
with bits `0x3E587910`; `0x43F910` stores its reciprocal at `+0x0C` and the
profile-derived argument at `+0x14`. The runtime already retains these as
body `+0xCA0/+0xCA4/+0xCAC`, respectively. The live player step calls the
combined modeled controller and integrator sequence described below.

`advance_ncar_controller_setter_in_body_and_wheels` now connects this
controller projection to the selected pair and persistent wheel state. It
uses the current wheel getters, writes only the selected wheel endpoints,
and applies the disabled branch's body-moment update. Body and wheels are
committed together after validation. The combined drivetrain update supplies
the signal and invokes this operation in the recovered tick sequence.

`advance_ncar_signal_pair_in_body_and_wheels` now joins the signal smoother,
curve setter and controller setter in the `0x43F830` order. It samples both
getters before either setter and commits the whole body/wheel update only on
success. The surrounding selector and ramp gates are updated by the combined
drivetrain operation in the live tick. The live tick's curve
getter sample was moved after wheel updates to match `0x4313F2`.

The signal pair's other child is the curve object at parent `+0xBFC`: its
vtable `0x4C8780` uses `0x43F970` as getter and `0x4402E0` as setter. The
setter smooths curve `+0x38` from `+0x2C`, computes a scale from `+0x30`,
`+0x2C`, cutoff `+0x3C`, and the getter, evaluates the raw `0x4404D0` curve,
and adds its setter argument before calling `0x43F980`. The bounded
`project_update_curve_setter_prefix` models those steps. Its output is the
input to the separately modeled `0x43F980` integrator. The bounded
`project_update_curve_setter_tail` models the subsequent accumulator update,
mode flags and linked `0x440DC0` transfer. `project_ncar_curve_setter`
composes the prefix, base integrator, and tail in native order, retaining the
getter sampled before the integrator for the later calculations. It returns
the body-moment delta and state updates for a caller to apply. The parent constructor now retains
the numeric defaults used by that child; native pointer links are represented
by the model rather than fabricated in the byte-backed object. Host
floating-point behavior is not established as x87-bit-exact.

The noninteractive wheel regression follows a nonzero selected-pair `+0xE4`
output through the spin preprojection and exit-rate stores, contrasting it
with the same synthetic wheel state with `+0xE4` cleared. Both resulting rate
values differ. This confirms that reconstructed data path for that fixture;
it does not prove correct acceleration or numerical parity with the original.

The original reaches the `0x443E6C` rate-finalization block even when wheel
`+0x168` is zero (`0x443CBC..0x443CC3`). At `0x443666..0x443691` it clears
the instantaneous tire components and filter history, then continues through
the trigonometric response and response-vector calculation. The player path
now models that reset and continues to rate finalization on no-contact ticks
when the required projection and tire inputs are available. It does not assume
the resulting response vector is zero. Cases where those inputs are unavailable
and parity with original no-contact output remain unverified.

`advance_ncar_curve_setter_in_body` bridges that operation to the initialized
byte-backed body: it reads the curve, body axis and existing moment, then
commits the getter, curve accumulators, flags and updated body moment together.
Missing or non-finite inputs leave the body untouched. The caller must supply
the native setter argument and execute it at the correct point in the tick;
the combined drivetrain update now supplies both.

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
helper now runs in the fixed-step wheel-pair update, but the semantic units and
behavioral correspondence of its getter values remain unverified.

## PCAR constructor and tick boundary follow-up (2026-11-04)

**Correction, 2026-11-06:** PCAR `+0x5B0` and `+0x8F8` alias wheel 0
and wheel 2 `+0x5C`, respectively: wheel bases are `+0x554 + 0x1A4*i`.
The earlier wheel-constructor exclusion below is wrong. `0x00441AE4`
initializes every wheel `+0x5C` to `1.0`, and wheel setup `0x00441C10`
subsequently writes it from its first input structure. For wheel indices 0/1,
the setup stores `input[+0x138] * 0.0127 + (input[+0x130] * 0.001) *
(input[+0x134] * 0.01)`; for indices 2/3 it uses offsets `+0x144`, `+0x13C`,
and `+0x140` respectively. Constants were read from the supplied `race.bin`
at `0x4C8870`, `0x4C8874`, and `0x4C885C`. `0x00433C00` retains its first
argument in EBP at `0x00433C12`, passes that same pointer as the first setup
argument at `0x004344BB`, and calls `0x00441C10` for all four wheels before
reading these fields at `0x0043470C..0x00434718`. This is the runtime-record
pointer already used for the constructor's `+0x90` input. The runtime now
uses the shared wheel-scalar projection for wheel core initialization, copies
wheel 0/2 values into the body-relative aliases, and initializes `+0xDC4` and
`+0xDD0` from the runtime record and native `0x4C7D48` scale. Its arithmetic
is host floating point, not proven x87-bit-exact; later mutations and the
remaining controller inputs still need investigation.

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
That initializer clears known wheel fields, including wheel `+0x124`. The
later correction above identifies `+0x5C` as the alias of the two body fields,
including its initialization and subsequent setup writes.

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

An exact-write probe, `tools/pcar_source_field_watch.js`, arms x86 hardware
write watchpoints for both fields at PCAR constructor `0x4381E0`, using its
`ECX` body pointer. The constructor-thread watchpoints remain armed after
constructor return and through setup callbacks until selected-integrator
sampling or capture cleanup, covering writes between construction and the
first tick. The probe's bounded observation is 120 seconds; when selected with
`--startup-only --include-pcar-source-watch`, the capture runner now extends a
shorter requested startup window to 122 seconds so it can receive the probe's
completion event and tear down cleanly. It records every callback body with PCAR vtable `0x4C8188` and its
`+0x510` ordinal; the first body is only provisional. The completion event
marks the capture valid only if exactly one callback body was observed, it
matches a constructor watch retained after return, a selected integrator sample
exists, and no watch errors or unclassified events occurred. A watch event
counts as a write when supplied Frida access details identify one watched
address with a write operation, or when exactly one watched field changes and
no access details are supplied. When both are available they must agree, and
the post-write instruction must resolve inside `race.bin`. A reported hardware
write remains valid when it stores the field's existing value, because that
writer is relevant provenance. A same-value single-step without access details
remains unclassified. The script does not launch the game or modify input.
Syntax and mock coverage pass, but a live original capture has not validated
Frida's native watchpoint setup or exception behavior. The deterministic mock
covers changed-value and same-value writes, unreported same-value events,
unrelated single-step events, wrong memory addresses/operations, multiple PCAR
candidates, missing constructor identity, re-arm failure, selected-object tick
sampling, and cleanup; it still cannot establish native Frida behavior.

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
run `0x43F830`; call the shared wheel-scalar callback using `+0xDE4/+0xDE8`;
copy the resulting per-wheel scalars; then call `0x442530` four times.
The session-owned tick now executes the modeled selector and signal sequence
before the wheel updates. It has not been checked against a live original or
verified as a complete simulation of `0x431150`. The
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
capture in this audit proves the selected brake key byte. The controlled probe
samples the type DWORD and selector byte at `0x509328` and `0x509368` into each
PCAR event. Its current controlled schedule derives each key dynamically from
that record and patches only type-1 keyboard bindings; other types are left
untouched and invalidate the capture. It samples neutral, throttle, neutral,
brake, and neutral phases and requires both `0x5093F4` and `0x509320` to change
along with adequate per-phase PCAR and wheel coverage. This is a stricter
script-level gate, not original-client evidence: the probe has not been run
against the game in this session, and the Node mock cannot validate native
record bytes or input response. Synthetic intervention remains distinct from
physical keyboard delivery.

Static inspection resolves the runtime record path: `0x43B030` calls
`0x44F00` with the brake setting name at `0x4D5A94`, then `0x447000` matches
that descriptor against a registered device entry and `0x447060` copies its
type and value at `+0x0C` into the runtime record. The evaluator at `0x46F60`
dispatches on type; type 1 reads the configured selector byte through
`0x413190`, while other types take separate device paths. Therefore the
probe's `type` and `selector` fields describe the resolved record, but the
selector is a keyboard index only when the observed type is 1. The updated
probe enforces that condition independently for throttle and brake. Its Node
mock checks script wiring and fail-closed behavior only. No original-client
brake capture yet proves the live type, selector, or matching `0x509320`
response; reject captures unless those native responses and phase coverage are
actually observed. Synthetic input remains distinct from physical keyboard
delivery.

The analog evaluator confirms the hook target: `0x446F40` calls `0x446980`;
for type 1, `0x446980` reads `[record+4]` and calls `0x413170`, then returns
the configured pressed/released scalar. Thus the type-gated probe intercepts
the same keyboard wrapper used by the throttle/brake ramp, while its matching
record selector prevents patching unrelated key reads.

The capture completion event reports per-phase throttle/brake means and compares
`throttle` against the mean of all three neutral phases and `brake` against the
same neutral baseline. A whole-run range change alone cannot make the capture
valid.

## Curve scoring is not wheel torque

The `configure_acar_update_curve` helper builds the curve subobject at parent
`+0xBFC` from runtime-record floats, including the terms named `torque_scale`
and `horsepower_term` in the portable arithmetic. In the checked native
consumer, `0x43FDD0` passes each controller candidate (`entry * x`) to
`0x4404A0`, then uses the returned score to choose or preserve selector
`+0x24` through `0x43FAD0`. `0x43FAE0` updates selector state. The
`0x431150` path later reads the selected-pair getter, scales
it into `+0xDDC`, and dispatches wheel updates. The reviewed curve-score path
has no store or call that applies its score as wheel force, wheel rate, or body
force. Therefore the coefficient names and values are not evidence for a
throttle torque law. Keep this model confined to the source-observed selector
until a separate force-producing consumer is identified.

This is a bounded conclusion for the reviewed call chain, not a claim that no
other engine/transmission producer exists. The `+0x5B0/+0x8F8` constructor
source watch and state correlation around the selected D block and live wheel
`+0x124` writer remain the next decisive investigation.
