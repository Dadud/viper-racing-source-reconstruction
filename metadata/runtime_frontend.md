# Runtime setup window and session loop

The Windows runtime now has a small keyboard-operated race setup window. It
enumerates installed track names in the `common.res` `tracks.tab` table and
keeps rows whose `.trk` bundle exists beside `common.res`; it enumerates
top-level `.car` bundles in the same Win32 file-search order used by the saved
`car_no` loader. `options.def` supplies the initial `GLOBAL.track_no` and
`GLOBAL.car_no` selections. On Enter, the runtime persists selected track/car
resource stems in the separate, versioned `Config/viper-racing-setup.cfg`
file. Version 3 stores the lap count and selected opponent-strength profile
with the track and car; version 1 and version 2 files remain readable. Legacy
files without a profile retain the `GAME.opponent_strength` option. A saved
stem is applied only if its bundle remains in the current inventory; a missing
track or car independently falls back to its
`options.def` selection. Page Up/Down changes the quick-race lap count from 1
to 99; its initial value comes from `GAME.laps`. Original GameData and
`options.def` are not modified.

Use Up/Down to choose a track, Left/Right to choose a car, `[`/`]` to choose
one of the eight Easy, Intermediate, Hard, and Career 1–5 opponent profiles,
and Enter to load the assets and start the existing race session. Press `K`
for the keyboard controls page. Use Up/Down to select any of the thirteen
named actions from `options.def`, Enter to capture a key, or Backspace to
unbind it. Press `R` to restore the binding values from the installed
`options.def`; Escape returns to race setup. All thirteen remaps are saved in
version-5 `Config/viper-racing-setup.cfg` alongside selection preferences;
versions 1–4 remain readable, including their previously saved actions, and
`options.def` is never modified. Only actions already consumed by the live
runtime affect current gameplay. During a
race, press P to
pause or resume. Pause clears held gameplay controls, freezes player/opponent
simulation and race time, and stops the looping engine buffers. Resume resets
the wall-clock and fixed-step anchors so paused time is not replayed as
simulation catch-up. Escape exits the race; on results, Enter returns to setup.
Closing the race window exits the app. The loaded
`GameAssets` value owns resources through the race; returning from the race
destroys it before the next setup selection and reload. CLI-selected car/track,
asset validation, bounded smoke runs, audio smoke, and trace recording bypass
the interactive window. `--play-trace` without explicit track/car overrides
opens setup, then leaves results open after a finished playback so Enter can
return to setup; explicit CLI asset selections retain bounded replay behavior.

`game_frontend_launch_smoke` is a Windows CTest that launches the actual
interactive executable with an isolated temporary Config directory. It checks
career-slot validation and in-memory session opening, verifies the visible
player identity and byte-for-byte native save preservation, deletion
cancellation and confirmation, native career file creation, and refusal to
overwrite valid or invalid occupied files. It
then changes the opponent profile, starts a race, and checks the saved profile.
A second scenario selects Random, verifies that setup resolves it to one of
the eight installed tracks, confirms the concrete track is persisted, and
verifies restoration after relaunch. This exercises those frontend flows and
setup-to-race asset loading; it does not validate career continuation, complete
a race, or establish parity with the original menus.

`game_frontend_session_lifecycle_smoke` opens the setup and controls pages,
remaps throttle, changes the selected car and lap count, starts a Viper/Bemidji
session, and closes the runtime window. A second process verifies that the
saved selection and binding reload. It intentionally does not drive around the
ideal line, complete a lap, enter results, or claim gameplay behavior.

The `--diagnostics-only` runtime mode bypasses the provisional player and AI
route-motion updates while preserving keyboard sampling, recovered PCAR control
projection, fixed-step scheduling, contact observations, rendering and trace
capture. `game_keyboard_binding_smoke` and
`game_fixed_step_trace_replay_regression` use this mode; the latter records and
checks in its frame trace that the route-motion adapter ran zero steps. These checks therefore
exercise input/callback plumbing without treating ideal-line following as a
driving surrogate. They still do not establish physical movement.

The fixed-step driving replay keeps the ordinary race placement branch. Its
`-grid` command-line switch is a distinct native override that forces the
track-basis/default placement instead of normal race placement; use that flag
only when that override itself is under test. Route motion remains disabled in
the driving replay, so its vehicle movement comes from the simulated body and
wheel state.

`game_fixed_step_trace_replay_regression` checks deterministic short trace
capture/playback from a 320-frame authored throttle/coast/steer/brake/coast
schedule and requires those inputs to reach the PCAR control outputs on their
scheduled update windows. It replays twice with route motion disabled and
requires identical frame and tick traces plus zero route-adapter steps. This
validates deterministic input/control propagation, not vehicle movement. It
does not assert route progress, opponent finishes, lap completion, or results.
Route-motion model unit tests remain tests of that prototype adapter only; they
are not game-playability evidence.
Race standings display localized AI driver names from the selected
`GAME.opponent_strength` profile in `english.lng`, aligned through the
single-race entrant's modulo-7 driver slot. If the selected AIDR profile has no
active driver or language key for a slot, standings use its numbered entrant
fallback. This does not imply that the AIDR setups or native AI controller are
connected to opponent motion.

The driving bindings pass through the reconstructed type-1 binding
evaluator. The Win32 adapter converts configured virtual keys to DirectInput
scan-code selectors and records key-down/key-up events in a 256-entry digital
snapshot, then samples the configured `CONTROL.steer_left`, `steer_right`,
`throttle`, and `braking` records through the model's native `> 0.2` active
predicate. Empty options remain unbound. Focus loss and input cancellation
clear the snapshot so a missed key-up cannot leave a driving control latched.
This connects runtime keyboard state to the recovered binding logic. The route
adapter remains provisional and does not establish a gearbox or physical
simulation. The Windows input smoke checks configured W/S/R and other action
signals in recorded input, focus-loss release, and pause/resume of simulation
time and fixed-step dispatch. It does not assert route displacement, speed,
reverse travel, or opponent motion. DirectInput polling and controller support
remain open.
Press C at race setup to open the five-slot career-file screen. The runtime
uses the resolved Config directory and the native `careerN.dat` reader to
show slots 1–5 as empty/invalid or display the stored player-name bytes for a
valid slot. Enter repeats header, payload, XOR, and checksum validation; on
success it opens an in-memory career session retaining the selected slot,
resolved file path, player name from the bounded first 0x10 payload bytes, and
the complete validated 0x674-byte decoded payload. All other payload bytes
remain opaque and are kept verbatim. The window title and career screen identify
the open player. Opening the session does not alter the save file. Press N to
create a native-format career file
in the selected empty slot; creation refuses to overwrite either a valid or
invalid occupied slot. Press D and confirm with Y to delete only the
selected slot; N or Escape cancels, and the row refreshes to empty after
removal. Escape returns to race setup. This screen remains a slot browser and
validator: the original career action loop, event setup, upgrades, and result
progression are not connected, so passing a slot does not launch a quick race
or claim to continue a career. `--config-dir` can point the screen at an
explicit Config directory; if no Config directory can be resolved, the career
hotkey is unavailable.

## Acceptance still open

- Complete a physics-owned race from setup through standings and back to
  setup. Current lifecycle and trace tests cover selection/settings persistence,
  runtime startup/shutdown, and deterministic trace mechanics only. They do not
  drive the ideal line, simulate lap completion, or serve as race evidence.
  Manual gameplay, physical simulation, results, and original parity remain open.
- Verify each track and supported car through this UI, not only the CLI smoke
  matrix; establish the supported car roster against original frontend data.
- Implement and retain additional settings beyond quick-race lap count and
  opponent profile, including race-mode selection and native-compatible
  writeback. Random-track selection is implemented and covered by the frontend
  launch smoke, which checks installed-track resolution, persistence of the
  realized track, and restoration after relaunch.
- Connect validated career payloads to the reconstructed career action/event
  loop, preserving upgrades and progression through results and native saves.
- Compare controlled original and reconstructed menu/race/results captures.
