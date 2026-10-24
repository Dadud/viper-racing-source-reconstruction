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
`GameAssets` now requires and owns this parsed resource alongside its shared,
car, and track bundles. The parser currently does not accept the legacy
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
