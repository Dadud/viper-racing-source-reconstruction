# AI driver resource

`drivers.res` owns the `aidriver.adr` resource (type `AIDR`, version 2). The
shipped resource has a `0x11020`-byte payload. Native function `0x41B550`
accepts this size for version 2 and initializes the driver manager from it;
`0x41B2E0` lays out eight groups of sixteen `0x220`-byte records and reads a
DWORD count after each group's records. The group stride is `0x2204`, making
the complete current layout eight groups, or `0x11020` bytes. The related
`0x41B340` code uses these groups to create localized driver-name keys for
Easy, Intermediate (the medium profile), Hard, and Career1 through Career5.

`parse_ai_driver_resource` validates the version, current payload size, and
that each group count is at most sixteen. It preserves all sixteen raw slots
per group because the record fields have not been semantically recovered.
`find_ai_driver_record` selects an active slot within a supplied profile group.
`select_ai_driver_track_tuning` mirrors `0x41A000`'s row address calculation:
`0x90 + (track_index + mode * 8) * 0x18`, for the eight `tracks.tab` rows and
two observed modes. It reads binary32 values at row offsets `+0`, `+4`, and
`+0xC`, plus record values at `+0x20` and `+0x6C`. The initializer writes the
product of row `+0` and record `+0x20` to setup word 1, passes row `+4` to
`0x41A0C0`, and scales setup words `0x12` and `0x13` by row `+0xC` times record
`+0x6C`. Field meanings remain unlabeled. Evidence: `0x41A000`
`ev_67a3cffd701fba3131c3793b441c8ae76089194d822832bebb5902ef73ac6b54`.
`GameAssets` now requires and owns this parsed resource alongside its shared,
car, and track bundles. When the selected player car is not Viper, it also
opens and owns `viper.car` as a separate AI vehicle bundle and parses its CARF
without player upgrade overlays. It resolves the selected track's `tracks.tab`
row. At runtime startup, the setup planner is invoked for all eight AIDR
profiles and both observed tuning modes, keyed by each AI entrant's modulo-7
slot. The current profile/mode selector, native fallback block acquisition,
and FDIV helper mode remain unresolved; generated candidates are not yet fed
to opponent vehicle construction. The parser currently does not accept the legacy
`0x660C`-byte version-2 payload that the native loader also recognizes and
converts. Native `0x41B9B0` copies the legacy payload's first three groups and
builds the remaining five groups from the first current runtime-manager record
for each group. The manager table is populated by `0x41BB50` constructors; its
initialized image bytes are zero, so those five records are not a static table
that can safely be copied into the source reconstruction. Supporting legacy
conversion requires reconstructing or providing the same initialized manager
records. Evidence: `0x41B9B0`
`ev_aee9eab2229a10b2a4f337b04c3c89ffe9e4411ce604729a192e7665ea99cf66`,
`0x41BB30` `ev_f576fea3271bfdca92952c71c0fdb89423efd828a5f02ab99120fe532ffba015`,
and `0x41BB50`
`ev_88536434a4e9f02940f233812b02e911b5576593e57f294b8ee4fd05775efd03`.

The Win32 single-race runtime reads `GAME.opponent_strength` as the selected
profile index and resolves the corresponding localized display names from the
English language table using the native format `AIDriverName:<profile>:Driver<n>`.
It only assigns a name when the chosen AIDR group marks that slot active and
the language key exists. The 7 active Easy driver names in the shipped data
therefore populate the 7 AI entrant rows; results use those names instead of
placeholder slot labels. Missing names retain a numbered display fallback.
The setup UI still does not expose difficulty selection, and this name wiring
does not connect AIDR candidate setup records to AI vehicle behavior.

Evidence from `race.bin` (SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`):

- `0x41B550`: `ev_183fa2e38cb18d1bc0e73cea55dd40c5b7eceea2ef11ba2c785cd1af3793c28e`
- `0x41B2E0`: `ev_651ded3da66343924f102182038532ca0820d63bfbee2ee8f43fa9e325deb0a0`
- `0x41B340`: `ev_7734202bfa6baa8fb9a35a2831295c3af2a63205c72758a4f53e45654e4c839d`

The extracted `drivers.res` inventory reports `aidriver.adr`, type `0x41494472`,
attributes/version 2, and 69,664 (`0x11020`) payload bytes. The native initializer
uses a distinct per-entrant group key modulo 7 for ADNT lookup; it is not the
same index as the eight AIDR profile groups. The key also contains the selected
difficulty and track index. See `metadata/ai_notes_resource.md` for the current
ADNT header parser and filename construction. Runtime discovery/loading of those
optional note sidecars and their checksum validation are still incomplete.
