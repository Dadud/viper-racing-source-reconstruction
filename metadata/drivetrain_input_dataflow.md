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

Continue from the call sites that update the PCAR's drivetrain-related state
outside this bounded callback/integrator/wheel slice. Trace any engine RPM,
gear selection, clutch engagement, differential allocation, and the update
that supplies the active contact response at `0x443E9D`. Establish object
ownership and caller identity before adapting it into the C++20 session. No
direct throttle-to-force formula should be introduced from the current traces.
