# Shipped career season resources

The four `SEAS` chunks are in `GameData/ui.res`. They were read from the
extracted shipped bundle with the RST0 index/chunk layout documented in
`resource_loading.md`.

| Resource | Payload bytes | DWORDs | Events | Header money values (1–8) |
|---|---:|---:|---:|---|
| `season0.ssn` | 300 | 75 | 3 | `4000, 750, 250, 0, 0, 0, 0, 0` |
| `season1.ssn` | 476 | 119 | 5 | `6000, 2500, 1000, 0, 0, 0, 0, 0` |
| `season2.ssn` | 652 | 163 | 7 | `10000, 5000, 2500, 2000, 1250, 1000, 750, 500` |
| `season3.ssn` | 828 | 207 | 9 | `25000, 10000, 5000, 0, 0, 0, 0, 0` |

All four payloads have exactly `9 + event_count * 22` DWORDs. Word 0 is the
event count, words 1–8 are the header money values, and each event occupies 22
DWORDs beginning at word 9. The `0x4ADB00` lookup reads eight money values at
record offsets 6–13, corresponding to payload words `15 + event * 22` through
`22 + event * 22`.

Observed event money rows:

- `season0.ssn`: all three rows are `500, 300, 200, 150, 100, 75, 50, 25`.
- `season1.ssn`: the first four rows are `1500, 1100, 800, 650, 500, 400,
  250, 100`; the final row is `2500, 2000, 1500, 1000, 750, 500, 250, 100`.
- `season2.ssn`: all seven rows are `4000, 3250, 3000, 2500, 2000, 1500,
  1000, 500`.
- `season3.ssn`: all nine rows are `10000, 7500, 5000, 4000, 3000, 2000,
  1500, 750`.

Track order by season:

- `season0.ssn`: `bemidji`, `heaven`, `limbo`.
- `season1.ssn`: `hastings`, `uptown`, `heaven`, `dundas`, `bemidji`.
- `season2.ssn`: `limbo`, `nfield`, `hastings`, `dundas`, `uptown`,
  `kenyon`, `uptown`.
- `season3.ssn`: `limbo`, `uptown`, `nfield`, `hastings`, `bemidji`,
  `dundas`, `kenyon`, `heaven`, `nfield`.

The first eight event bytes are a NUL-terminated track name; the shipped rows
include `bemidji`, `heaven`, and `limbo`. DWORDs 6–13 of each event record are
placement payouts, which `0x4ADB00` resolves as `Career:Money:<value>` labels
and `0x4AC290` consumes by placement. DWORDs 14–21 are placement points, read
by `0x4AA5E0` for the standings update. Every shipped event row uses
`20, 15, 10, 6, 4, 3, 2, 1` points. `0x4AA5E0` also reads the low byte at
record offset `0x10` and the DWORD at offset `0x14`, then copies them into its
race context. The decoder exposes these as `field_0x10_low_byte` and
`field_0x14` without assigning semantics. DWORDs 2–3 have no direct
field-specific read in the inspected function body; their higher-level role
remains unknown.

Observed values by event order:

- `season0.ssn`: offset `0x10` bytes `0, 0, 0`; offset `0x14` DWORDs `4, 2, 3`.
- `season1.ssn`: offset `0x10` bytes `0, 0, 1, 0, 1`; offset `0x14` DWORDs
  `2, 5, 3, 3, 8`.
- `season2.ssn`: offset `0x10` bytes `1, 0, 0, 1, 0, 0, 1`; offset `0x14`
  DWORDs `8, 3, 4, 4, 8, 3, 8`.
- `season3.ssn`: offset `0x10` bytes `0, 0, 0, 0, 0, 0, 0, 0, 1`; offset
  `0x14` DWORDs `12, 12, 5, 8, 16, 8, 8, 8, 8`.

`decode_career_season_data` in `include/viper/career_season.hpp` preserves each
complete 22-DWORD event row and exposes the track name, payouts, and points.
The ranking projection can be populated from a decoded event with
`make_career_event_ranking_observation`; it still takes the runtime entrant
order and result rows as observations.
