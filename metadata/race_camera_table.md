# Race camera table

`0x4228C0` loads `camera.tab`. A failed file-open check sets the selector count
at `0x5081DC` to zero. On success, the `camera.tab` section supplies the row
count and each row has seven values at a `0x1C`-byte stride. Column zero is
matched case-insensitively: `fixed` maps to type 0, `pan` to 1, `pan_zoom` to 2,
and `chase` to 3. Any other name reaches the panic reporter with
`Unknown camera type %s`; if the reporter returns, the type word is left as it
was. Columns one through six are converted with `_atof` and stored as floats at
record offsets `+0x04` through `+0x18`.

`load_race_camera_table` projects this loader from observed rows and conversion
results. The rows can be passed directly to `prepare_race_camera_modes_10_or_12_output`.
Mode 10 selects the nearest target candidate among indices 1 through
`selector_count - 1`, using squared distance and a strict comparison; if no
candidate beats its initial threshold, index 1 remains selected. Mode 12 uses
index 0. Both modes then consume the chosen seven-field record. The selected
record index is written to `0x4D488C` and also feeds `0x4203E0`, which stores the
camera position and, for type 0, captures two orientation angles in degrees.
When the target direction is nearly vertical, the fallback x and z components
come from elements 6 and 8 of the selected car's 12-float transform copied from
`+0x38` in the common `0x421120` preamble.

The camera table parser evidence is `0x4228C0`
(`ev_b906c2365d4e529592f7c40ba86ebcf4ace8ed85ee259f884d5eecb2f118b3e8`). The
mode selector and record use evidence from `0x421120`
(`ev_1ae43339ba97ab8e558c634a0d88a8c54eb714fc8881d6681ae7d05fdfa9711c`); the
preset writeback is `0x4203E0`
(`ev_fb2afb0c499dab4157b228aec5abf5e061fe5ed0bed86e7e3f92cf7c48a87ff1`).
Section lookup, row retrieval, and `_atof` results are inputs to the portable
projection. Its span bound prevents out-of-range writes; the binary itself does
not check the parsed row count against the global record capacity.
