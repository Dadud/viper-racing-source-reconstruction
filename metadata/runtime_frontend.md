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
interactive executable with an isolated temporary Config directory, finds the
setup window, posts C to open career slots and Escape to return, then posts
Enter, waits for the race window, posts Escape, and checks that the process
exits with code zero. This exercises slot-screen navigation and setup-to-race
asset loading. It does not validate a real saved career, run a complete race,
verify every selector row, persist changed choices, or establish parity with
the original menus.

`game_frontend_race_lifecycle_smoke` creates a deterministic two-lap throttle
trace, launches through the real setup window, selects Viper and two laps, and
completes Viper/Bemidji. It returns from standings to setup, changes the car to
Sports, then completes Sports/Bemidji. It next wraps the installed track
selector from Bemidji to Dundas and completes Sports/Dundas. The same
trace-driven setup/result lifecycle now covers Hastings, Heaven, Kenyon, Limbo,
Nfield, and Uptown with each selected through its isolated saved setup
preference. Replay exhaustion keeps the results window open only when playback
was started through the interactive frontend; explicit CLI replay still closes
when the trace ends. The smoke relaunches against the same Config directory and
verifies the Sports/Dundas pair and two-lap setting load into the race. This
verifies all-track setup/race/result resource lifecycles for the route
prototype, not physical simulation or original-game parity.

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
This connects runtime keyboard state to the recovered binding logic. In the
route-motion prototype, the separate Reverse binding requests backward route
motion; braking reduces speed magnitude and no longer drives the car backward.
This control mapping remains provisional kinematics, not a gearbox or physical
simulation. A Windows smoke test starts with temporary W/S Config remaps, records throttle while W is held,
confirms the control clears when focus is lost, and checks that S brakes while R
produces backward route motion. It also pauses while throttle is held and
asserts that race time, player route state, opponent route state, and fixed-step
count remain constant until resume. DirectInput polling and controller support
remain open.

Press C at race setup to open the five-slot career-file screen. The runtime
uses the resolved Config directory and the native `careerN.dat` reader to
show slots 1–5 as empty/invalid or display the stored player-name bytes for a
valid slot. Enter repeats header, payload, XOR, and checksum validation and
reports whether the slot passed. Press D and confirm with Y to delete only the
selected slot; N or Escape cancels, and the row refreshes to empty after
removal. Escape returns to race setup. This screen remains a slot browser and
validator: the original career action loop, event setup, upgrades, and result
progression are not connected, so passing a slot does not launch a quick race
or claim to continue a career. `--config-dir` can point the screen at an
explicit Config directory; if no Config directory can be resolved, the career
hotkey is unavailable.

## Acceptance still open

- Run a complete race from the setup window, inspect standings, return to
  setup, choose another track/car, and start the next session. The lifecycle
  smoke now reaches results for two-lap trace-driven prototype sessions on all
  eight tracks, with selector changes, asset reloads, results returns, and
  persisted track/car/lap settings on representative sessions. Manual gameplay
  input, physical simulation, and original parity remain open.
- Verify each track and supported car through this UI, not only the CLI smoke
  matrix; establish the supported car roster against original frontend data.
- Implement and retain additional settings beyond quick-race lap count and
  opponent profile, including random-track behavior,
  race-mode selection, and native-compatible writeback.
- Connect validated career payloads to the reconstructed career action/event
  loop, preserving upgrades and progression through results and native saves.
- Compare controlled original and reconstructed menu/race/results captures.
