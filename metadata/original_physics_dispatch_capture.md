# Original physics task order and dispatch capture

This is a bounded runtime observation from the shipped original client, not a
reconstruction gameplay comparison. The private capture is outside the source
repository at
`../../analysis/reference_captures/physics_dispatch_20261025/`; its selected
trace extract is `runtime_trace_selected.json`. The REA capture Evidence ID is
`ev_236967dcd176d9064393dca8dc656341efd711883f98b7e433dec7abb7cbc23e`.

## Scenario and instrumentation

- Original `race.bin`, SHA-256
  `F9D6BD3EE1B9E8A7E101BF4CC3C4712969176255111BCF76A85D82568096DA56`.
- Quick Race, Dodge Viper, Bemidji, Easy, Arcade, three laps, player grid slot
  8. The screenshots are 640×480 PNG captures of the desktop screen.
- The process-only probe changed only the first successful
  `GetAvailableVidMem` total (`0xFFFF0000` to `0x00200000`) to pass the
  host-specific DirectDraw startup failure. Hook locations and script hashes
  are in the private manifest. Treat the run as instrumented.

## Factory order observed

At `0x420480` task-list loading, hook `0x422580` observed eleven successful
factory allocations. The post-load task-array snapshot matched their returned
pointers, vtables, and stream cursors.

| Index | Logical tag | Declared record size | Stream cursor | Runtime object | Vtable |
|---:|---|---:|---:|---|---|
| 0 | ACAR | `0x1E4` | `0x000` | `0x4C77604` | `0x4C7B78` |
| 1 | ACAR | `0x1E4` | `0x18C` | `0x4C7D75C` | `0x4C7B78` |
| 2 | ACAR | `0x1E4` | `0x318` | `0x4C838B4` | `0x4C7B78` |
| 3 | ACAR | `0x1E4` | `0x4A4` | `0x4C89A64` | `0x4C7B78` |
| 4 | ACAR | `0x1E4` | `0x630` | `0x4C8FC14` | `0x4C7B78` |
| 5 | ACAR | `0x1E4` | `0x7BC` | `0x4C95DC4` | `0x4C7B78` |
| 6 | ACAR | `0x1E4` | `0x948` | `0x4C9BF74` | `0x4C7B78` |
| 7 | PCAR | `0x1E4` | `0xAD4` | `0x4CA20CC` | `0x4C8188` |
| 8 | CHKP | `0x20` | `0xC60` | `0x4CA795C` | `0x4C82A0` |
| 9 | CHKP | `0x20` | `0xC94` | `0x4CA79EC` | `0x4C82A0` |
| 10 | CHKP | `0x20` | `0xCC8` | `0x4CA7A7C` | `0x4C82A0` |

The cursor advances observed from factory arguments are `0x18C` for ACAR and
PCAR and `0x34` for CHKP. CHKP's observed `0x20` declared size is therefore
not its task-stream cursor advance. The instrumentation formats raw little-
endian FourCC bytes in reverse display order; the table uses logical tags
decoded from the numeric DWORDs.

## Callback observations and limits

The dispatcher and PCAR callback hooks both reached their sampled count of
20,640. Calls into shared integrator `0x431150` exposed all eight ACAR/PCAR
object pointers in the sampled event history; the periodic integrator log
reached 165,720. CHKP pointers were not among those integrator objects. This
confirms task creation order and callback activity for this run. It does not
recover the missing wheel-history, force, timer, or per-step gate inputs needed
to connect the production race loop to the reconstructed rigid-body step.

A foregrounded `VK_UP` pulse was sent for 1.5 seconds after clearing the
on-screen reset prompt with Space. The high-frequency samples at the PCAR
callback showed zero steering, throttle, and brake globals, and no DIK_UP
high bit at the sampled `0x4EF2C8` byte. The saved post-pulse frame still showed
0 MPH. That pulse did not establish original throttle response. A separate
earlier pulse ended while the reset prompt was displayed and is not a valid
control comparison. The sampler's `nativeTick` field was read as a float from a
time-related address and must be ignored.

## Bounded synthetic key-read intervention

To separate host-key injection from the original input/update path, a second
probe temporarily changed only the return value of the original key-read
wrapper at `0x413170` for selector `DIK_UP` (`0xC8`). During a 1.5-second
window, the wrapper's original zero return was replaced with one on 49 of 101
matching reads; all other selectors and the original update/callback code ran
unchanged. This was a synthetic intervention, not physical keyboard delivery.
The private trace extract is
`../../analysis/reference_captures/physics_dispatch_20261025/synthetic_dik_ramp_capture.json`
and the probe script SHA-256 is
`DB5E3C52E740D8F32842EA713A96366E9DA6E18B5A7E575AC926CD31328D82C4`. REA
Evidence ID: `ev_492e1d41c4041bb2979c3e8b9e2e62458c90b5f19d664b29cef755f9a8acdd86`.

The captured JSON timestamps the intervention bounds and sparse wrapper reads,
but its throttle-output samples are ordered by callback/message index rather
than timestamped per update. It is therefore not a frame-accurate input
schedule for a numeric replay comparison.

The original control update then produced a nonzero throttle global at
`0x5093F4`: `0.31662598`, `0.63345343`, `0.95028085`, then `1.0`; after the
synthetic key read ended, it decayed through `0.9472290`, `0.6831726`,
`0.3663452`, and `0.0495178` to zero. Nineteen nonzero PCAR input samples were
retained. A later screenshot showed 8 MPH, lap 1/3, position 8/8. This confirms
that the original key-read binding and downstream throttle ramp are active
under the controlled return-value intervention. The movement is temporally
associated, but this long-running race trace does not prove a clean causal
motion comparison or parity with the reconstruction. The screenshot SHA-256
is `4D1DD24D77FD0413F0E5242AA3796C9B02BBE6BB4DD66CF44D5647D041CD965F`.

PCAR position and velocity changed during the session while those sampled
control globals were zero. Their change is recorded in the private extract but
is not attributed to the ordinary host-key pulse. The synthetic intervention
does show the original throttle ramp, but clean input-to-motion attribution,
race completion, and original-versus-reconstruction parity remain unverified.

## Timestamped throttle and PCAR state sample

A follow-up bounded Frida capture attached to the same running `race.bin`
process (PID 25904; executable SHA-256 above). It overrode the return value of
`0x413170` only for DIK_UP (`0xC8`) reads between 500 and 1500 ms after attach.
Thirty-eight of 130 matching reads were overridden. The `0x43B290` update
returned 130 times and the PCAR callback `0x4384D0` was entered 212 times.
Samples taken after `0x43B290` returned recorded the shared throttle/brake
globals and PCAR body position/linear velocity together at roughly 32 ms
intervals. Evidence ID: `ev_7f4476f48b1c4b39a977284c3db68a040c1052e042635759a30fb7e4f9775524`.

The throttle global `0x5093F4` rose from zero to one during the intervention
and decayed to zero after it ended; brake `0x509320` stayed zero. In the same
samples, the observed PCAR's Z velocity changed from approximately `-0.36` at
531 ms to `-5.73` at 1495 ms, then reached `-7.97` at 1917 ms and was
approximately `-6.59` at 3349 ms. Position and velocity continued changing
before and after the throttle pulse, so this bounded run establishes a
timestamped control-update/body-state co-observation, not a force law or clean
input-to-motion causal comparison. The key byte itself remained zero; the
intervention changed only the selected wrapper's return value. Do not infer
throttle-to-force, engine, gearbox, or tire behavior from this capture alone.

The current private force-capture bundle is
`../../analysis/reference_captures/pcar_throttle_body_force_capture_20261025.evidence.json`
(SHA-256 `D09D20212312BDD0753EB76C420FD4D50CCB15AEB79A3BD51C425CD51CD970B9`).
It contains the two retained Frida records for the force-boundary follow-up;
originals and captures remain outside the public repository.

## PCAR force accumulator at the body-integrator boundary

A second bounded capture selected the live PCAR pointer from callback
`0x4384D0`, then sampled that object's `0x43ECE0` entry and return around the
same synthetic DIK_UP intervention. Thirty-eight of 129 matching key reads
were overridden from 500 to 1500 ms. The hook observed 212 PCAR body-integrator
calls in 3.4 seconds (1696 calls across all cars). At each sampled PCAR call,
it recorded throttle/brake, body position and velocity, and the force/moment
accumulators at `+0x24C/+0x258` before and after `0x43ECE0`. Evidence ID:
`ev_7c1d90d1eff4e08618672c18cc0b500cab5a37c62a46cd0554764c37b6800af7`.

At 489 ms, throttle was zero, force was approximately `(1124.45, 16000.35,
314.43)`, and PCAR linear velocity was `(-4.0651, -0.0001, -1.0824)`. At
809 ms, throttle was one, force was `(-11525.45, 16028.58, -3093.53)`, and
velocity was `(-4.9411, -0.0009, -1.3147)`. At 1497 ms, throttle remained one
and velocity was `(-10.1962, -0.0017, -2.7066)`. After the pulse ended, throttle
decayed while position and velocity kept changing. The aggregate force and
moment accumulators were zero after `0x43ECE0` returned in the sampled calls,
matching the model's consume-and-clear boundary.

This is direct evidence that the body integrator consumes changing aggregate
force/moment state during the controlled throttle ramp. Static call-flow
evidence places four `0x442530` wheel updates before `0x43ECE0`. The capture
does not attribute individual force components to a specific wheel or prove
that throttle alone caused the observed force and motion; the car was already
moving, and terrain/collision inputs were not held constant. The native
per-wheel force producers and their complete runtime inputs remain the next
seam to trace; do not use these values as a throttle-to-force lookup table.

## Follow-up force-writer probe

A later passive probe hooked `0x43E6F0` and observed 58 selected-PCAR accumulator
updates over about 3.1 seconds. The sampled force vector was passed through
`arg0`; `arg1` was the point argument, and the body force/moment bytes changed
across the helper call. Evidence ID:
`ev_508defee4d9d7bb7cd6ca513958ff9ca00c4048824e7ba5aa9befc3ffa19f2f0`.

The probe's return address was consistently `0x43113F`, inside the forwarding
wrapper at `0x431130` that calls `0x43E6F0`. That return address alone did not
identify which wheel update was active. The probe's `+0x554` wheel label came
from stale context and cannot be used for attribution. A later scoped capture
with an explicit `0x442530` entry/return lifetime resolved this ambiguity;
the corrected result is below. All 58 samples in this first probe had throttle
zero and the run did not inject input.

The private probe bundle is
`../../analysis/reference_captures/pcar_force_writer_probe_20261025.evidence.json`
(SHA-256 `AF041F571D7CBAD131DF1AF2B4A24547A2A5FBC0AFDB1836F744B16F6CD75A43`).

## Corrected wheel-scoped force callsites

A bounded read-only probe attached to the verified ISO `race.bin` (PID 16852,
SHA-256 `F9D6BD3EE1B9E8A7E101BF4CC3C4712969176255111BCF76A85D82568096DA56`).
It selected the player PCAR through callback `0x4384D0`, counted entry/return
of each `0x442530` call, and sampled the `0x43E6F0` force vector, application
point, body center, and accumulator bytes while that wheel call was active.
The probe observed 219 selected PCAR callbacks/integrators, 876 wheel updates
(four per integrator), and 4,161 force-writer calls over 3.5 seconds. Evidence
IDs: callsite/slot capture `ev_7b5684967fcf4e1801c54934781e1156526d85e94b8dd30de9b9b83ec1875bac`;
centered input/output capture `ev_df5f1c10472e7957072f1bd7df720c6dfd3518148b2f7d7da013687f2f6cb72a`.

The first selected integrator shows two nonzero `0x43E6F0` writes per wheel:
the wrapper return `0x43113F` (whose nested stack includes wheel callsite
`0x4430E9`) and direct return `0x443C09`. The wheel pointer at each sampled
call is the corresponding PCAR subobject at `+0x554 + 0x1A4 * wheel_index`.
Calls returning to `0x443CBC` and `0x443E6C` had zero vectors in this sample
and left both accumulators unchanged. After the fourth wheel updater returned,
separate writes at `0x431C2D`, `0x431D12`, and `0x431DBC` were observed with
no active wheel context; those remain integrator-level contributions. This
distinguishes wheel-scoped contributions from later body-level additions and
corrects the earlier return-address-only interpretation.

The centered trace bundle remains outside the public repository at
`../../analysis/reference_captures/viper_wheel_force_centered_callsites_20261025.json`
(SHA-256 `8487BE6ACC43F8D86723CA293E41F39821D11B7240AC0BE1CB2E3BA091577E27`).
The first-pass callsite bundle is
`../../analysis/reference_captures/viper_wheel_force_callsites_20261025.json`
(SHA-256 `AA08122BD2C7B3BFDCF2A8F5D05E9CFCAACE746A6F2BE6BA5C08F2063368981A`).
The reusable bounded probe is [`tools/pcar_wheel_force_callsite_probe.js`](../tools/pcar_wheel_force_callsite_probe.js).

An asset-free CTest fixture replays eight nonzero observed force writes through
`accumulate_ncar_force_moment` and checks the captured force/moment accumulator
outputs within 0.02 units; a captured zero-vector call also checks the native
no-write gate. This validates the shared accumulator primitive and the
captured wheel-call inputs. It does not reconstruct how the tire/suspension
response vectors were produced, supply those inputs to the live reconstructed
session, or prove player-physics parity. Exact upstream wheel response inputs,
callback state, collision/query state, and integration into `WindowState`
remain open.

## Bounded shared-integrator object probe

Prepared [`tools/pcar_integrator_object_probe.js`](../tools/pcar_integrator_object_probe.js)
as a read-only next probe. It hooks the existing `0x431150` entry, selects the
PCAR vtable `0x4C8188`, and snapshots the factory-declared `0xEF0`-byte PCAR
allocation before and after every 16th selected-PCAR call, capped at 64 sample
pairs. The factory allocation bound comes from `0x422580`; it covers the four
wheel subobjects (`+0x554 + 0x1A4*i`) and known body fields through `+0xECC`.
Each record includes the object pointer, vtable, call index, phase, wall-clock
timestamp, byte length, and a full snapshot encoded as a JSON `rawHex` field.
No binary side channel is required; decode a snapshot with Node's
`Buffer.from(rawHex, "hex")`. The hook does not change target memory or inputs.
The RPC status reports total calls, selected calls, attempted samples,
successful before/after pairs, and read failures. The ready event includes the
module path/size and expected executable SHA-256; it does not hash loaded
memory, so the caller must verify the on-disk executable before launch.

Use it only in the launcher-owned child flow above, after confirming the
executable, staged Data files, and `options.cfg` hashes. Install it while the
child is suspended, resume through the launcher bridge, then retain a quiet
grid baseline, a short controlled throttle pulse, and a coast interval. Pair
the snapshots with the existing PCAR control/body and `0x43ECE0` force-boundary
hooks. Compare same-object before/after bytes and annotate four wheel bases at
`+0x554 + 0x1A4*i`. The probe is syntax-checked only and has not been run; it
provides no live-state result yet. Internal field meanings, per-wheel force
attribution, and a complete physics input mapping remain unverified.
