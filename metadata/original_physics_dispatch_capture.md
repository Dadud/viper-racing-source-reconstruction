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

The complete private REA evidence bundle is
`../../analysis/reference_captures/pcar_throttle_body_trace_20261025.evidence.json`
(SHA-256 `8B6BA3F000B313AC5471C13C55525EA3DC3C2B931CDEA5C12A65C3F97F299EBC`).
It contains 18 Evidence records; originals and captures remain outside the
public repository.
