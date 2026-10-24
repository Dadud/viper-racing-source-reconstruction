# AI note resources

The native loader at `0x41F670` requests ADNT resources using a lookup key made
from four values: the entrant's group value modulo 7, the selected difficulty,
the `tracks.tab` row plus 8 when the track mode byte is set, and the car name.
`0x41F5E0`/`0x41F630` concatenate the first three values as lowercase,
unpadded hexadecimal followed by a normalized car suffix. The caller searches
the `.trk<key>.dnt` track sidecar path; the general `*notes\\<key>.dnt` path is
the alternate form.

`make_ai_note_path` implements the key formatting. The native `0x40DA00` car
normalizer maps `viper` to `vipr`, maps `vipergt` to `vgt`, and otherwise copies
the first four bytes. The ADNT header itself retains the full car string and is
compared case-insensitively.

`parse_ai_note_resource` accepts only resource version 7 with the internal
version 7 and `0x40`-byte header. It parses track index at `+0x08`, entrant
group at `+0x0C`, difficulty at `+0x10`, a bounded car string at `+0x14`, and
the check/count/byte-size fields at `+0x34` through `+0x3C`. It requires the
data size to match the remaining payload and mirrors the native integer
division check `data_bytes / node_count == 8`. It preserves the node payload
raw. `ai_note_matches_lookup_key` checks the four lookup values.

The native loader cross-checks the note against the selected ILIN resource. It
derives a 16-bit checksum from eight bytes at an ILIN-relative offset and a
DWORD line value, then compares them with ADNT header fields. It also compares
a redundant header check; the exact helper semantics for these checks are not
yet reconstructed, so the parser exposes those raw values without claiming
full native acceptance. The new parser is not yet wired into `GameAssets`:
the shipped extraction contains `drivers.res` but no `.dnt` sidecars, and the
legacy game resource-set lookup path still needs integration. A fresh inventory
of the extracted install found no loose `.dnt` or `.ili` files, and all
`*.res` bundles under `GameData` were scanned by their RST0 indices for names
or type tags containing `dnt`, `ili`, `note`, or `adnt`; none matched. This
means the native `default.ili`/ADNT fallback chain cannot be satisfied from the
available extracted install. This was checked against the original installer
media as well: the `DATA1.CAB`, `_USER1.CAB`, and `_SYS1.CAB` cabinets contain
no loose `.ili` or `.dnt` files. Installer-extracted `drivers.res`,
`bemidji.trk`, and `common.res` are byte-identical to the corresponding files
in the installed game-data directory. The installer therefore confirms the
same available resource set; it does not prove that other retail releases or
user installations lack these resources. The `.trk` bundles still contain
their own ILIN data, so this finding is specifically about the separate
`default.ili`/ADNT lookup resources and is not evidence that ideal-line data is
absent.

Evidence from `race.bin` (SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`):

- Lookup and filename construction: `0x41F670` `ev_74a12a0aef0cdbba3851987fba3359d8ee696bd18e126f7df369c897a0513c06`, `0x41F5E0` `ev_129f0960b9e4cacf8ea2f2da0596f0173bb57fe58179ad5a34154d000005019a`, and `0x41F630` `ev_2480f1c8213bc8743fa32ce842b832d5f709b29ce651c14dfa41f934328ccd98`.
- ADNT and ILIN checks: `0x41F6E0` `ev_b18038372d52be1e67cc646151c911659ece05f172a744b3b6f146b437f76eb0`, `0x41F970` `ev_fef06a5aed6f2fd4c282d8cbb6a473bd08fc1ffa28b292f91fc41773795b95a8`, and car normalization `0x40DA00` `ev_c82ddea397c137b80358297b71e3cf10a5f778f652822fe2d840e7df2e479070`.
