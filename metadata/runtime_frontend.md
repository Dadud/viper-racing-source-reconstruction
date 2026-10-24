# Runtime setup window and session loop

The Windows runtime now has a small keyboard-operated race setup window. It
enumerates installed track names in the `common.res` `tracks.tab` table and
keeps rows whose `.trk` bundle exists beside `common.res`; it enumerates
top-level `.car` bundles in the same Win32 file-search order used by the saved
`car_no` loader. `options.def` supplies the initial `GLOBAL.track_no` and
`GLOBAL.car_no` selections. The UI does not write `options.def` yet.

Use Up/Down to choose a track, Left/Right to choose a car, and Enter to load
the assets and start the existing race session. On race results, Enter returns
to setup; Escape exits. Closing the race window exits the app. The loaded
`GameAssets` value owns resources through the race; returning from the race
destroys it before the next setup selection and reload. CLI-selected car/track,
asset validation, bounded smoke runs, audio smoke, and trace recording bypass
the interactive window. `--play-trace` without explicit track/car overrides
opens setup, then leaves results open after a finished playback so Enter can
return to setup; explicit CLI asset selections retain bounded replay behavior.

`game_frontend_launch_smoke` is a Windows CTest that launches the actual
interactive executable, finds the setup window, posts Enter, waits for the race
window, posts Escape, and checks that the process exits with code zero. This
proves setup-to-race asset loading and the early-exit path. It does not yet run
a complete race through results and back to setup, verify every selector row,
persist changed choices, or establish parity with the original menus.

`game_frontend_race_lifecycle_smoke` creates a deterministic one-lap
Viper/Bemidji throttle trace, launches through the real setup window, selects
Viper, plays the full trace, and waits for Enter to return from standings to
setup. It then changes to Sports, launches the second full trace session, and
exits from setup after its results. Replay exhaustion keeps the results window
open only when playback was started through the interactive frontend; explicit
CLI replay still closes when the trace ends. This verifies repeated
setup/race/result resource lifecycles for the route prototype, not physical
simulation or original-game parity.

The four driving bindings now pass through the reconstructed type-1 binding
evaluator. The Win32 adapter converts configured virtual keys to DirectInput
scan-code selectors and records key-down/key-up events in a 256-entry digital
snapshot, then samples the configured `CONTROL.steer_left`, `steer_right`,
`throttle`, and `braking` records through the model's native `> 0.2` active
predicate. Empty options remain unbound. This connects runtime keyboard state
to the recovered binding logic; it does not implement DirectInput polling,
controller support, control rebinding UI, or physical simulation. A Windows
smoke test records throttle while Up is held and confirms it clears after
key-up.

## Acceptance still open

- Run a complete Viper/Bemidji race from the setup window, inspect standings,
  return to setup, choose another track/car, and start the second session.
  The lifecycle smoke now covers two full trace-driven Bemidji sessions,
  Viper-to-Sports selection, asset reload, and result returns, but no manual
  gameplay input.
- Verify each track and supported car through this UI, not only the CLI smoke
  matrix; establish the supported car roster against original frontend data.
- Implement and retain supported settings, control rebinding, random-track
  behavior, mode/difficulty/lap selection, and native-compatible writeback.
- Compare controlled original and reconstructed menu/race/results captures.
