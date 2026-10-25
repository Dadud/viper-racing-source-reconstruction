# Class-specific car update branches

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
The addresses are image virtual addresses. Ghidra did not identify the
`PCAR` and `NCAR` update entries as functions, so this note separates their
saved `.text` disassembly from analyzed helper pseudocode. The offsets below
remain numeric; they are not semantic field names.

## Shared integrator entry at `0x431150`

The first branch tests the object dword at `+0x4FC`. When it is zero, the
integrator calls setter `0x4331C0` with zero (object `+0xDEC`), calls setter
`0x4331B0` with float 1.0 (object `+0xDE4`), and clears object dwords `+0x234`
and `+0x23C`. `apply_ncar_integrator_entry_reset` models this exact prefix;
nonzero `+0x4FC` leaves those fields untouched. In the four-wheel body-step
composition, the same gate now clears the modeled linear-velocity X/Z
components before later projections. The following `+0xE3D` branch transforms
local point `(0,2,0)`, evaluates its point velocity, and accumulates the
vertical force `+0x7C * point_velocity.y * -4.4145`. Its nested contact-byte sum
reaches the `+0x50` virtual callback only when the sum is greater than two;
that callback's native effects remain outside the projection.

Evidence: `ev_fb7a9b21717cfb6308ed0b4ff5c8e3f5b185399be225d0fbbfbde36ee68b1217`
(`0x431150` pseudocode), `ev_4e237485e955a5ada3411b37c141b93778bcfda352b438d4c0414812405a213a`
(`0x4331C0`), and `ev_985b150bd698d190e02732dbb8ae7b75ef0248366c9a97b122fdf5308da4f4ce`
(`0x4331B0`). The nearby timer/control calls remain outside this prefix
projection. The retained analysis bundle is
[`ncar_integrator_prefix.evidence.json`](ncar_integrator_prefix.evidence.json).

The next helper call `0x433570` is now modeled by
`advance_ncar_integrator_timer`. It computes `now = timer_ticks * 0.016`,
then continues only when the x87 comparison finds `now - +0xE44 < 6.2` (an
unordered comparison also follows the native less-flag path). The stored
single-precision delta takes the quadratic ramp when its signed raw DWORD is
below `0x40C00000`; otherwise `+0x50C` is set to 1.0. The quadratic branch
uses the exact binary constants and x87-style extended intermediates. While
inside the time window it also resolves the `+0x510` ordinal through the
runtime table at `0x4D45B0`; a non-null record whose `+0x04/+0x08` squared
distance is less than `25.806398` advances `+0xE44` by `0.016`. The runtime
table/record is supplied explicitly, with unresolved lookup data marking the
composed step incomplete. No semantic names are assigned to the table fields.

Evidence: `ev_b33d1d148f26718f4435cef4a33eef66d10e0d9c58e27c577477ccb2cce4b1f8`
(`0x433570` pseudocode), `ev_158aba98f08b44d3788819fdf2c99714a496b272bb50286b61cbc3dee5d00fa2`
(`0x433570` instruction sequence), `ev_4394f76cfa93de76984db3bbd98bb3696a7ab8b4159da55951abcbc5f7aabaef`
(`0x425790` timer scale), `ev_1816f75b415168f0d79f1c0012ccfbdc04a46e96fa08745bccef31f02c6f9061`
and `ev_946ec3efe850470f7eea21d3d08dc427a46c9a686b71d929dbd583ebeab36ae6`
(`0x433570` constants). The retained records are in
[`ncar_integrator_timer.evidence.json`](ncar_integrator_timer.evidence.json).

Immediately afterward, `0x431150` calls `0x432130` on the body position at
`+0x5C`. The helper returns zero only when all three raw DWORD pairs pass:
X/Z are strictly between -5000 and 5000, and Y is strictly between -1000 and
3000. The caller invokes global interface slot `+0x3C` when the helper returns
nonzero, so `plan_ncar_integrator_early_bounds` records the callback on any
out-of-range position and forwards ordinal `+0x510`. The comparisons are
retained as the original unsigned and signed integer tests over float bits,
including their behavior for non-finite values; callback effects remain
outside the portable model.

Evidence: `ev_76372b1d20911ae6c6596f9fc9987110b1639312dde8af814ff1145e92ab0d56`
(`0x432130` pseudocode and instructions), linked to caller `0x431150` in
[`ncar_integrator_timer.evidence.json`](ncar_integrator_timer.evidence.json).

The following `0x431150` branch calls `0x4402D0` on the embedded object at
`+0xBFC` when parent byte `+0xC3D` is set and the signed raw DWORD at `+0xC7C`
is below `0x3C23D70A`. The helper writes child byte `+0x40` only if child byte
`+0x41` is nonzero; this aliases parent byte `+0xC3C`. If the resulting parent
byte `+0xC3C` is nonzero, the caller invokes setter `0x4331A0(1.0)` for
`+0xDE0`. `advance_ncar_integrator_curve_gate` models this sequence in the
four-wheel step. `advance_pcar_callback_tail` now also requires the observed
child `+0x41` byte before projecting the parent flag write.

Evidence: `ev_96c88cabb88c7c78ae26cb2ae250b2939ebe2a35e35c952ee28e62aee3be1e0a`
(`0x4402D0`) and `ev_c4b47e34375fbf4bd3cb556c06f630cca74cfbc46dce80a77aed30a155152827`
(`0x4331A0`); the caller branch is in `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`.
The helper and setter records are in
[`ncar_integrator_curve_gate.evidence.json`](ncar_integrator_curve_gate.evidence.json).

After the curve gate, `0x431150` initializes the embedded controller at
`+0xC98`: it copies car `+0xDE0` to child `+0x64`, car `+0xDEC` to child
`+0x5C`, then calls `0x43FAE0` with car byte `+0x534`. The four-wheel step now
composes `setup_ncar_integrator_child_control` and
`advance_ncar_animation_controller` at that point, feeding the post-curve-gate
`+0xDE0` value into the first child setter. Controller callback values, child
threshold fields, and setter target objects remain caller observations.

Evidence: `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`
(`0x431150` call order), `ev_1128dc7f406320fc5ef2f1ed4675611a89fdc97effbfc2c22b8811268e06d4ea`
(`0x43FF80`), `ev_498d3e18c8202f1635d22ed6fdab875a921c74ff081aee95b6a8369055328bf2`
(`0x43FF70`), and `ev_e3e3d51d09604ce173a3a18906c819e7484977c6a91f49f3edb6d87a451e26bf`
(`0x43FAE0`).

Immediately after the child controller, `0x431150` calls the virtual getter at
`+0xCE4` (array index `+0x339`), multiplies its float return by `+0xDD0`, and
stores the single-precision product at `+0xDDC`. The caller callback remains an
explicit observation. The integrator copies byte `+0xC3C` to `+0xE49`. If
byte `+0xBE4` (array index `+0x2F9`) is set and the signed raw DWORD at `+0xD70`
(array index `+0x35C`) is greater than `0x44FA0000`, it calls `0x43F8E0` to
store float 1.0 into `+0xC7C`. It then always calls `0x43F830` on the
six-field block beginning at `+0xC7C`. The helper obtains returns from the
virtual getters through `+0xC84` and `+0xC80`, forms their difference times
`+0xC90`, clamps it using `+0xC8C * +0xC7C`, and smooths `+0xC94` with binary
weights `0x3E199998` and `0x3F59999A`. It calls the paired `+0x04` setters on
the `+0xC80/+0xC84` objects with opposite values `(+0xC7C)^2 * +0xC94` and
its negation. `advance_ncar_integrator_signal` reports those setter arguments;
external callback and target-object effects are not emulated. The `+0xC7C`
input to the earlier curve gate seeds this shared block's value.

Next, the virtual callback at slot `+0x4C` receives four floats initialized to
`[+0xDE4, +0xDE4, max(+0xDE4,+0xDE8), max(+0xDE4,+0xDE8)]`. Its resulting
four-float buffer is copied to wheel subobject `+0xE0` fields starting at
parent `+0x634`, with `0x1A4` bytes between wheels. When the signed raw DWORD at
`+0xDE8` exceeds `0x3DCCCCCD`, buffer entries 2 and 3 are negated before the
stores. Both observed class vtables that contain `0x431150` at slot `+0x08`
point their slot `+0x4C` to `0x432170`. That routine returns unchanged if
`+0x4F8` is zero. Otherwise it computes
`speed(+0x234) * ((+0xD84 - +0xD90) / tan(+0xDCC * +0x500))` and proceeds only
when the resulting float is greater than the global float at `0x4C7E2C`
(`0x3F638E39`). Selector value 1 uses scale 0.5 for the positive response;
other nonzero selectors use 5.0, while the negative response uses 5.0. The
raw signed value at `+0x244` and the signed raw float bits of the delta choose
which buffer lane receives a max update: lane 3 or 0 when `+0x244 > 0`, lane 1
or 2 otherwise. The updated array is then copied to the four wheel `+0xE0`
fields. `advance_ncar_integrator_wheel_scalars` now reconstructs this callback
and applies the caller's pairwise sign gate before exposing the stores. It
feeds each wheel value into the existing spin-response stage, which consumes
its absolute value. Exact x87 tangent/division rounding and the optional
Pentium math-wrapper path remain approximate.

The callback's geometric inputs can now be initialized from the source vehicle
setup record using the constructor stores in `0x433C00`: `width = source[0x58]
* 0.0254`, `ratio = source[0x5C] * 0.01`, `+0xD84 = (1 - ratio) * width`,
`+0xD90 = -(ratio * width)`, and `+0xDCC = source[0x4B0] * 0.0174`. Set
`derive_constructor_setup` to use these formulas in the wheel callback model;
otherwise it uses the explicit object-field observations. The `+0x500` angle
scale remains an input from the interpolated object state, where the existing
`0x432420` model derives it from the sampled record. The constructor dossier's
value-flow is truncated after an omitted operation, so this records the
observed field stores without claiming a complete vehicle-constructor model.
Evidence is retained at
[`ncar_wheel_callback_setup.evidence.json`](ncar_wheel_callback_setup.evidence.json)
(`ev_a957a079d7cb019a7d9f11b607222893011d8db06a2f29fc193137619c0e6eeb`,
`0x433C00`).

Evidence: `ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5`
(`0x431150`), `ev_561bda702abc7970b5b820be76dc22cedb78968c7da2d82ede95809777fcf6c1`
(`0x43F830`), `ev_e1224f8c92d66a13ee1793d2d6ed81095204c66493ad2a6b1423e4e65cc60352`
(`0x43F8E0`), and `ev_c3a729ee52e88cb9fda90eb175a7221995a84e0f6df71064034ccfebfdf3c791`
(`0x4C8640` constant bytes). The five-record bundle is retained at
[`ncar_integrator_signal.evidence.json`](ncar_integrator_signal.evidence.json).
The adjacent `+0x4C` callback and four strided wheel stores are in the caller's
same assembly evidence. `ev_6759c726d8701068a83e1faac987ee5a6eead35b89062328d7cc81b4c2174ad7`
(`0x442530` pseudocode, including the `ABS(+0xE0)` consumption) is retained at
[`wheel_update_value_e0.evidence.json`](wheel_update_value_e0.evidence.json).
The routine bytes, vtable references/entries, tangent wrapper, and threshold
constants are retained at
[`ncar_integrator_wheel_callback.evidence.json`](ncar_integrator_wheel_callback.evidence.json).

## PCAR callback at `0x4384D0`

The prologue combines object fields `+0x234`, `+0x238`, and `+0x23C` with
`+0x50`, `+0x54`, and `+0x58`, takes the absolute value, and passes it to
`0x43B8C0`. It calls `0x4330F0`, then uses returned/scaled values with setters
`0x4331A0`, `0x4331B0`, and `0x4331D0`. Further helper/setter calls include
`0x4331E0`, `0x433210`, `0x4331C0`, and `0x433250`.

`advance_pcar_callback_control_prefix` now composes this exact opening
dataflow: it projects the body's `+0x234..+0x23C` velocity onto `+0x50..+0x58`,
takes the absolute value, applies `0x43B8C0`, updates the `0x4330F0` smoother,
then writes the initial `+0xDE0/+0xDE4/+0xDE8/+0xDEC` values. Body velocity,
basis, callback globals, smoother state, and the `0x40D270` result remain
explicit inputs. The live route session does not yet own body velocity/basis;
it therefore does not substitute route speed or heading into this callback.

The first state split is `0x43B9E0`:

- If true, it calls `0x4331E0(-1)` and sets `+0x54` to the greater of the
  `0x43B930` result and the constant at `0x4C814C`.
- If false and byte `+0xEC8` is nonzero, it calls `0x433210`.
- Otherwise it calls `0x43B9A0`; if byte `+0xEC9` is nonzero or dword
  `+0xC7C` is below `0x3F666666`, the returned value goes to `0x4331E0`.
  It then passes dword `+0xD64` to `0x43B9B0`.

Both paths join at `0x43B950`/`0x4331C0`. If `0x43B9F0` returns true,
`0x433250` runs. The method then sets bits in dword `+4`: bit 0 is set when
`+0xDE4 > 0x3DCCCCCD`, and bit 1 when `+0xC34 > 0x3ECCCCCD`.

`advance_pcar_callback_branch` now projects this split and its direct stores.
The constant at `0x4C814C` is verified from the executable bytes as
`9A 99 99 3E` (`0x3E99999A`, 0.3f), used for the `max` written to `+0x54`.
The branch model now performs the `0x4331E0` selector setter and `+0xCBC` to
`+0xD64` copy directly, sourcing its conditional argument from global
`0x5093BC` returned by `0x43B9A0`. The `0x433210` path invokes the numeric
`update_embedded_selection` model and copies its resulting `+0xCBC` to
`+0xD64`; selector and curve snapshots are required for that path. The
false-gate, clear-`+0xEC8` arm then reports the post-helper `+0xD64` value
passed to `0x43B9B0`. Regression covers gate precedence, the direct wrapper
copies, `+0xEC8`, `+0xEC9`, the strict signed `+0xC7C` threshold, the
`+0x54` floor, missing-input atomicity, and status bits. This remains a
standalone model; runtime callback dispatch and live selector/curve snapshots
are not yet connected.
The floor constant bytes were read from the shipped `race.bin` at `0x4C814C`
(`ev_c1a1c96bc554d4b7efe42229fd89168076d2116e4f6cb2b020456143cbca6054`).

`0x43B9D0` returns the global byte at `0x509360` and gates a timestamp
operation on `+0xECC`. On its true path, a zero value triggers a backend
virtual `+0x3C` call with the car ordinal at `+0x510`, then stores current time
from `0x425790`. A nonzero value is compared against the current time using
the 0.1-second constant at `0x4C8154`; the callback stores sentinel bits
`0x4CEB79A3` (float 123456800) when the x87 comparison reports neither less
nor equal (greater or unordered). The false path
clears `+0xECC`. The portable `update_pcar_timestamp` reports whether the
backend callback is requested and models the timestamp store; the backend
virtual itself is outside the model. Both paths continue through
`0x434A80` and `0x43B880`, then cache values from `0x43BA00` and `0x43BA20` at
`+0x4F0` and `+0x4EC`.

When byte `+0xC3D` is set and the signed dword at `+0xC7C` is below
`0x3C23D70A`, `0x4402D0` is called with `this+0xBFC`; it sets subobject byte
`+0x40` to 1 when subobject byte `+0x41` is nonzero, which writes parent byte
`+0xC3C`. If dword `+0x4FC` equals 3, the method calls `0x4331A0(0)` and
`0x4331B0` with the greater of `+0xDE4` and the float at `0x4C8164` (raw bits
`0x3E4CCCCD`, 0.2). If byte `+0xBE4` is nonzero, it passes the `0x43BA10`
result to `0x434B20`, which stores it at `+0x508`. The portable
`advance_pcar_callback_tail` applies those offset writes and reports which
helper branches ran. Its inputs now include the embedded curve object's raw
`+0x41` byte so the `+0xC3C` store only occurs on the helper's observed gate.

Evidence: `0x4402D0` `ev_96c88cabb88c7c78ae26cb2ae250b2939ebe2a35e35c952ee28e62aee3be1e0a`,
`0x434B20` `ev_22e4b24214928c99dced78db39dbbb2176546ab0ebc869e98bf2ffaa8b44d948`,
and constant bytes at `0x4C8164` `ev_1d3fa4c3977b2511de323c76a97b48fc43e74e3db3e7f6e51c73a8c23c5df582`.

All observed control-flow paths join before the call at `0x43872F` to
`0x431150`. After that integration call, the method writes a per-step record
using `0x422890` and backend virtual slots `+0x50` and `+0x48`, keyed by
ordinal `+0x510`. Further stored fields come from `+0xDDC`, `+0xEA4`, and
`+0xEA8`, with current time from `0x425790`. The branch gates and data flow
are observed, but the setters' and record fields' meanings remain unresolved.
`0x422890` calls `0x415090` and `0x4150A0` (both return immediately), then
returns the same global address `0x507DB0`; therefore the five callback calls
write one `0x14`-byte record at offsets `+0`, `+4`, `+8`, `+0xC`, and `+0x10`.
`write_pcar_step_record` applies those little-endian DWORDs in order, keeping
the two backend return values raw. The sampled current time is used by the
nearby-object scan and is not one of these five record words.
`advance_pcar_callback_post_integrator` now composes that record write with
the following nearby-event scan, gated by reachability of `0x431150`; the
backend return values, current-time sample, and filtered candidate list remain
inputs. The event helper applies the raw selector/motion/time gates and updates
nearby state in scan order, then applies the selected target's `+0x29 = 1`,
`+0x2B = 0`, `+0x2A = 0` stores when the target span is present. The target
object span is supplied after resolving the returned target offset. Evidence is
the `0x4384D0` disassembly around
`0x43872F`-`0x4389B4` and the `0x422890` helper evidence cited below.

Evidence: `0x422890` `ev_99d1a5ef812c5d8d34234c5f54ea2b12581f7aef4bddbbf7128e3fa719d70342`,
`0x415090` `ev_0a6245398867d99afebbf36daae1eded82acb6bf777db22fa3f66e4c9950cbbd`,
and `0x4150A0` `ev_b5ad73fbc4bc5b19008cdf33db08d98f96ef68417199f3e3fdbc8877fbce1065`.

### PCAR callback global helpers

The small helpers near the callback are direct accesses to process globals:

| Helper | Observed effect |
|---|---|
| `0x43B9D0` | returns byte `0x509360` |
| `0x43B9E0` | returns byte `0x5093F0` |
| `0x43B9F0` | returns whether byte `0x5093CC` is nonzero |
| `0x43B930` | returns float at `0x5093F4` |
| `0x43B9C0` | returns byte `0x5093F8` |
| `0x43B940` | returns float at `0x509320` |
| `0x43B990` | returns overlapping global data beginning at `0x5093B8` |
| `0x43B9A0` | returns dword at `0x5093BC` |
| `0x43B9B0(value)` | stores `value` at `0x5093BC` |
| `0x43B880(a,b,c)` | stores its arguments at `0x509384`, `0x509388`, `0x5093A4` |
| `0x43BA00` | returns the float at `0x509344` |
| `0x43BA10` | returns the float at `0x509340` |
| `0x43BA20` | returns byte `0x509420` |

`0x43B950` computes `clamp(1.1 - 1.2 * float[0x5093C8], 0, 1)` and is
consumed by the PCAR callback at `0x4385CE`; the portable model exposes this
formula as `pcar_callback_value_0x43b950`. The arithmetic uses ordinary C++
float operations, so it does not preserve x87 extended intermediates.

The callback overwrites dword `+4` with bit 0 set when the signed dword
representation of `+0xDE4` is greater than `0x3DCCCCCD`, and bit 1 when the
signed dword representation of `+0xC34` is greater than `0x3ECCCCCD`.
`pcar_status_flags_0x4385f0` retains those bitwise comparisons.

The portable `PcarCallbackGlobals` state in
`include/viper/pcar_control_update.hpp` preserves selected raw addresses as
field labels; `set_pcar_callback_globals_0x43b880` models the three direct
stores. The overlapping value returned by `0x43B990` is left as a separate
raw input because its aliasing type has not been resolved. No semantic names
are inferred from these accessors.

### PCAR force-like effect branch

The callback invokes `0x433250` when `0x43B9F0` is true. That helper returns
without effect if byte `+0xE3D` is set or dword `+0x4FC` is 0 or 3. Otherwise
it compares the raw dword at `+0x48` against `0xBF4CCCCD` and `0x3E4CCCCD`:

- Above the first boundary, it normalizes the three floats at `+0x38`, scales
  them by 40000, and accumulates them twice. The application points are the
  center `+0x5C` with y offsets `+1` and then `-3`; the second vector negates
  x and z.
- Below the second boundary, it normalizes `(-(+0x44), +0x48, -(+0x4C))`,
  scales by 40000, and applies it at center `+0x5C` with y offset `+1`.
- Between the boundaries, it applies `(0, 40000, 0)` at center plus three
  times the vector at `+0x50`.

The accumulator `0x43E6F0` ignores an input vector unless its squared length
exceeds `1.1920929e-7`. It adds the vector to accumulators `+0x24C..+0x254`
and `(application_point - center) x vector` to `+0x258..+0x260`. The portable
offset-preserving model is `accumulate_pcar_effect` in
`include/viper/pcar_control_update.hpp` and `src/pcar_control_update.cpp`.
Normalization and accumulation use ordinary float operations and do not
reproduce x87 intermediate precision or the optional division-emulation path.

NCAR's `0x431150` integrator calls this same helper for its force and moment
contributions; `0x431130` is an observed three-argument forwarding wrapper to
`0x43E6F0`. `accumulate_ncar_force_moment` models its direct accumulator
effect, including the squared-length threshold, float-rounded application arm,
and extended intermediates before each stored float. Its threshold is strict;
zero, sub-threshold, and unordered squared lengths do not write either vector.
The caller determines the input vector and application point, and accumulators
retain their existing values. Host `long double` precision may differ from the
original x87 format.

Evidence: `ev_049bb4485cc19293f077224e678d7b1b099b4715a61c0ee476057a2030461ca1`
(`0x43E6F0` Ghidra dossier, including callers and assembly) and
`ev_8db8a1d5658a18c75d6ac745ac3cd8262d27a5255c639c9c24deeb0f3dbdb392`
(`0x431130` Ghidra pseudocode).

### NCAR wheel dynamics helper primitives

The repeated wheel updater `0x442530` uses three shared body helpers now
exposed in `ncar_state.cpp`. `transform_ncar_vector_at_point` models
`0x43E9B0`: `linear(+0x234) + angular(+0x240) x (point - center(+0x5C))`.
`accumulate_ncar_moment_vector` models `0x43E7F0`'s unconditional vector
addition to `+0x258..+0x260`. `accumulate_ncar_point_impulse` models `0x43E830`:
duration adds to `+0x2AC`, `impulse * duration` to `+0x294..+0x29C`, and
`impulse x (point - center) * duration` to `+0x2A0..+0x2A8`. The `0x43E830`
moment order is the reverse of `0x43E6F0`'s arm-cross-vector moment. Each
helper preserves float stores and uses host `long double` intermediates where
the original x87 holds intermediate results; host precision may differ from
x87 extended precision. These primitives do not reconstruct the full wheel
state machine or its ray/ground query.

The updater also advances wheel field `+0xAC` by field `+0x124` times global
scale `0x4C88DC`, stores the float, and performs at most one wrap by global
`0x4C88E0`. Its positive threshold comparison is signed on the raw float bits;
the negative threshold comparison is unsigned. This isolated operation is
modeled by `advance_wheel_angle_accumulator` in `vehicle_setup.cpp`; all three
global values are explicit inputs, and the helper does not reproduce other
wheel-update stages.

Evidence: `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`
(`0x442530` dossier), assembly `0x004425BB..0x00442609`.

The preceding entry prefix is modeled by `advance_wheel_update_prefix`: it
stores `+0xA8 = +0x124 * +0x5C`, samples global byte `0x50878C` through
`0x425870`, conditionally sets `+0x20` when the squared distance exceeds
global threshold `0x4C88D8`, then advances and wraps `+0xAC`. The referenced
point and runtime constants are caller-provided. Evidence: the same
`0x442530` dossier and assembly `0x0044254C..0x00442609`; `0x425870` is
`mov al,[0x50878C]; ret` at `0x00425870..0x00425875`.

The later conditional `+0x124` blend block (`0x00442805..0x0044286D`) is
modeled by `adjust_wheel_rate_blend` in `vehicle_setup.cpp`. Below the signed
raw-bit gate of 16, it caps a coefficient product at one, scales the signed
response local by its complement, and updates the signed rate by a separate
factor. Constants and the incoming response local remain explicit inputs.
Evidence: the same `0x442530` dossier.

The downstream positive-excess update (`0x004429AB..0x004429DF`) is exposed
as `adjust_wheel_excess_weight`. It preserves the raw signed-positive gate
and caps `(excess * 0x4C8924 + 1) * weight_0x2C` at one. Whether the upstream
response path was reached and both local values are explicit inputs; the
earlier contact classification and body-velocity projection remain separate.

Evidence: `ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`
(`0x442530` dossier and caller relationship),
`ev_62f22884a021b5e6b35a4599167abed076dab39b2e4e174c16d8beb7c69ad0cc`
(`0x43E9B0` pseudocode),
`ev_eaeef05664092817b3b70f8df578d140a9192fcbf7be27fb7f1c293497a70ec0`
(`0x43E9B0` assembly),
`ev_2d9e719221b3c0511e6d34af474a67522d7ab77fcaf1f5732ab6b88bccabd31b`
(`0x43E7F0` pseudocode),
`ev_b089d2a219b6ab79a55c710a5ffc24fec210b6ca976490adb2d698d57349ec20`
(`0x43E7F0` assembly),
`ev_29620bc7086becd747123fc4a9ea31d718e8edf670c058fad15a951bc97c63f0`
(`0x43E830` pseudocode), and
`ev_c93c6c36af307b59d4b6665e1f43c0f489148e62a2d48ab64c9f25db67f4a338`
(`0x43E830` assembly).

After four calls to the per-slot updater `0x442530`, `0x431150` increments
object `+0xD78`. If the signed incremented value is at least 8, it resets the
field to zero and calls `0x4323E0` with raw float bits `0x3E03126F` (0.128).
It then obtains a float through the callback at `+0xBFC` and stores it at
`+0xD70`, adds floats at `+0x5F8` and `+0x79C` into `+0xEB0`, and copies
`+0xCBC`/`+0xCB4` to `+0xD64`/`+0xD68`. The effect of `0x4323E0` and the
indirect callback remain external; `advance_ncar_integrator_bookkeeping`
models the counter and direct stores.

Evidence: `ev_2534fc83d0f6e0f4cea9397149195b61a7e7383fec5e886f81c33c3d9bc331f2`
(`0x431150` assembly, bookkeeping window).

Earlier in the same function, an embedded controller at `+0xC98` receives
direct stores to child offsets `+0x64` from car `+0xDE0`, then `+0x5C` from
car `+0xDEC`; `0x43FAE0` is called afterward with the byte at car `+0x534`.
`setup_ncar_integrator_child_control` preserves that order and reports the
controller invocation. `advance_ncar_animation_controller` reconstructs the
controller's six state branches (including mode-byte reset, index changes,
the 0.128 ramp, transition selection, and the callback-derived ratio clamp).
It takes the two virtual return values and child fields at `+0x34/+0x3C/+0x41`
as observations. It stores, in order, the `0x4402C0` argument to the pointed
target's `+0x2C`, then the `0x43F8E0` ramp value to its target's `+0x00`. Both
called functions are direct stores, so `advance_ncar_animation_controller`
applies them to explicit target state. The Pentium FDIV correction is reported
but not emulated.
The setters are `0x43FF80` and `0x43FF70` respectively.

Observed state transitions: state 0 detects a selection change and chooses
fade-out state 1 or immediate transition state 4; state 1 subtracts 0.128 and
uses the signed half-value and index predicates to route to state 2 or 3;
state 2 clears the ramp and enters state 4; state 3 sets the child callback
argument to float 1 and tests the product of the `+0x4C` callback value and
selected item against the `+0x50` callback value and child limit; state 4 adds
0.128 toward 1 and returns to state 0 when saturated; state 5 applies the
observed 0.7 and 0.2-related conditions. Except when the mode byte is zero or
state 3 remains active, the controller clamps the ramp to the ratio
`(callback_0x50 - child_0x34) / (child_0x34 + 1000 - child_0x34)`.

Evidence: `ev_1128dc7f406320fc5ef2f1ed4675611a89fdc97effbfc2c22b8811268e06d4ea`
(`0x43FF80` pseudocode),
`ev_498d3e18c8202f1635d22ed6fdab875a921c74ff081aee95b6a8369055328bf2`
(`0x43FF70` pseudocode), and
`ev_e3e3d51d09604ce173a3a18906c819e7484977c6a91f49f3edb6d87a451e26bf`
(`0x43FAE0` pseudocode).
The setter bodies are `ev_755fef1cdeed6baa5aed78d2c0b845c438520258e465c0fdecdfbe67eaf5fe76`
(`0x4402C0`) and
`ev_067b6043dabac8d052f56b5bf9ca2171b08aa6ac561f4576487a76d38d304ad0`
(`0x43F8E0`).

### PCAR nearby-object transition scan

After integration and record construction, the callback runs a 16-slot scan
when the signed dword at `+0x48` is positive and the dot product of
`(+0x234,+0x238,+0x23C)` with `(+0x50,+0x54,+0x58)` is greater than 3.0 (or
unordered). If that gate fails, it clears dwords `+0xED0` and `+0xED4`. The
scan itself also requires byte `+0xEEC` and elapsed time from `+0xEE8` greater
than 0.5 (unordered comparisons pass this x87 gate).

Each slot comes from `0x420930(index)` and must be non-null, differ from the
source, and pass its virtual `+0x30` query. For a candidate displacement from
the source's `+0x5C` position, the projection onto basis `+0x50` must be above
-5 and have signed raw bits below the 4.0 bit pattern. The projection onto
basis `+0x38` sets bit 1 for `(0.7, 7.0)` and bit 0 for `(-7.0, -0.7)`.
After all slots, a changed state is copied from `+0xED0` to `+0xED4` and the
current time to `+0xEE8`. State 0 selects pointer `+0xEE4`, state 1 selects
`+0xED8`, state 2 selects `+0xEDC`, and other states select `+0xEE0`; when
that pointer is non-null, bytes `+0x29,+0x2A,+0x2B` are written as `1,0,0`.
The constructor creates the targets with these keys:

| State | Target field | Resource key |
|---:|---:|---|
| 0 | `+0xEE4` | `cclear.sfx` |
| 1 (bit 0) | `+0xED8` | `cleft.sfx` |
| 2 (bit 1) | `+0xEDC` | `cright.sfx` |
| 3 (both bits) | `+0xEE0` | `cboth.sfx` |

The portable `PcarNearbyCue` enum names those strings.
`update_pcar_nearby_events` models the scan and reports the selected cue and
target. `apply_pcar_nearby_notification` applies the exact target writes at
`+0x29`, `+0x2A`, and `+0x2B` when a state transition selected a non-null
target. In instruction order, these stores are `+0x29 = 1`, `+0x2B = 0`, then
`+0x2A = 0`; the target object's other bytes are preserved.
Evidence: `0x438949` `ev_de9a278efc0fc98900b1d67bdb2b2d242a15819b880eb01833e12b509f67dd64`,
`0x43894E` `ev_89df755f3df572403a8c1932db6cc09926ad842203c89133c1e40f566c498d14`,
and `0x438952` `ev_d0d8846394de8f19a7affe4748cf95cc77a9387fefa60c094cbc3d4918a960b4`.

The preceding helper `0x43B8C0` starts from global float `0x509358`, or
`abs(value) * value` when byte `0x5093FC` is set. Its input is first compared
as a signed dword against `0x4178E38E`; only a greater value enters the float
path, where it is capped against the float constant at `0x4C836C`, transformed
by multiply/add constants at `0x4C8370` and `0x4C8374`, and multiplied by the
selected global value. This mixed integer-bit-pattern and floating-point
behavior is preserved by `pcar_global_scale_0x43b8c0`. The PCAR callback
passes the absolute value of the sum of three products (`+0x58 * +0x23C`,
`+0x54 * +0x238`, and `+0x50 * +0x234`); for nonnegative finite floats, the
signed raw-bit comparison is equivalent to a numeric threshold of about
15.55556. Another observed caller at `0x47E820` passes `0.0`, taking the
unchanged-global path. The three constants decode to approximately `44.44444`,
`-0.01730769`, and `1.269231`; no gameplay meaning is assigned to them.

## NCAR callback at `0x439200`

At entry, the method reads time from `0x425790` and checks `0x49BE40`. When
that predicate is false, it compares `current_time - object[+0xF30]` strictly
against the float at `0x4C8204` (bits `0x3D888889`, approximately 1/15 second).
The elapsed-time branch stores current time in `+0xF30`, performs
an update through `0x420930`/`0x43E050`/`0x4393B0`, then enqueues a 62-byte
payload from `0x4393B0` through `0x49DB20` with event code 0. It polls the
same queue through `0x49DBC0` with a 62-byte output capacity; a successful
copy calls `0x439490` with the returned sample. Thus `0x3E` is the payload
size, not the event code. `advance_ncar_callback_queue` composes this producer,
queue, and consumer prefix with the existing timed-event queue and sample
buffer models. It accepts the `0x4393B0` payload and queue clock values as
inputs; only a copied payload of exactly `0x3E` bytes is passed to the sample
buffer model. When `0x49BE40` is true, both enqueue and dequeue are skipped,
but the callback continues into its input/mode branch. The payload layout is
partly recovered. `0x4393B0` builds a 24-byte temporary
packed value, then passes it to `0x49DF60`, which expands it into the 62-byte
global record at `0x5091E8`; the callback queues that global record.
`0x49E210` performs the inverse direction from the producer object into the
24-byte temporary value. Helpers `0x49E340` and `0x49E060` convert the vector
region. The 62-byte record now has an offset-preserving decoded projection in
`NcarSampleRecord`; its raw bytes remain authoritative. Directly established
fields are `+0x00` (time), `+0x04` (timestamp converted from a packed tick),
`+0x08` (0 or
0.5), `+0x0C` (signed scalar from the packed nibble/sign bits), `+0x14` (low
three packed bits, with 7 mapped to -1), `+0x18` (byte scaled by 8000/255),
`+0x1C..+0x27` (three vector floats), `+0x28..+0x37` (four converted rotation
floats), `+0x38` and `+0x39` (individual packed flags), and `+0x3A..+0x3D`
(four unpacked 0/1 flags). These are storage/conversion observations; names
and gameplay meanings remain unresolved. The new projection exposes exact bit
patterns so consumers can choose float reinterpretation explicitly.

`0x49E340` copies the first three float components verbatim, then converts
three rotation components to signed 16-bit values through `0x49E370`. The
inverse `0x49E060` copies those first three components and calls `0x49E090`,
which maps each unsigned 16-bit value as `value * 40 / 32768 - 20` (range
`[-20, 60)`) and constructs
four rotation components using sine/cosine. The producer/consumer scalar,
small-integer, and flag bit packing is now mapped at the byte-operation level.
`encode_ncar_packed_payload24` writes the observed fields into the 24-byte
buffer: a low-word tick at `+0`, three vector floats at `+2..+0x0D`, three
encoded rotation shorts at `+0x0E..+0x13`, the scaled byte at `+0x14`,
signed-scalar magnitude/sign plus the low three integer bits at `+0x15`, and
the value threshold plus six flags at `+0x16`. The stack buffer in `0x4393B0`
is not initialized before `0x49E210`; byte `+0x16` bit 7 and byte `+0x17` are
not assigned. The portable projection zero-fills them and reports their mask,
rather than inventing stable values. `decode_ncar_packed_payload24` applies the
observed fields written by `0x49DF60` into an existing destination record,
including the tick-to-seconds conversion, scaled byte, scalar, integer
sentinel, vectors, quaternion, and unpacked flags. It preserves destination
`+0x00` and the bytes that `0x49DF60` does not write.
The tick word is `low16(tick_origin + ftol((sample_time - seconds_origin) *
1000 + 0.5))`, matching `0x420AC0`'s two epoch globals and constants.

`encode_ncar_rotation16` follows `0x49E370`: it applies `acos` to quaternion
`w`, takes the sine, and forms a rotation vector by multiplying each xyz/sine
quotient by twice the angle unless the sine magnitude is at most `1.1920929e-7`.
It clamps each rotation-vector component to `[-20,20]`, then maps it to an
unsigned short using `(component + 20) / 40 * 32768` and `__ftol` rounding.
The zero-sine path encodes three zero axis components, which quantize to
`0x4000`. Host long-double math and current-rounding-mode conversion approximate
x87; the Pentium FDIV correction is reported but not emulated, and pathological
non-finite quaternion inputs remain outside the portable helper's contract.
The queue timing offset is set by the chat latency handler described below;
its accepted integer is added directly to the selected clock value.

The packed tick conversion in `0x49E1D0` combines the 16-bit tick with the
current tick's high word, then adjusts by one `0x10000` epoch when the signed
difference exceeds either half-range boundary (`+0x8000` or `-0x8000`).
`expand_ncar_wrapped_tick` models that integer rule. The inverse rotation
decoder is represented by `decode_ncar_rotation16`: each unsigned component is
mapped by `value * 40 / 32768 - 20`; the three values form an axis-angle vector,
whose magnitude controls sine/cosine and whose normalized axis supplies the
quaternion vector. The near-zero branch returns identity. Long-double
intermediates approximate x87, but the helper does not emulate Pentium FDIV
corrections and may differ at threshold/rounding edges.
After epoch expansion, `0x420A90` converts the signed tick delta to seconds as
`delta * 0.001 + epoch_seconds`; `ncar_seconds_from_tick` preserves the
32-bit wrapped subtraction before converting to float. Its producer inverse
`0x420AC0` subtracts `DAT_00508190`, multiplies by 1000, adds 0.5, applies
`__ftol`, then adds `DAT_00508194`; the constants are observed as `1000.0`
and `0.5` respectively.

Evidence: `ev_92e0c1eb69aa42416641bb656cd657db97f32e1ef5f18508619044efedad558c`
(`0x439200` callback bytes),
`ev_0fd4f4ffcd253e2e44dd488c2a92311b5effae5472fe2af27d63a0b345fae1d5`
(`0x4C8204` interval bits),
`ev_3b67cac8275fe0a398f9a6b12ff5dc99e762ad20592fb48a23a450b630a8b3f0`
(`0x49DF60`), `ev_e29c46085d1e4d91b60df137a84e34b1465e5696d01fa8ac36bfe5a959b78f11`
(`0x49E210`), `ev_7ba0b8909a695a0c4268ff43243b5cacc763ba3867eda2f3328162be645569ba`
(`0x49E340`), and `ev_5c8755db85cade8348aa4cc6fa6b92c1ccde37c4ffc696bd95a76b6ee9e1121f`
(`0x49E060`), `ev_d51ef1605c6326b5656a998df9b75019e28233d7d25b1879aa67284b8529a357`
(`0x49E370`), `ev_a13a7bb8e4a53872961261e4bfa1ab71e4fb594950fa6ef9533aa4963dabd11c`
(`0x49E090`), `ev_eeeec60211b8df917b4d8008229078171d7c9a521c31825c4f484573be5f48cc`
(`0x49E1D0`), and `ev_3da9b7a76d60847a8094f03ff6a659d94f2f8ffddf9d5824cc87b6fdf3c2dc31`
(`0x420A90`), `ev_ff1bb2ce4cfe71909a38cff0ee9d2d0be606e0cfa9d5124f63939b09a45d57e2`
(`0x420AC0` assembly), `ev_8184d1aa2fdc1016052f4bcfd12a682725cdd2da0c608b0761ab9c5ab94755e6`
(`1000.0` constant), `ev_d6dea5bfbc41634a285c76ecdb596eaf3b3e3eab2bba7fafd214308950eb5d93`
(`0.5` constant), and `ev_2158d715ce59b4079ea402f08416d2e327fdac4a2ef2b9b64ff4c6c9dfce16bb`
(`0x49E210` assembly), `ev_08eeb441951b91ed628ea723f30ab7f3b30c4da3dc1921a25af628753a8d8b0c`
(`0x49E370` assembly), `ev_e5ac8cfcf0fb9e0e113aafd0669fa0d3dde52ca2912ca98b0dec68b5bf3633a1`
(`0x4BDFBC`), `ev_0e89df502dc83e140ae938a97a9cd7fe4d459a58653919ea2d7ccf114887df11`
(`acos` dispatch bytes), and `ev_6aa8070fb2344f0d28fec120c0388e6c89b4a2059b952c9dcbeece6d309dfeae`
(`0x4393B0` scratch-buffer caller).

The generic `0x49DB20` queue appends a payload, event code, and due tick
(`0x413BD0()` plus `0x49C590()`) to a linked FIFO. `0x49C590` returns
`DAT_004E3224`; setter `0x49C5A0` writes that global. Its sole observed caller,
the chat packet handler `0x4A3470`, searches the packet text for `latency`,
parses the following integer with `_atoi`, and applies the setter only when the
value is nonnegative and below 1000. The value is stored without scaling and
added directly to the clock value when a queue node is enqueued. The portable
`TimedEventLatency` plus `apply_timed_event_latency_command` models this
accepted-range/store behavior; callers pass its offset to
`enqueue_timed_event`. `0x49DBC0` examines the
oldest node and only dequeues it when its signed due tick is no later than
the current clock value. It copies the node if its size fits the output
capacity, optionally returns the event code, and dequeues the node either
way. A too-small output buffer therefore loses the due message and returns
false. `include/viper/timed_event_queue.hpp` and
`src/timed_event_queue.cpp` model these queue semantics with clock and
capacity supplied by the caller.

Evidence: `ev_0cfef64b37fd4d174ff0bfce8aed30d01c598e1b600971a4dfe75c0f62107b0b`
(`0x49C590`), `ev_23a99c38c32692e58c5be575ea11b98905ed0143aff700aeb64528917b9ae20c`
(`0x49C5A0`), `ev_2e5e75fca114123516ae6c6b12d61beeeffb0bb7017b03046974aec391b430e8`
(`0x4A3470`), `ev_af1d17b1fd579eed1c48404d50ad7ad8f86553f2cc3a78b9d83ab1637ee530bb`
(`0x49DB20`), and `ev_dbc78ef0e9a4522ea5e22417f43005db86c40315b4d4d08eb971bce9910a0e63`
(`0x49DBC0`).

The main body gates on `0x4390C0`:

- If true, call `0x439190`. A true return skips `0x431150`; a false return
  calls it at `0x4392BF`.
- If false, continue only when dword `+0xEC8 == 3`. The method then loads a
  record pointer from `+0xEC4` and calls a sequence of setters with values
  from record offsets `+0x0C`, `+0x14`, and `+0x08`, interleaved with zero
  values. It passes the byte at record `+0x38` to `0x434A80`, then calls
  `0x431150` at `0x439336`.
- After that second integrator call, it updates four bytes at object offsets
  `+0x574 + n*0x1A4` to 0 when record byte `+0x3A+n` is zero, and to 1 when
  that record byte is nonzero. `apply_ncar_callback_gate_bytes` models this
  normalized nonzero mapping; the meanings of these bytes are unresolved.
  It passes record dword `+0` to `0x438E10`; a false return sets object
  `+0x50C` to 1.0 and calls `0x43A130`, while a true return calls `0x439160`.

Before return, when `0x49BE40` is false, object `+0x50C` is set to the float
constant `0x3F4CCCCD` (0.8). The helper `0x4390C0` is independently observed
to compare current time against a value reached through record `+0xEC4`, plus
2.0; it also reads input through `0x40A160` when `+0xEC8 < 1`. The detailed
predicate and `0x439190` rotational initialization are recorded in the
progress notes, but the input record's semantic type is still unknown.

The isolated NCAR helpers further establish these effects:

- `0x438E10(timestamp)` returns true only when current time minus the supplied
  timestamp is strictly greater than `0.5`.
- `0x4390C0` returns false when `+0xEC8 < 1` and `0x40A160` does not return 1;
  otherwise it returns whether current time minus the float at record `+0xEC4`
  is strictly greater than `2.0`.
- `0x439160` compares the signed 32-bit representation at `+0x50C` against
  `0x3C83126F`; if greater, it subtracts `0.016`, otherwise it clears `+0x50C`.
- `0x439190` first runs that timer update. On its first call it clears fields
  `+0x234`, `+0x23C`, `+0x244`, `+0x240`, and `+0x248`, sets byte `+0xF34`,
  and stores bit pattern `0x414C0C4A` at `+0x238`. On later calls, the unsigned
  comparison `uint32(+0x238) > 0x80000000` returns true early. Otherwise it
  invokes setters at `+0xDE4`, `+0xDE0`, and `+0xDEC` with zero.

The offset-preserving portable subset is in `include/viper/ncar_state.hpp` and
`src/ncar_state.cpp`. `advance_ncar_callback_main` now composes the observed
input-gate result with `initialize_ncar_rotation`, the mode-3 gate-byte writes,
the direct setter values for `+0xDE8`, `+0xCBC`, `+0xD64`, `+0xDEC`, `+0xDE4`,
and `+0xDE0`, the `0x431150` entry reset, the `0x438E10` half-second branch,
and the final `+0x50C = 0.8` assignment when `0x49BE40` is false. It reports
`0x4330F0`, `0x434A80`, `0x431150`, and `0x43A130` as reached effects; only the
`0x431150` entry reset is simulated, and the numeric effect of `0x4330F0` and behavior of
`0x434A80` remain available only through their separate models.
`initialize_ncar_rotation` includes the
normal-path zero setter writes at `+0xDE4`, `+0xDE0`, and `+0xDEC`, preserving
those fields when the unsigned `+0x238` early-return gate fires. The input
predicate and producer payload are inputs; integrator effects, the actual
`0x434A80` target callback, and remaining mode-3 helper effects are not
executed by this composition.

`advance_ncar_callback_main_with_bookkeeping` adds an ordered outer composition
for callers that have the values produced by the four `0x442530` wheel calls:
it runs the same input/mode prefix and `0x431150` entry reset, applies the
integrator's direct post-wheel bookkeeping (`+0xD78`, conditional
`0x4323E0(0x3E03126F)`, sampled time `+0xD70`, sum `+0xEB0`, and copies
`+0xCBC/+0xCB4` to `+0xD64/+0xD68`), then applies the `0x439200` timer tail.
The wheel calls themselves, time callback return, addends, and copied source
values remain explicit observations. The bookkeeping is only marked reached
when the main callback reaches `0x431150`. Evidence for the bookkeeping stores:
`ev_2534fc83d0f6e0f4cea9397149195b61a7e7383fec5e886f81c33c3d9bc331f2`.
`advance_ncar_frame_callback` now joins the queue prefix to that main callback
composition in `0x439200` order. It reuses the queue sample time and
`0x49BE40` result for the later callback tail, while the `0x4390C0` result,
current callback record, backend callbacks, and wheel-produced bookkeeping
values remain supplied observations. The queue result is kept separately from
the state-record input because their runtime relationship is not yet proven.

Evidence: `ev_5b17d9f129b69c7430beb10935f988e94cf12f2277e9e51658e3dce983f14daf`
(`0x439190` pseudocode and its three setter callees),
`ev_e1b810a87a718d79f278854b8278c4ea874ebc0cb72a83f42e61b599303e5ce5`
(`0x439160` timer decay).

### NCAR linked-target update

After accepting a sample and reaching mode greater than 2, `0x439490` calls
`0x439110`. That helper proceeds only if mode is greater than 1, slot-zero
byte `+0x38` is nonzero and differs from slot-two byte `+0x38`, and the target
lookup at `0x43ABB0(car_ordinal)` returns a non-null pointer. It passes the
source object's twelve dwords at `+0x38`, vector at `+0x234`, and the accepted
sample's `+0x04` value to `0x43AD70`.

`0x43AD70` always clears target dwords `+0x240`, `+0x244`, and `+0x248`. It
continues only when `now - 2.0 > target[+0x478]`; then it copies twelve dwords
to target `+0x38` and records `now` at `+0x478`. The remaining update requires
`0 <= now - sample_parameter <= 0.1`. It queries the current type string and
compares it case-insensitively with "plane":

- For "plane", target vector `+0x234` becomes input vector plus
  target `+0x50..+0x58` multiplied by `31.11111`; target `+0x5C..+0x64` has
  target `+0x44..+0x4C` subtracted component-wise.
- Otherwise, it adds source dwords interpreted as floats at `+0x50..+0x58`
  times `(elapsed * 31.11111 + 3.5)` to target `+0x5C..+0x64`, adds `0.5` to
  the middle component, then writes target vector `+0x234` as target
  `+0x50..+0x58` multiplied by `sqrt(input_vector·input_vector) + 31.11111`.

The portable model keeps the target offsets raw and exposes the wrapper gate
as `should_update_ncar_linked_target` and the helper arithmetic as
`update_ncar_linked_target`. Field purposes remain unassigned.
`update_ncar_sample_linked_target` composes the accepted `0x439490` record
shift with the `0x439110` mode/slot-byte gate and `0x43AD70` update. It reads
the newest slot's `+0x04` sample parameter after the shift and preserves the
target lookup result and source-object `+0x38/+0x234` values as observations.
It does not call the game's target lookup or current-type-string getter.
Evidence: `0x439110` `ev_93b1cff1b5a647d3a30db15188a73ec7aa4af4b8c48fbe2472c8345a7dd5d1de`,
`0x439490` `ev_1e41695003417eaa3cd8324d7c21f4c6c4cef9c5c7c8e276ede14c2e5ae8ad34`,
and `0x43AD70` `ev_f98ad28368adb259a46778eef5383ed0424b9218923da4b1193f3d305191423b`.

`0x434A80` is a second wrapper around `0x43AD70`. Its enable predicate is the
conjunction of globals `0x4D10FC` and `0x4ECCC0`; it then requires the callback
byte at `+0xE28` to change to a nonzero value and a non-null target selected by
the object's car ordinal at `+0x510`. In global mode 11 it passes the twelve
dwords at `0x5081A8` and a zero vector; otherwise it passes the instance data
at `+0x38` and `+0x234`. Both cases pass current time as the sample parameter,
so the age gate in `0x43AD70` sees zero elapsed time. The wrapper stores the
new `+0xE28` byte on every call, including calls that do not update a target.
`run_ncar_linked_callback` models this dispatch and cache write. The global
enable bytes and target lookup are explicit inputs rather than hidden globals.

### NCAR timed interpolation callback

`0x43A130` runs its main callback chain only while byte `+0xF20` is zero and
`now - dword(+0xF2C) > 0.5`; the chain calls `0x43A190`, `0x439A10`, and
`0x439DB0`. Independently, a nonzero byte at `sample_window_pointer + 0x39`
calls `0x439D30` and is then cleared. The latter computes two components from
the record's `+0x28..+0x34` floats, passes them through an indirect method at
vtable offset `+0x40`, stores current time at `+0xF2C`, and sets byte `+0xF20`.
The `NCAR` vtable at `0x4C8238` maps slot `+0x40` to `0x433620`.
`advance_ncar_timed_callback` models both independent gates, reports each
direct helper call, clears the sample flag after `0x439D30`, and now composes
the virtual target's recovered parent effects. It decodes the sample vector at
`+0x1C`, derives the two callback components from quaternion values at
`+0x28..+0x34`, and models `+0xF2C = now` and byte `+0xF20 = 1`.
`advance_ncar_timed_frame` composes these recovered stages from one sample-window
snapshot and current-time observation. It updates the interpolation ramp,
feeds the updated `+0xEFC/+0xF0C` values into the `0x439A10` reset gate, runs
the rotation stage after `0x439A10` returns, and then handles the independent
sample flag. If `0x439A10` invokes `0x439D30` and `+0x39` is also set, the
wrapper's later flag branch can invoke it a second time, as in the observed
call order. The NCAR virtual method first invokes the secondary group at
`this + 0x478` through virtual slot `+0x14`. The group target `0x42D0A0` walks
its `+0x4C` child count and invokes that slot on each child; child target
`0x42CC10` copies the three DWORDs at child `+0x64` to `+0x20` and clears byte
`+0x88`. The group slot is invoked again through `0x4338C0` after resource and
wheel resets, so the model applies two passes and counts two child calls per
child. Child state is supplied with the target state; a missing group pointer
is reported as a reached invalid dereference. The NCAR virtual method then
copies the sample vector to `+0x5C`, builds
the orientation basis at `+0x38`, clears six velocity DWORDs at `+0x234..+0x248`,
clears `+0xD70/+0x50C`, writes `now` to `+0xE44`, and sets bytes `+0xE3C/+0xE3D`.
Its helper chain zeroes embedded `+0xCBC` and copies it to `+0xD64`; flags
counted child slots; clears four callback-slot bytes and the `+0x48C/+0x48D/+0x48E`
block; and clears `+0xC0C`. It appends event type 7 with payload
`{byte(+0x510), 0}` and length 2. Five child resource passes and secondary
object virtual methods remain reported call effects. The five resource passes
now copy the first three DWORDs for each destination-counted row with a `0x20`
byte stride and clear the registered `+0x15` flag while preserving each
descriptor pointer. Short spans report unprocessed rows; downstream resource/
device behavior remains external. The model reports how much of the supplied
child-flag span covers the native count.

The four callback slots begin at `+0x554` with `0x1A4` stride. Their
`0x441B90` reset clears DWORDs `+0x13C`, `+0x124`, `+0xE8`, `+0xDC`, `+0xA4`,
`+0x10C`, `+0x16C`, and `+0x158`, plus bytes `+0x168` and `+0x20` in each slot.
The model now applies these exact writes to four offset-preserving slot states.

For finite nonzero components `a` and `b`, its orientation rows are
`(b/L, 0, -a/L)`, `(0, 1, 0)`, `(a/L, 0, b/L)`, where
`L = sqrt(a*a + b*b)`. The model routes each reciprocal-length division
through the existing finite-normal FDIV approximation when the compatibility
flag is set; exceptional x87 behavior remains approximate. Evidence: vtable bytes
`ev_6e32a6817044acefa8764aeeb4fe1e050abcb8b63cbc9a5ef2dc9aa9528da753`,
constructor/vtable write `ev_32889c6ea19b675c584a2a82897777c119f9e26f45a391f109b8b4ab608a610a`,
`0x439D30` `ev_4035b1285ed30a04f1640a355504319d1da692aaf75b76639be736a56a2514f4`,
`0x433620` `ev_4e20ce0d4acd4d4639db7448c6fb997dd001d622a1c425c488e0481e7c0f8c26`,
`0x4331E0` `ev_f974ae5591cab735d624fdac9decd3aa90e5f4ea3ea6eae34bb3c5e127a277bf`,
`0x4338C0` `ev_84bbec06835985531a2b09bdad49d78002149029985f83121d821528d234097a`,
`0x433920` `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`,
`0x441B90` `ev_4a2b61d33988864abf28dc3967396235c19b9e10`,
group creation and traversal `ev_7884fb79acfb26d4c7b0004cd2a1679669a9b097e9fd4e49a6650ec607b8731e`,
`ev_5d28c6b28248475019e59e99da0c1ada50c94b768203aac20dca9e4f7e84e876`,
`ev_b568de17134e6a04d94f9cb497f37ad3da071d14295b40e6078752b74ec1cade`,
child target `ev_464f63788b649460ec644f40b8d24e71ad3c91ececfd` and vtable
`ev_fdade6325cc82d40fe3da7e815b06918bd0557728c174681ec27f639abf60d77`,
cross-product helper `0x434BF0` `ev_ac3fb6731a02c8b3b38810c3f7f4e410b2d37c0086429db5c04c8b6dda4ff2d7`,
and event helper `0x4273C0` `ev_49606e49141a3606cd6e046bc10936586662df1207ab8af724502df1c10b8ade`.

Evidence: `ev_7f433d793b1d8af66c67c7c6282647f3f2339a177f1ae96820f3ea816586b058`
(`0x43A130` Ghidra pseudocode) and
`ev_85596c056d78dcde80c2ca3e8f00b3163268da942db18d5374775c4dae7cf97b`
(`0x439D30` Ghidra pseudocode), plus
`ev_cc8d2f9f8c4d2414dddc28c9da17ba2209cb91bb7ecd8b5a3905340070c8c313`
(`0x439D30` instruction listing).

The numeric portion of `0x43A190` uses the difference between the first two
sample timestamps, doubled, as a wait interval. After `now - +0xF1C` reaches
that interval, it ramps four fields at `+0xEFC..+0xF08` toward the four values
at `+0xF0C..+0xF18` over seven more intervals, then copies the target values.
Every call scales the three values at `+0x240..+0x248` by
`1 - field[+0xEFC]`. `advance_ncar_timed_interpolation` represents these numeric
writes; it preserves offsets and uses ordinary float operations, while the
binary's x87 intermediate precision and `DAT_004E9754` division emulation are
not reproduced exactly.

`0x439DB0` interpolates rotations from the three sample records. Its two
`0x43A080` calls compute the unclamped fractions `(now - slot1.time) /
(slot0.time - slot1.time)` and `(now - slot2.time) / (slot1.time - slot2.time)`,
then slerp slot 1 toward slot 0 and slot 2 toward slot 1. `0x4C5F00` flips the
first quaternion when the dot product is negative, uses a direct copy when the
sine term is below `1.1920929e-7`, otherwise performs shortest-arc spherical interpolation and
normalizes the result. The callback then slerps those two estimates using
`+0xF24` and increments that field by `+0xF28`.

The weight from `0x43A0E0` is `0.2` times the number of four slot dwords at
`+0x554+n*0x1A4+0x19C` that equal 10; the constant at `0x4C8220` is `0.2`
(`CD CC 4C 3E`). `0x439DB0` multiplies `+0xEFC` by one minus that weight,
converts the existing row-major matrix at `+0x38..+0x58` to a quaternion,
slerps it toward the sample quaternion, converts it back, and writes all nine
matrix floats. `advance_ncar_rotation_blend` models this path and leaves sample
times/quaternions, wheel-slot dwords, and the already-updated `+0xEFC` as
explicit observations. The Pentium FDIV correction and exact x87 transcendental
rounding remain unimplemented.

Evidence: `ev_f68aaa16e3a6de7ec9bd93468640e1c2d633e5e2286b4ddbffb8b81f9fb8ad63`
(`0x439DB0` pseudocode),
`ev_b1d1bab5b9a5f3a14d46b9c376317746f82f8fc0aa08ee0120f4072b4c5158df`
(`0x439DB0` assembly),
`ev_19e5251ee0cb50b47552dd57d1d6b040979a95d87b8133e1d4c623f096d69432`
(`0x43A080` pseudocode),
`ev_042c710fec58d7e8ccbbaeecd9d7dd3e4ad0863583398cc617d329a45037446f`
(`0x43A0E0` pseudocode),
`ev_23aa287be5f2ae599520e942f0080e2abf2198d66bc887d30ad68567bc683e7a`
(`0x43A0E0` assembly),
`ev_d9f32a66cfe0d0bf4f599ec58983914643ded5a0548ad64e43cee11b173edbbb`
(`0x4C5F00` pseudocode), and
`ev_fadaf3bf47039f834e1b3d8ac4a1f2fd28a1661949212b8bf555d3e1599f82cc`
(`0x4C8220` bytes).

`0x439A10`, the next member of that callback chain, predicts position from
the newest sample's vector at `+0x1C`, `now - sample[+0x04]`, current vector
`+0x234`, and three values at `+0xEF0..+0xEF8` using the observed quadratic
`((a*dt*0.5 + v)*dt + p)`. It also derives a sample velocity from the three
sample vectors at `+0x1C`, `+0x5A`, and `+0x98` by evaluating the derivative
of their quadratic interpolation at `2*now`; those samples use timestamps
`+0x04`, `+0x42`, and `+0x80`.

When `abs(+0xEFC - +0xF0C) <= 1.1920929e-7` and the squared position error is
at least `83.72251`, it calls `0x439D30`, which produces the same indirect
callback arguments and `+0xF2C/+0xF20` latch writes modeled by
`apply_ncar_sample_callback`. Otherwise, it blends current position `+0x5C`
toward the prediction and current vector `+0x234` toward the sample velocity,
using per-component weights at `+0xF00..+0xF08`. The observed primitive
`0x438D30` is exposed as `blend_ncar_vector`. `advance_ncar_position_prediction`
models this branch; the indirect callback's target effect and Pentium-FDIV
correction remain unresolved.

Evidence: `ev_35b212984782237fd2b79a7b86bb6b1ee0f0701ac470f96592230a8871325277`
(`0x439A10` pseudocode),
`ev_082a0218c9e724005361f50ccc634a1ffe4d26756ddfd0689f35925bb85fc606`
(`0x439A10` assembly),
`ev_3756283fdb0b0506686b9972b94bfc14338957354b8bd08dc0f193adc6c92be5`
(`0x438D30` pseudocode), and
`ev_1b8ba4c6e1b6d7537ded29ba5f0e211fec56d1f366e72772b2b5c1c7261a4547`
(`0x438D30` assembly).

### NCAR constructor and sample window layout

The `NCAR` constructor `0x438E40` calls base constructor `0x42FF90`, clears
four 12-byte helper records at `+0xECC`, `+0xED8`, `+0xEE4`, and `+0xEF0`,
and sets bytes at `+0x6F4 + n*0x1A4` to 1 for four slots. It clears dwords
`+0x538..+0x550` (stride 4), `+0xF30`, and `+0xEC8`; initializes `+0xF1C`
to raw bits `0xC47A0000` (-1000), `+0xF20` to 1, `+0xF24` to 1.0, and
`+0xF34` to zero. Four interpolation fields at `+0xEFC..+0xF08` start at
`(0.4, 0.4, 0.1, 0.4)` and are copied to target fields `+0xF0C..+0xF18`.

It allocates `0xBA` bytes for three `0x3E`-byte sample slots, initializes each
slot through `0x43A3B0` (a no-op), then clears all `0xBA` bytes. The portable
`initialize_ncar_constructor_storage` applies these direct writes, including
vtable `0x4C8238` and the two object pointers at `+0xEC0/+0xEC4`, when supplied
with an already allocated nonzero 32-bit sample-window address and buffer.
Allocator failure handling and the constructor's external global event-queue,
manager registration, and base-constructor effects remain outside this write
helper. Evidence: `0x438E40` `ev_19bf81abe7b7839248d8b07d41978221f7062938726e062ee76cf5788aa0a9b1`,
`0x438E30` `ev_fa18b2d4949c7e658a21f926e1bdd21f1d40ac98b31b566b4e74186f156756a2`,
and `0x43A3B0` `ev_0ca4925317f69e08f76e11febcfebda8e132743cdd6527b8762d29076ba06d96`.

The matching teardown at `0x439040` reinstalls vtable `0x4C8238`. If the
event processor is inactive, it destroys the global delay queue when present,
frees its wrapper, and clears the global pointer. If active, it calls the
shared-manager unregister method. It then passes object `+0x510` to
`0x41AFC0` to release and clear that indexed manager slot, releases the sample
window pointer at `+0xEC0`, clears object pointers `+0xEC0/+0xEC4`, and calls
base cleanup `0x4303F0`. `plan_ncar_destruction` records this ordered action
sequence from observed global/pointer state; `apply_ncar_destructor_field_writes`
applies the direct vtable and pointer clears. Evidence: `0x439040`
`ev_06542682aa9134d98c43f266c11ef2fd20e59e42f7ef0e3fa9af9fefdeda83b0`,
`0x41AFC0` `ev_6d380cc9f50d8a4ee18e886fb7fafa96883bed95a23af8aa0d81e9f614a9b1ae`,
`0x49C290` `ev_a5d6793a734304781a4a4c3cbc397baab5f8cd7f1af06e120dbd7874bda1c0c2`,
`0x49DAF0` `ev_0f02be7caf01eafe35e421f018b7cbf240409d55a843bad44641620cecf1ebf4`,
and `0x4303F0` `ev_cb2c4de89710a3eceebd6e0ef85842cb1c233180dfc332309fcd9625d93a5bbd`.

Further analysis of `0x439490` and `0x439520` establishes a three-slot raw
window rooted at the pointer in object `+0xEC4`. Each slot is `0x3E` bytes;
the three bases are `+0`, `+0x3E`, and `+0x7C`. `0x439490` shifts `0x7C`
bytes from the first slot to `+0x3E`, copies a `0x3E`-byte input record into
the first slot, and overwrites its dword at `+0` with current time. It accepts
the new record when `+0xEC8 == 0` or when existing record `+4` is below input
record `+4`. If the mode is greater than 2 it calls `0x439110` and
`0x439520`; otherwise it increments the mode.

The `0x7C`-byte overlapping copy resolves to CRT address `0x4BD4F0`. Ghidra
identifies its implementation as one of the `_memcpy`/`_memmove` matches, and
the body explicitly detects `src < dst < src + size` and copies backward.
Thus this call safely shifts the old first and second slots into slots two and
three before installing the incoming slot. `update_ncar_sample_buffer` models
the exact shift, `+4` acceptance comparison, timestamp replacement, and mode
transition; its result reports whether the post-accept helper chain runs.

The callback and helpers directly consume these fields within each slot:

| Slot offset | Direct use |
|---:|---|
| `+0x00` | timestamp overwritten by `0x439490`; read by `0x4390C0` and `0x438E10` |
| `+0x04` | compared by `0x439490`; read at all three slots by `0x439520` |
| `+0x1C`, `+0x20`, `+0x24` | three values at all three slots, consumed by `0x439520` |
| `+0x38` | byte passed to `0x434A80` |
| `+0x3A` through `+0x3D` | four bytes normalized as 0/1 into object `+0x574 + n*0x1A4` (zero stays zero; nonzero becomes one) |

The first three outputs from `0x439520`, at `+0xECC`, `+0xED0`, and `+0xED4`,
are the derivative at sample 0 of a quadratic through the three vector values.
For sample parameters `x0..x2` from each slot `+4`, the coefficients are:

```text
c0 = (2*x0 - x1 - x2) / ((x0 - x1) * (x0 - x2))
c1 = (x0 - x2)        / ((x1 - x0) * (x1 - x2))
c2 = (x0 - x1)        / ((x2 - x0) * (x2 - x1))
derivative = c2*y2 + c1*y1 + c0*y0
```

The portable `derive_ncar_sample_vector` models this first output triplet on
the ordinary divide path for distinct finite parameters. `0x439520` copies
object `+0xECC..+0xEE3` to `+0xED8..+0xEEF`, shifting the current three-vector
into the first history slot and the prior history vector into the second. It
computes another derivative triplet at `+0xEF0`, `+0xEF4`, `+0xEF8` from the
current vector and those two history vectors. If its squared magnitude is
greater than `96.23611`, it normalizes the vector and scales it to `9.81`.
`derive_ncar_history_derivative` models this gate and scale. The final
`advance_ncar_interpolation_ramp` helper decrements `+0xF24` and calculates
`+0xF28 = (1 - new_0xF24) / ((x0 - x1) * 60)`.

`advance_ncar_sample_dynamics` now composes those pieces in the observed order:
shift the prior current derivative to `+0xED8`, shift the old first-history
vector to `+0xEE4`, derive the new `+0xECC` vector from the three raw samples,
derive the `+0xEF0` history vector from the new and two shifted derivatives,
apply the strict `96.23611` squared-magnitude cap, then decrement `+0xF24` and
write `+0xF28` using sample parameters 0 and 1. The initial history values stay
explicit in `NcarSampleDynamicsState`. This models the ordinary division path;
the compatibility-divider and x87 precision limits described below still
apply. Evidence: `ev_923cded32bd825c6432928cd4d514db473c89750a2f5589e1f643edf258a82ea`
(`0x439520`, full function dossier).

The arithmetic-mode global `DAT_004E9754` is initialized by the MSVC 1998
`__fpmath` routine at `0x4BC260`, which stores the result of
`__ms_p5_test_fdiv`/`0x4BE370`. A nonzero result takes the compatibility divide
helper `0x4BC437`; zero uses native x87 division. In `0x439520`, observed
compatibility-dispatch operation IDs include `0x12` and `0x0D`; the dispatcher
passes selected x87 register tuples to `0x4BC320`, whose body extracts and
normalizes 80-bit significand/exponent pieces and consults data at
`0x4E9770..0x4E9788`. Reversed memory division sites call the MSVC runtime
`__adj_fdivr_m32` at `0x4BC9FC`, which conditionally executes the reversed
x87 divide helper `0x4BC8E9`. This means matching the compatibility path
requires more than changing the ordinary quotient precision. The current
portable helpers use host `long double` intermediates where available and
model the common finite-normal significand-table correction, but do not claim
exact 80-bit x87 rounding or complete exceptional-case handling. The helper
and remaining gaps are documented in `metadata/ncar_sample_dynamics.md`.
`decode_ncar_sample_window` preserves all raw bytes and exposes observed bit
patterns separately without naming unknown fields.

## Portable implementation boundary

These callbacks call shared setters and helpers whose field meanings are not
yet resolved. The PCAR scalar input calculation and four direct stores are
represented in portable source, but neither full class callback is translated.
The next code step is to recover remaining helper effects and record layouts,
then model the numeric gates without guessing labels.

## Shared PCAR scalar helper effects

Direct instruction inspection of `0x4331A0` through `0x4331D0` establishes
these stores: `0x4331A0` writes its float argument to object `+0xDE0`,
`0x4331B0` to `+0xDE4`, `0x4331C0` to `+0xDEC`, and `0x4331D0` to `+0xDE8`.
These are exact destinations; their application-level labels remain unknown.

`0x4330F0` subtracts object `+0x500` from its input, clamps the difference to
`[-0.24, +0.24]`, adds that result to `+0x500`, and clamps the sum to
`[-1.0, +1.0]`. It multiplies the clamped value by object `+0xDCC`. If byte
`+0xBE4` is zero, the product is written to `+0x630` and `+0x7D4`; otherwise
the negated product is written to `+0x978` and `+0xB1C`. Only the selected pair
is written. The portable implementation in
`include/viper/pcar_control_update.hpp` and `src/pcar_control_update.cpp`
uses offset-preserving names and models finite inputs; the input's semantics
remain unassigned.

## Embedded PCAR helper object

The parent wrappers further establish the boundary of the object at parent
`+0xC98`. `0x4331E0(value)` calls `0x43FAD0(value)` on that subobject, then
loads parent field `+0xCBC` and copies it to parent `+0xD64`. `0x433210` and
`0x433230` call `0x43FDD0` on the same subobject and perform the same parent
field `+0xCBC` to `+0xD64` copy. The setter `0x43FAD0` itself writes its
argument directly to subobject `+0x24`.

`0x43FA30` initializes this embedded selector's numeric fields: `+0x18`,
`+0x1C`, `+0x20`, `+0x24`, `+0x28`, `+0x4C`, `+0x5C`, and `+0x60` to zero,
sets byte `+0x58` to zero, and stores 4 at `+0x68`. The setup helper
`0x43FA80` stores its count at `+0x20`, copies that many floats from its
supplied table to `+0x2C`, and stores its two pointer arguments at `+0x50`
and `+0x54`; the constructor call passes the curve object at parent `+0xBFC`
as the first pointer and parent `+0xC7C` as the second. The selector's
`+0x68` condition is therefore initially nonzero; the portable model defaults
that boolean gate to true.

The base car constructor `0x42FF90` initializes the object at parent `+0xBFC`
through `0x440100`. That routine installs vtable `0x4C8780`; its virtual `+0`
slot is `0x43F970`, which only returns the float at curve-object `+0x10`.
`0x440100` also initializes fields used by the selector, including
`+0x18 = 0x43142000`, `+0x1C = 72000.0`, `+0x20 = 1/72000.0`,
`+0x24 = 0x43910000`, `+0x28 = 0`, `+0x34 = 0x43142000`, `+0x38 = 0`,
`+0x3C = 6000.0`, and byte flags `+0x40` and `+0x41` set to zero. It also
sets `+0x68 = 0x43910000`. Thus the selector's virtual getter has no observed
write effect, and its returned value is ignored by `0x43FDD0`.

Later initialization calls `0x4401D0` on that same curve object. It copies
six floats from its `param_4` array into offsets `+0x44`, `+0x48`, `+0x4C`,
`+0x50`, `+0x54`, and `+0x58`; calls `0x43F910` to store `param_1` at `+4`,
`param_2` at `+8`, `param_3` at `+0x14`, and `1.0 / param_2` at `+0x0C`;
then stores `param_5` at `+0x3C` and `param_6` at `+0x34`. It sets `+0x10`
to `1000.0` and computes `+0x30` as
`param_6 / ((param_5 / max(param_6, 1000.0)) * curveValue(param_6)) * param_3`,
where `curveValue` uses the same quadratic, linear, intercept, offset, and
penalty fields described above. This directly ties the evaluator's
coefficients and cutoff to configuration inputs while leaving their
source-record meanings unresolved. `configure_update_curve` models these
configuration writes and derived value for ordinary finite inputs.

The linear body at `0x43FDD0` uses subobject fields `+0x18`, `+0x1C`, `+0x20`,
`+0x24`, `+0x28`, `+0x2C` onward, `+0x50`, and `+0x68`. When `+0x1C <= 0`
and `+0x24 <= 0`, it stores 1 at `+0x24`. When the absolute value at `+0x18`
compares below or equal to the constant at `0x4C8668`, it also stores 1 and
returns. Otherwise it reads through object `+0x50`, iterates the count at
`+0x20` over float entries beginning at `+0x2C`, calls `0x4404A0`, and may
store a selected index at `+0x24` through `0x43FAD0`. A nonzero `+0x68` and
positive selected index can preserve the prior `+0x24` instead. The numeric
selection metric is reconstructed below. Entry meanings remain unresolved.

The score call `0x4404A0` inside this loop is fully analyzed. It computes
`0x4404D0(curve, x, scale) - 0x43FA20(curve, x)`. `0x43FA20` is
`curve[+0x14] * x`. `0x4404D0` behaves as follows:

- If byte `curve[+0x41]` is zero and `x < curve[+0x3C]`, compute
  `((curve[+0x48]*x + (x*x*curve[+0x44] - max(x-curve[+0x54],0)*curve[+0x58]) + curve[+0x4C]) * scale)`.
- If byte `curve[+0x41]` is zero and the cutoff test fails, return zero.
- If byte `curve[+0x41]` is nonzero, return 80 when byte `curve[+0x40]` is
  nonzero, otherwise return zero.

`include/viper/update_curve.hpp` and `src/update_curve.cpp` model this
piecewise arithmetic for ordinary finite inputs, retaining raw offsets. The
inputs' application-level meanings and extended-precision edge behavior are
not assigned.

The rest of the `0x43FDD0` selection arithmetic is also reconstructed. It
first applies its index and denominator guards, then calls the curve object's
virtual `+0` method and continues with the curve gate and candidate scoring.
Car construction calls `0x440100` on the curve object at parent `+0xBFC`;
that routine installs vtable `0x4C8780`, whose first slot points to `0x43F970`.
`0x43F970` returns curve field `+0x10`, and `0x43FDD0` discards that return.
The virtual call therefore has no observed state-changing effect on the fields
used by the numeric selector:

1. If `+0x1C <= 0` and `+0x24 <= 0`, write 1 to `+0x24` and return. If the
   fields differ, return without changing `+0x24`.
2. If `abs(+0x18) <= 1.1920928955078125e-7`, write 1 and return. Otherwise
   call virtual slot `+0` through the object at `+0x50`; it reads `+0x10` and
   returns without a write.
3. If curve `+0x34 <=` object `+0x28`, write 1 and return. Otherwise start
   with `base = object[+0x28] / object[+0x18]`. Compute
   `step = (min(curve[+0x38], 1.0) * 500.0 + 500.0) * curve[+0x3C] *
   0.0001176470614`. For each entry `i` at object `+0x2C + 4*i`, use
   `x = base + step` only when `+0x1C > i+1`; otherwise use `base`. Evaluate
   the entry times `x` through `0x4404A0`. Scores are set to zero when
   `entry*x > curve[+0x3C] - 200.0`.
4. Keep the greatest score starting from zero; the candidate-index register
   starts at `+0x1C`. Equal scores replace the previous candidate, so a tie
   selects the later one-based index. If byte `+0x68` is nonzero and the
   selected index is positive, preserve the prior `+0x24`; otherwise write
   the candidate-index register. If there are no entries or every score is
   negative, that register retains its initial `+0x1C` value.

The implementation `update_embedded_selection` models these numeric branches
from the current curve fields. Iteration is bounded by both the observed count
and the supplied span; valid callers must supply at least that many entries.

### Four-wheel body-step accumulator order

In `0x442530`, the unconditional `0x43E7F0` addition occurs at `0x442C04`, before the later terrain/contact and collision-impulse work (`0x43E830` at `0x44302A`). `derive_wheel_moment_vector_0x43e7f0` now models the supplied vector: transform wheel `+0xB0..+0xB8` with `0x422F30`, cross the transformed axis with car axis `+0x44..+0x4C`, combine updated wheel spin `+0x124` with prior spin `+0x13C` and filter values `+0x140..+0x148`, then scale by wheel `+0x34 * 12.5`. The four-wheel composition can derive and accumulate this vector at the observed pre-contact point. Wheel-local `+0xB0`, prior `+0x13C`, `+0x140..+0x148`, and `+0x34` remain explicit inputs; the result exposes the `+0x13C` store of current spin. The later `0x443F60` basis call returns the next `+0xB0/+0xB4/+0xB8` axis in `wheel_moment_local_axes_next_0xb0`, which can be carried into the next step. Evidence: `0x442530` assembly `0x442BC1..0x442C09`, `0x422F30` pseudocode, and `0x43E7F0`.

### Post-body integrator tail

The observed tail immediately after 0x43ECE0 clears car fields +0x37F..+0x381, transforms the source vector at +0xD88 by the updated orientation at +0x38, then subtracts half of +0x385 along the body axis at +0x50..+0x58 and stores the result at +0x382..+0x38A. The composed four-wheel step now derives this matrix-vector result from the orientation produced by body integration rather than taking the transformed value as an observation. It conditionally updates three damped angular accumulators at +0x3A9..+0x3B1. The angular gate is strict squared axis length greater than 0.001 for (-axis.x, 0, axis.z). The +0x38C maximum-speed store is reached only when the four contact bytes sum to 4 and 0x433A00 returns zero; the helper returns a bitmask of signed DWORD comparisons at +0x670/+0x814/+0x9B8/+0xB5C against 0x3FB9999A. It stores max(current speed, previous maximum). The post-body result also records the unconditional final global-interface vtable +0x38 callback and its vehicle ordinal; native side effects remain outside the model. Evidence: analyze_function 0x431150, ev_fb7a9b21717cfb6308ed0b4ff5c8e3f5b185399be225d0fbbfbde36ee68b1217; 0x433A00, ev_cd20ad2db9adef1758cfb0c521badb3c0f694ec8ad8a4c9d316636b5b34016a5; assembly 0x431E5D..0x432120.

### Mode-specific force branch before 0x43ECE0

The 0x431150 mode 0/1 branch computes an accumulator force from the angular projection onto +0x44, linear velocity, four raw wear-field comparisons, and scales at +0xDCC/+0x500. Mode 0 uses the strict >1 velocity normalization and the 177.77777 speed-squared transition; mode 1 uses the <1 velocity adjustment and the 0.2..1 angular-response clamp. Both paths require the four observed contact bytes to sum above 2, wheel damage bytes at +0x574/+0x718 to be clear, and a forward projection strictly above 0.1 before calling 0x43E6F0. The model accepts the call-site application point and raw force/wear inputs, then reuses the existing force/moment accumulator. Other mode values skip this branch. Evidence: analyze_function 0x431150, ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5; assembly 0x4313F2..0x431977.

### Timeout-triggered 0x433920 reset after the integrator

After the +0x38C speed check, 0x431150 calls 0x425870. If that byte is zero and +0x48C is nonzero, it samples time and calls 0x433920 only when current time is strictly greater than +0x488 + 2.0. The helper clears +0x48D, eight DWORDs beginning at +0x48E, and +0x48C; it visits five resource groups, clears each wheel damage byte at +0x574 with 0x1A4 stride, then calls the backend virtual at +0x478 slot +4. advance_ncar_post_body_tail models the gate/direct clears, reports the five resource groups and backend call, and propagates the wheel-byte clears. The helper resource-list copying and backend behavior remain opaque. Evidence: analyze_function 0x431150, ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5; 0x433920, ev_342c70e3f58d04a0da275f2f8297b839839ce3c96dfe8b2f7e3d77133c229404.
### Resource-group effects inside the 0x433920 reset

The five reset passes resolve 0x450B40/0x450B20 to descriptor pointer/count pairs and copy the first three DWORDs from each source row at 0x20-byte stride into destination rows, using the destination count. The source count is observed but not used by the binary loop. Each group then calls 0x44F750 with the existing descriptor pointer and flag 4; 0x44F750 forwards to 0x44F9C0, whose flag-4 path writes that pointer back unchanged at +0x10 and clears byte +0x15. The reconstruction exposes five bounded source/destination spans and reports missing rows rather than reading beyond them. Helper evidence: 0x450B40 ev_574f9355bf178d9ccb64573602ce8f0f9fe3363e0e13fbc1f16282fd3ddb64ad; 0x450B20 ev_aa5d3b89792def96f89b7b4aadfe4fcd68a554c2616ec962b2ebd93a0ba5535e; 0x44F750 ev_ae086326d3d80f4eb2d8606a0bf9df800b98782b106605629bb331998281af4c; 0x44F9C0 ev_caf16edf9d5a790cf3c58c81f9d5ff085a19a55dc8656c4c46eb65f3599c2e48.

### Paired wheel differentials before body integration

After the mode-specific force gate, 0x431150 computes (+0x63C - +0x7E0) * +0xDD4 and (+0x984 - +0xB28) * +0xDD8, storing float-rounded results. The first pair is zeroed only when both contact bytes +0x6BC and +0x860 are zero; the second pair is zeroed only when both +0xA04 and +0xBA8 are zero. It writes opposite signs to +0x850/+0x6AC and +0xB98/+0x9F4 respectively. apply_ncar_integrator_wheel_pair_differentials models these calculations from explicit raw-field inputs, using the same four contact observations consumed by the mode-force gate. Evidence: analyze_function 0x431150, ev_4b3139981dceaf6b39dcb6ac3fd8f2d8561e94ba04311112a26b0c9e02b0d1c5; assembly 0x43197C..0x431A08.

### Resistance and velocity forces before body integration

After the paired stores, `0x431150` forms the linear-velocity delta against the prior sample, transforms it to body space, and computes the application point from the stored center plus a transformed offset. With fewer than two contact bytes set, it projects the raw delta onto the three basis axes, computes the square root of the weighted squared projections using `+0x14E/+0x14F/+0x150`, and applies the negative delta scaled by that magnitude. At two or more contacts it applies component-wise quadratic resistance to the transformed delta using `+0x14F/+0x150/+0x14E`, transforms the force back, then accumulates it. Following that call, two more forces combine velocity-projection squared with speed-scaled damping, each with separate scales and application point; their Y components update the per-step `+0xEB4` energy value. The reconstruction models these equations and keeps transform outputs and field scales explicit. Evidence: analyze_function `0x431150`, `ev_fb7a9b21717cfb6308ed0b4ff5c8e3f5b185399be225d0fbbfbde36ee68b1217`; assembly `0x431A08..0x431DDD`. The four attached-object writes and force calls are represented by `apply_ncar_attached_object_force` and composed before body integration. The ordered handoff into `0x43ECE0` is represented by `advance_four_wheel_body_step`, carrying wheel and embedded-record force/moment sums through force collection, callback projections, kinematics, and the post-body tail. Callback effects and exact x87/libm rounding remain open. Fresh dossiers: `0x43ECE0` `ev_a39a0df0b76df8e646f3d5399b4c39e51d6d36c1d3e47868ce2cadb111123100`; embedded callback `0x4473D0` `ev_6fc89f777e9a7b4af954c0e3f3bbd7288303403a67314c57a5f22e80ed65ffbc`; point transform `0x437BA0` `ev_fb622c4d8d185d3011f870a90d485616c3ea8e2e96307278fd5f9619e9025478`.

The conditional block at `0x431DDF..0x431E5B` writes four target projections at each child object's `+0x38` and calls `0x440B40` for each object. The targets are `+0x500*0.08726646+0.034906585`, `0.034906585-+0x500*0.08726646`, `-+0x508*0.12217305`, and zero. `0x440B40` caps its transformed relative-speed denominator at 1, projects normalized velocity onto the selected child basis, clamps `(target-projection)*3.8197186` to [-1,1] then scales by 1.6, and suppresses that slip term when the body-axis/velocity dot product is negative. It adds a capped quadratic resistance term and scales both components by the two child coefficients, speed squared, and 0.645 before accumulating at the child application point. `apply_ncar_attached_object_force` models this helper law; the relative velocity, basis vectors, coefficients, application points, gate and two caller tuning values remain explicit. Evidence: `0x431150`, `ev_fb7a9b21717cfb6308ed0b4ff5c8e3f5b185399be225d0fbbfbde36ee68b1217`; `0x440B40`, `ev_f3d5398d8f7ed0cf5867950367cc314f98bf859790cdc0493869f40636b37ba8`.

### Timed NCAR producer snapshot at `0x43E050`

`NcarSamplePipelineState` now owns the producer snapshot, persistent expanded global record, timed-event queue, callback timer, three-record sample buffer, and derivative/ramp history. `advance_ncar_sample_pipeline` composes the observed strict 1/15-second gate, snapshot, 24-byte packing, expansion into the existing global record (preserving its unwritten `+0x00/+0x10`), enqueue/dequeue, and exact-size ring acceptance. Once an accepted update reaches mode greater than two, it decodes the accepted three-record window and advances the `0x439520` derivative, acceleration, and interpolation-ramp state. Regression checks the transport path, interval gate, event-processor suppression, preserved bytes, and finite dynamics after the fourth accepted sample. The native order within `0x439200` is established: this timed sample/queue prefix, including accepted-sample handling, precedes the later conditional `0x431150` branch in the same callback. Thus the sample captures the object's state before that callback's current integrator call. The live session still does not own the mutable registered body state required by the producer, and the relation between the accepted queue window and the separate `+0xEC4` callback record remains an explicit input; the pipeline is not wired into `WindowState` and does not create gameplay motion. The lookup behavior is confirmed by `0x420930` dossier `ev_33c4dd341cf4668cfab882395344a1216dc9a03679d4cae15b7d16397591e57d`. Degenerate orientation rejection is a portable-model guard; the native routine has no corresponding failure return. The callback's linked-target/dynamics consumers beyond `0x439520` are not part of this composition.

The callback at `0x439200` obtains the physics object, calls `0x43E050` with a time pointer, then passes that captured scratch record into `0x4393B0`. `0x43E050` writes current time at record `+0x04`; body `+0xDE4`, `+0x500`, `+0xC34`, `+0xD64`, and `+0xD70` to record `+0x08..+0x18`; body position `+0x5C..+0x64` to record `+0x1C..+0x24`; normalized-matrix quaternion to record `+0x28..+0x34`; body flag `+0xE28` and one-shot flag `+0xEC0` to `+0x38/+0x39`; and four damage bytes from `+0x574 + i*0x1A4` to `+0x3A..+0x3D`. It clears body `+0xEC0` after capture. Orientation normalization at `0x422C90` rebuilds the basis by normalizing its first vector, normalizing the cross of its original third and first vectors, and deriving the final vector by cross product; the producer then calls the matrix-to-quaternion conversion. `capture_ncar_sample_record` models these scratch-record writes and the orientation/flag side effects. Its regression covers field bytes, untouched leading record data, orthonormalization, one-shot clearing, and no partial mutation on degenerate input. This is the producer scratch record, not the later packed 24-byte payload or the final expanded ring record; in particular, the downstream packing/expansion path has distinct preservation behavior for `+0x10`. Live runtime still lacks the mutable NCAR body integrator state needed to call this helper from gameplay, so no autonomous physics is implied. Evidence: callback `ev_92e0c1eb69aa42416641bb656cd657db97f32e1ef5f18508619044efedad558c`; capture `ev_2280812bbc52a838c46cbdc795957f97830f8121182f5f2e8eed7a921b37b7f1`; basis normalization `ev_7b41c60c024f9b6e54f0b04d6ab723a5bc25b493fd070a223d2d6ef55b8634ac`; packed/expanded path `ev_e29c46085d1e4d91b60df137a84e34b1465e5696d01fa8ac36bfe5a959b78f11`, `ev_2158d715ce59b4079ea402f08416d2e327fdac4a2ef2b9b64ff4c6c9dfce16bb`, and `ev_3b67cac8275fe0a398f9a6b12ff5dc99e762ad20592fb48a23a450b630a8b3f0`.
### Live input to PCAR callback scalar prefix

The multiplier used by the initial `+0xDE0` store is now modeled at its
producer, `0x40D270`, instead of remaining an unexplained float input. The
function returns the float at `0x4C72E4` (observed bytes `00 00 80 3F`, 1.0)
unless byte `0x4D10FC` is nonzero. With that gate set it calls `0x49BE40`,
which returns whether DWORD `0x4E322C` is exactly one; selector one returns
1.0, other values return the float at `0x4ECC60` (bytes `00 00 00 00`, 0.0).
`PcarCallbackMultiplierObservation` keeps the mutable gate and selector
explicit, and `advance_pcar_callback_control_prefix` now computes the
multiplier before writing `+0xDE0`. Evidence: `0x40D270`
`ev_2d15c0a885891fcf248190882242031626ef2f733f06cce618aea2d0b5fdb382`,
`0x49BE40` `ev_3880fbc859089f05482ecfc0c35f2c3f5d00a58b9d157278aff09a5f1b3123d2`,
and constant/global reads `ev_ff88ee3fb2ab77d839288892cb88ff837277257f3d0a49dbc480f08a514bb6a5`,
`ev_4a859c6a98d058ad0d3dca6117e102262a0af3f1a07d05efa8a5f74da5889c6a`,
`ev_7a62919447bcaa6df0a2ab1e88cc38dce5da0b279e7e7946aab438eb6aa1f7ac`,
`ev_fd6b3fe06010d8e36eb070f0c6ca5d6e3dee2a63edc06150fec12e657a0ec757`.
The runtime still does not own these callback globals or use the modeled prefix
to drive body physics.

The live race session now projects the reconstructed `0x43B290` throttle,
brake, clutch-like, steering, and control-state outputs into the selected raw
`PcarCallbackGlobals` fields, then applies the four initial stores observed at
the beginning of `0x4384D0`: `+0xDE0 = 0x5093F4 * result(0x40D270)`,
`+0xDE4 = 0x509320`, `+0xDE8 = 0x5093B8`, and `+0xDEC = clamp(1.1 -
1.2*0x5093C8, 0, 1)`. The model names this a prefix because later callback
branches can overwrite `+0xDE0/+0xDE4`. The runtime currently supplies 1.0
for `result(0x40D270)`, matching the observed startup fallback; profile-driven
settings and conditional PCAR callback branches are not yet integrated. The
four projected values are appended to race traces. `pcar_control_update_regression`
checks the scalar mapping with an explicit multiplier.

### PCAR constructor control settings

The live session now parses the constructor's `CONTROL` options into persistent
PCAR control state and materializes the observed constructor stores into the
byte-backed player body/wheel objects. The binary starts traction control,
ABS, auto shifting, auto clutch, and yaw control at `1, 1, 1, 1, 0` before
reading configuration.
`0x4381E0` forces auto clutch back on when auto shifting is enabled, then copies
the final value to `+0x534`. It stores the game-mode helper result at `+0x4F4`
as unresolved unless supplied. The traction/ABS setters `0x4330B0/0x4330D0`
write each enabled flag as a DWORD into four wheel subobjects at `+0x6B0` and
`+0x6B8` with `0x1A4` stride; the runtime retains those four exact values.
The controls are currently session-owned initialization state; wheel-force
response and automatic transmission behavior still need the corresponding
native update paths before they affect motion. Evidence: `0x4381E0`
`ev_365b74f41f3ffe8a5d0ca349ed3801a7069ccd56fe16be69203abbe524a123f4`,
`0x4330B0` `ev_056007ebb6040c48717f042c74961338ddcb9afe2ac850531fddb6e0e1cb0d7d`,
`0x4330D0` `ev_c98a80c4b55e307bf7ad25561ea0a1a16f2421c8c98d9bb16c18ea59177dfc7d`,
and game-mode accessor `0x425860`
`ev_1c3a9bf935b6573c5a7c6279f49730c305e395a1f150383e8dc332cb85662528`.
The first `0x43BA90` helper result is also stored as a byte at PCAR `+0xD74`.
The accessor `0x425860` returns global `0x508794`; physics-session initializer
`0x425420` copies its incoming session word into that global. The mapping from
the reconstruction's selected mode to this native session word is unresolved.
`0x43BA90` samples control state through global `0x509328` when gate
`0x5093A0` is clear, but that state is not yet mapped to a configured binding.
The model now retains that raw byte only when an observation is supplied;
helper polarity/state and the second helper result's live source are not
reconstructed, so runtime leaves `+0xD74` unknown. Evidence:
`0x4381E0` `ev_365b74f41f3ffe8a5d0ca349ed3801a7069ccd56fe16be69203abbe524a123f4`.
