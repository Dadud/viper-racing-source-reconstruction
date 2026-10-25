# Original-versus-reconstruction capture protocol

Use this protocol to validate behavior-sensitive features. It defines what to
record, not expected values or tolerances. No comparison recording is claimed
by this document. Original binaries and assets remain external; do not commit
private captures or payloads to the public source repository.

Current reconstruction evidence includes `viper_game` loading six original
bundles and 822 language entries with `--validate-assets`, plus a three-frame
D3D11 WARP smoke run that exits cleanly. The top-down scene renders every
parsed `obj car` grid position and supports provisional keyboard-driven
kinematic motion for the first mesh; smoke coverage does not replay input.
Record these as startup, asset-validation, and bounded rendering evidence only;
they do not constitute a gameplay comparison or replace the paired playable
scenario below.

A private original-client startup attempt is recorded at
`../../analysis/reference_captures/original_graphics_failure.json`. Its
`-no3dfx` argument is not valid evidence for a graphics workaround: the
recovered command-line parser does not recognize that switch. A controlled
retry with the documented `-1` primary-adapter switch is recorded at
`../../analysis/reference_captures/original_primary_adapter_20261025T022451Z.json`.
The subsequent DirectDraw traces in [graphics_startup.md](graphics_startup.md) establish
that the generic startup dialog was caused by the engine's 32-bit video-memory
classification: the host reported `0xFFFF0000`, addition wrapped the value,
and the engine exited with code 5. In a private, process-only probe, changing
only the first successful `GetAvailableVidMem` total to `0x00200000` selected
memory class 4 and allowed DirectDraw surfaces to be created. With the same
bounded override, controlled clicks reached Quick Race setup, the Bemidji
grid, and the 640x480 3D race at Lap 1/3, Position 8/8, 0 MPH. Screenshots and
the 55-second API trace are retained outside the repository at
`../../analysis/reference_captures/viper-quickrace-frame_03_after_action_2.png`
and `../../analysis/reference_captures/viper-quickrace-trace.json`; the trace
contains drawing activity but no confirmed vehicle movement or accelerator
response. Original binaries, ISO, and GameData hashes matched their baselines
after cleanup. This establishes a controlled race-start observation under a
host-specific video-memory override, not physical behavior or parity. A later
instrumented run captured the original task factory and callback order. A
foregrounded 1.5-second throttle pulse still produced zero sampled throttle;
then a bounded synthetic return-value intervention at the original DIK_UP
key-read wrapper caused the original throttle global to ramp up and decay.
This validates the binding/update path under the intervention but does not
establish ordinary host-input delivery, clean motion causality, or parity. See
[`original_physics_dispatch_capture.md`](original_physics_dispatch_capture.md)
and its private extract at
`../../analysis/reference_captures/physics_dispatch_20261025/runtime_trace_selected.json`
plus the bounded intervention extract at
`../../analysis/reference_captures/physics_dispatch_20261025/synthetic_dik_ramp_capture.json`.
The next comparison should first capture per-update timestamps for a bounded
throttle ramp on the original, then replay that schedule against the
reconstructed input-update path and capture a short, repeatable
input-to-motion interval on both clients. The current callback-ordered samples
do not support a numeric ramp comparison. Race completion and parity remain
unverified in the original capture.

A second controlled launch through the extracted install's own launcher is
recorded in the private manifest
`../../analysis/reference_captures/original_quickrace_relaunch_20261025.manifest.json`.
The capture preserves the launcher's `Data` working directory and changes
only the first successful `GetAvailableVidMem` total from `0xFFFF0000` to
`0x00200000`; all 32 staged external data files and the install's
`options.cfg` match their recorded baselines. It reached the main menu, Quick
Race setup, Bemidji grid and 3D race start with Dodge Viper, Easy AI, Arcade,
Sprint/3 laps, damage enabled and seven AI cars. Private 640x480 frames are
`../../analysis/reference_captures/original_main_menu_relaunch_20261025.png`,
`../../analysis/reference_captures/original_race_start_relaunch_20261025.png`,
`../../analysis/reference_captures/original_race_no_input_7s_20261025.png`,
and `../../analysis/reference_captures/original_race_no_input_2m_20261025.png`.
The no-input follow-up shows Lap Time 0:10.6, Position 8/8 and -1 MPH with the
reset prompt; a later no-input frame at Lap Time 2:04.5 shows Position 8/8,
-4 MPH and the same prompt. These are raw UI observations, not a controlled
motion attribution. This is a startup and no-input baseline only; it does not
verify input delivery, race completion, audio, or reconstruction parity.

### Isolated launcher handoff retry on 2026-10-25

A fresh copy of the staged `GameRoot` (47 files, 70,536,575 bytes) was
launched from its own copied `Data` directory. The copied launcher and data
files matched the source capture copy by SHA-256. A Frida bridge was installed
before resuming the launcher thread and watched `CreateProcessA/W` for a
`race.bin` child with the exact staged working directory. The launcher process
exited after resume without a matching child-creation event; no new engine
process or gameplay capture was produced. The earlier preserved launcher and
the reconstructed game remained running. Details are in the private manifest
`../../analysis/reference_captures/wheel_history_probe_20261025_isolated_attempt1/isolated_launcher_handoff_20261025_attempt1.manifest.json`
(SHA-256
`0E3E5D5DB24833F67E50C3403E55BD1E68A00DFD99947924009523251BC08413`). This
is a failed handoff attempt, not runtime gameplay evidence. Its launcher exit
cause remains unresolved.

### Preserved launcher process check on 2026-10-26

The older preserved launcher process (PID 14636, executable under
`analysis/reference_captures/wheel_history_probe_20261025/GameRoot`, window
title `Our Apologies`) was inspected without changing its state. A read-only
Frida module enumeration found only `Viper Racing.exe` at `0x400000`; it did
not find the original `race.bin` engine module. Therefore this process cannot
provide per-frame gameplay evidence and must not be treated as the engine
capture target. The observation is retained outside Git in
`../../analysis/reference_captures/current_original_module_probe_20261026.evidence.json`
(SHA-256
`C35D6725D1F7FB831BD24D5072337DC5000DA86302EDDEAC5DFCE8B1A1A346CA`, Frida
Evidence `ev_feb945405f7bed8969dda6e8d55e41a1cd647e90e05679ed9121cdff968a77a4`).
This is a module-presence observation only; a new controlled launcher-to-game
handoff is required for additional physics captures.

## Pairing a scenario

Run the original and reconstruction separately on the same Windows machine and
display/audio setup where practical. Record OS/build, executable version or
source revision, display mode and resolution, frame-rate settings, input
device, locale, and any compatibility settings. Identify the exact game-data
directory without copying assets into the source checkout. Record SHA-256 for
the executable/build artifacts, each input data file used, and every capture
output. Keep the manifest with the captures.

Choose the same scenario and initial selections in both programs. Record all
menu/settings values that can affect behavior, track and vehicle identifiers,
opponent configuration, difficulty, race mode, and starting conditions. Note
any option that cannot be matched. Restart from a known state for each run.

## Input schedule and observations

Use a written, timestamped input schedule relative to a defined start event
(for example, race-start display). Record device and control mapping, button or
axis values, press/release times, and holds. Prefer automated/replayable input
when available; otherwise record the operator and method. Capture delivered
input timestamps when tools expose them. Note pauses, focus changes, missed
inputs, and timing drift. Do not silently adjust one run to compensate for the
other.

At defined checkpoints, record observable state and events: race phase,
vehicle position/orientation/speed when available, lap or checkpoint progress,
damage, opponent positions/order, timer, result/rank, menus, and errors. Include
event timestamps and the source of each value (UI, log, debugger, or saved
state). Preserve raw logs/state dumps as well as any normalized extracts.

## Media and persistence

Capture video or timestamped frames with identical display dimensions and
document capture method, scaling, color format, and dropped-frame information.
Retain raw frames; any alignment, crop, or difference image is a derived file
with its transformation recorded. Capture audio output for the same interval
and document sample rate, channel count, encoding, capture path, and any
resampling or gain changes. Keep unprocessed audio and derived comparisons.

For save, career, or replay scenarios, copy the relevant files before and after
each run. Hash the files, record paths and sizes, and use a format-aware
comparison where possible. Preserve raw bytes; report changed fields or byte
ranges without assuming their meaning. Never overwrite the only original save.

## Comparison report

Store a manifest containing scenario ID, paired-run IDs, date/time and
environment, source revision, input data hashes, settings, input schedule,
checkpoints, event log references, frame/audio references, save/replay hashes,
and known deviations. Keep original and reconstruction results side by side.
Report observed deltas and repeatability across reruns. Define tolerances only
after collecting representative original runs and documenting how variability
was measured; this protocol supplies no numeric tolerance. Mark unavailable
measurements explicitly, and distinguish measured results from visual or
operator judgment.

Keep private captures in an access-controlled location with a local manifest
reference. Public CI should use synthetic/free fixtures and must not fetch or
embed original assets. CI can validate parsers, deterministic synthetic
scenarios, and artifact handling; original-game comparison remains a separate
local acceptance activity using the external installation.
