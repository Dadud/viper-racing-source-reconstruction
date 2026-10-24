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
asset validation, bounded smoke runs, audio smoke, and trace record/play modes
bypass the interactive window so automated scenarios remain deterministic.

`game_frontend_launch_smoke` is a Windows CTest that launches the actual
interactive executable, finds the setup window, posts Enter, waits for the race
window, posts Escape, and checks that the process exits with code zero. This
proves setup-to-race asset loading and the early-exit path. It does not yet run
a complete race through results and back to setup, verify every selector row,
persist changed choices, or establish parity with the original menus.

## Acceptance still open

- Run a complete Viper/Bemidji race from the setup window, inspect standings,
  return to setup, choose another track/car, and start the second session.
- Verify each track and supported car through this UI, not only the CLI smoke
  matrix; establish the supported car roster against original frontend data.
- Implement and retain supported settings, control rebinding, random-track
  behavior, mode/difficulty/lap selection, and native-compatible writeback.
- Compare controlled original and reconstructed menu/race/results captures.
