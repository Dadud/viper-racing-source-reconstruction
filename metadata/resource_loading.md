# RST0 resource bundle loading

Target executable: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
The bundled data used for format inspection remains outside this source repo.

## Loader path

- `0x417740` acquires a named resource set through `0x417430` and reports an
  error if it cannot be loaded.
- `0x417430` opens the set file and calls `0x417550` to parse it. Existing sets
  are reference-counted rather than loaded twice.
- `0x417550` reads a 16-byte header, checks the little-endian signature value
  `0x52535430` (`RST0` in the code's FourCC convention), reads the index and
  resource blocks, and records a file-mapped boundary.
- Each index entry is 36 bytes. `0x417CB0` compares its type code and
  case-insensitive name, returns the payload pointer after an 8-byte chunk
  header, exposes the stored payload size/attributes, and updates usage state.
- `0x435AF0` requests a resource using type code `0x54495245` (`TIRE`) and
  escalates a missing result. `0x441C10` requests a named `.tir` resource and
  uses `def_tire.tir` when the requested resource is unavailable.

## File layout recovered from the loader and shipped bundles

The verified RST0 layout is:

| Offset | Field |
|---:|---|
| `0x00` | Signature DWORD, `0x52535430` |
| `0x04` | Index entry count |
| `0x08` | Number of initial resource chunks read into the temporary buffer |
| `0x0C` | Absolute file offset where file-mapped resource chunks begin |
| `0x10` | 36-byte index records, followed by resource chunks |

Each 36-byte record contains a 16-byte name, a DWORD resource type, a DWORD
attribute value, a DWORD payload length, and two DWORDs which the current
reconstruction leaves uninterpreted. Resource chunks occur in index order;
each has an 8-byte header followed by the payload length in the record. The
first resource chunks are read into memory and later ones are accessed through
the file mapping. The portable reader stores the 8-byte header and exposes the
payload without interpreting that header.

This layout and the computed mapped boundary agree with the extracted
`common.res`, `ui.res`, `drivers.res`, `race.res`, `postrace.res`, four career
bundles, `paintkit.res`, and all five `.car` bundles. For example,
`common.res` has 41 entries, 40 initially buffered chunks, and its TIRE record
`def_tire.tir` has a 120-byte payload. The `.car` bundles use the same RST0
container, including TIRE entries in `exotic.car`, `plane.car`, `sedan.car`,
and `sports.car`; `viper.car` has no TIRE entry and is covered by the shared
default `def_tire.tir` resource.

The reader is implemented in `include/viper/resource_bundle.hpp` and
`src/resource_bundle.cpp`. It validates the index extent, each chunk extent,
the mapped boundary, and the final file length. A separate fixed-size TIRE
decoder reads the 32-byte label and 22 float fields described in
`metadata/vehicle_setup.md`; other payload types remain opaque. `0x417EF0`
checks a separate `RES0` header and is not the RST0 bundle parser described
here.

## STMP stamp and font payloads

`0x44CFD0` requests resource type `STMP` through `0x417CB0`. On a returned
resource, it checks that the loader's metadata output equals 3. When the
loader's relocation flag is set, it treats payload offset `0x81C` as a dword
pointer table. The entry count is the target's signed 32-bit product of the
raw dwords at `+0x04` and `+0x10`; embedded data begins immediately after that
many dwords. Each table value is adjusted by adding the embedded-data base.
The parser in `parse_stamp_payload` preserves those raw factors, table entries,
relative payload offsets, and conditional fixup outcome while refusing to
read a truncated header or table. It also reports pointer targets outside the
payload rather than dereferencing them. It does not impose the original
runtime loader's unchecked-memory behavior on offline files.

The shipped `common.res` (SHA-256
`6d2100dc113914d08e9a2e0369a93914ec16fdccf1fa05f2a79df83cdb2bce82`)
provides concrete examples: `euro6.fnt` has payload size 17,884, factors 8 and
222, 1,776 entries, and embedded-data offset 9,180; `cross.stp` has size 2,207,
factors 16 and 1, 16 entries, and offset 2,140. Both have RST0 attributes 3,
complete tables, and no out-of-payload targets after projected fixups. The
`--inspect-stamp BUNDLE RESOURCE` command prints these fields. Evidence for the
consumer logic: `0x44CFD0`
(`ev_dc4e4553c2b95266ca3c76190a77d6371abee5ab9520656220facd6f817484b1`);
the shared lookup is already modeled under `0x417CB0` above.

For fonts, `0x44DA00` allocates a `0x608`-byte lookup object, initializes 256
character slots, then reads the signed glyph-record count at the STMP payload's
`+0x10` and DWORD records beginning at `+0x414`. Each encoded record selects a
character from its low byte, stores the loop index in that character's
16-bit glyph slot, stores the signed sum of bytes 1 through 3 in the field at
object `+0x204`, and stores signed byte 1 at `+0x404`. Duplicate character
codes overwrite earlier entries, matching the loop. The portable
`parse_stamp_font_index` reconstructs those three tables while retaining the
two metrics as raw fields. `--inspect-font BUNDLE RESOURCE` prints populated
rows. Evidence: `0x44DA00`
(`ev_4d95bea32a566762f91f581a174b2b9c3919d92416e39659afdfe44913a3bbee`),
`0x44D470` (`ev_9c87753a7b96dd2eab7167774892dd8c6877748cc1df5c8cabe67c4f1dd8917f`),
and `0x44D4B0` (`ev_257c3693e424fe89f3867434404a21c4e59f14182207e0decf39e99206b74e4d`).

`plan_resource_file_load` projects the file resolver in `0x417EF0`. A `~` name
is opened once beneath the game-root prefix after removing its marker; the
format bytes at `0x4D3784` are `%s%s`. Other names are concatenated to the two
global resource-path prefixes and tried in order. The plan can construct these
candidate paths when the prefix strings are supplied. The resolver reads
12 header bytes and accepts magic `RES0` (`0x52455330`). It then allocates a
`0x3c` resource node and a `0x24` entry, copies the basename and appends `*`,
stores the two remaining header words, and allocates/reads the rest of the file.
The entry payload length is file size minus `0x14`; the data buffer and second
read are file size minus `0x0c`, since the first 12 bytes were already read.
Successful reads link the node at `DAT_004D35A0`; failed data reads release the
three allocations, and opened handles are closed with `0x411500`. Global path
contents and file, allocator, and read outcomes remain observations. The model
flags a basename plus terminator that exceeds the observed `0x24` entry
allocation rather than writing it into a portable buffer. Evidence for
`0x417EF0`: `ev_88e35c1dea12881f52f3f76daee5da30ca7df02c8f218cd0bd0701ccca4beb92`;
the `%s%s` format bytes: `ev_a6be7006dfff7191cdf99cad3e62f85bca695798c55b7c3d190b3ed9ad5dcf59`.

## Runtime `ResourceGet` lookup

`plan_resource_get` models the side effects of `0x417CB0` from observed lookup
results. Names beginning with `*` search the local loaded set after
normalization; names beginning with `~` skip that search. Ordinary names search
loaded bundles first and fall through to `0x417EF0` when no entry matches. The
projection includes lock acquisition `0x4151A0`, name parsing `0x417B70`, and
the matching unlock `0x4151F0`. A
found entry must have the requested type; otherwise the routine calls panic
helper `0x411000` and may emit the guarded missing-resource message through
`0x410EA0` using string `0x4D3760`.

On a type match, the plan records the used bit (`attributes | 0x80000000`),
first-use state, payload length (`attributes & 0x7fffffff`), owner pointer, and
the returned payload address eight bytes after the chunk header. It increments
the reference count when it was zero or the first DWORD in the chunk header is
nonzero. Successful lookup, misses, and type mismatches clear the one-shot
loading gate and release the lock through `0x4151F0`; missing-resource logging
is suppressed when the gate was already set. Inputs describe bundle/resolver
results, so this plan does not perform runtime bundle I/O. Evidence: `0x417CB0`
(`ev_5e5a3a07556c51f28c9a1904559c37d5d9651c16a4c2dd9293af1ed04f1cad7b`),
`0x4179F0` (`ev_9ee54b3d2fb25a4cc4546de6973925714017f3064c6ab9abb1c5468a3819b183`),
and `0x417EE0` (`ev_5744568add5768e4359a29d5e0c503e96ad688653583c688bb05553ef7221d65`).

## File mapping fallback

`plan_resource_mapping` projects `0x411BE0`. The normal path creates a
read-only file mapping (protection `2`) and maps it with access `4`. If mapping
creation or view mapping fails, the loader logs the corresponding error and
calls `0x411B10`, which reopens the stored path for the handle slot, reads its
entire file into allocated memory, closes that handle, and reports
“Created fake memory map”. The helper ignores the fallback read result. A
missing fallback file calls panic helper `0x411000` with string `0x4D1C3C` and
returns a null base with the fallback ownership flag set. The projection keeps
the CreateFileMappingA, MapViewOfFile, fallback open, allocation, and read
outcomes as inputs; it does not perform OS I/O. Evidence: `0x411BE0`
(`ev_82aeff87c78227f70449855021a17aa75053a7e818c3ef815fe869427746dbc7`) and
`0x411B10` (`ev_08e233805ab7e0c509a149f5c964ac451e7c51275d887aacfcf11be8d5a7ccff`).

## Runtime RST0 load projection

`plan_rst0_bundle_load` projects `0x417550` after the set file is opened. The
loader reads 16 header bytes, gates only on the `RST0` signature, allocates a
`0x3c` set node and a buffer of `mapped_file_offset - 0x10` bytes, then issues
the initial-region read without checking its return value. It obtains the
mapping/fallback tuple from `0x411BE0` and walks the 36-byte index records.
Chunk headers before `buffered_chunk_count` are projected into the initial
buffer; later headers use mapped file offsets. For the initial chunks, the
loader overwrites the first chunk-header DWORD with the index-record pointer.
The model records each chunk's file offset, buffer-relative offset where
applicable, payload length, and header mutation. Counts, payload lengths,
allocation outcomes, and mapping outcomes are inputs. It reports unchecked
count/boundary combinations instead of imposing the stricter validation used by
the portable offline parser. Evidence for `0x417550`:
`ev_aaa4f2d3f975613baf65ceff82c4c8015dac5da03762aaebf211f26bcf581183`.

`plan_resource_set_acquire` composes the surrounding `0x417430` lifecycle. It
checks the loaded-set list case-insensitively; a hit increments the node's
reference count at `+0x28` and returns without opening a file. A miss tries the
two global resource prefixes, invokes the RST0 loader, closes the opened handle
when the header is rejected, and retains that handle on successful load. New
nodes begin with reference count 1. Prefix strings, open results, the handle
slot, and loader inputs are explicit observations. Evidence:
`0x417430` (`ev_fad4d024c643d67e86345e24f637ffe868bc3ef34eae7508981899247b624f90`).

`plan_resource_set_request` wraps acquisition in `0x417740`'s lock at
`DAT_004D359C`, missing-set panic, and unlock. Resource payload releases are
projected by `plan_resource_release` for `0x418160`: it obtains the owning index
entry from the first DWORD of the chunk header at `payload - 8`, decrements that
entry's reference count, and reports an underflow panic if the observed count
was already zero. A non-wildcard entry returns whether the count reached zero.
An entry name ending in `*` is owned by a dynamic `RES0` set; when its count
reaches zero, `0x4181E0` finds and unlinks the owner, then `0x418230` frees the
resource data, entry descriptor, and set node. A missing wildcard owner calls
the panic helper. `0x417EE0` is observed as a no-op. Evidence: `0x417740`
(`ev_33f06f0c2ab2a0355eacac31501a31573e46e7ef343e762e807edae09049075e`),
`0x418160` (`ev_19bb56f3601c354484189057db4ef33b81f572230b3c9ed82f0106ed95deadbf`),
`0x4181E0` (`ev_95abdfc504f67f9ea1381f0879a562b9c116a5ef814f4aadf3ec5f5bd0691bd4`),
and `0x418230` (`ev_4b90f67402273605610db531514346efd28b4db5f387d073306a17b88de694ee`).
The no-op helper `0x417EE0` is covered by
`ev_99be2873b17eebcda3702444925fb0552d234a7fa3d1fd3107e30e00795ee4db`.

`plan_resource_set_sweep` captures callback selection and effects for
`0x417870` and `0x4178B0`. The shared `0x417880` iterator visits nodes in list
order, checks the entry name with `0x417330`, and dispatches only names not
ending in `*`. The `0x417870` wrapper supplies `0x417820`, which finds the
matching set by name and releases its mapping tuple. `0x411CB0` frees the fake
map buffer when the ownership byte is set; for a real mapping it unmaps the
view, closes the mapping handle, and clears both fields.

The `0x4178B0` wrapper supplies `0x417790`. It finds the named set, remaps it
through `0x4172D0`/`0x411BE0`, then retargets the index-record `+0x20` pointer
for each initially buffered chunk to the corresponding file-mapped chunk
offset. The offset starts at `0x10 + entry_count * 0x24` and advances by the
low 31 bits of each entry's payload-size/attribute word plus the 8-byte chunk
header. Runtime mapping results, node counts, and index words remain inputs.
Evidence: `0x417880`
(`ev_e364f14f99f41318a23f9353cbdd57f82df9bf96409d66287a669a36404de008`),
`0x417330` (`ev_b0df93edd5b5a6f226529a6b4f4bcd9f2110e0e8e63fef7be2509a14f02570cc`),
`0x417870` (`ev_6fe560d009cc0a2ca10d108b7f3dbdc4ba0d674e41aed758647ffdac361fa515`),
`0x4178B0` (`ev_86e17d81e89805093da4b37a4444121dd3eeff3875f8ebe8d305163a439c2477`),
`0x417790` (`ev_6a9cd4d11a05b777b26ab214c5e6b34e80853c966de2141cb32223aecd8143aa`),
`0x417820` (`ev_912aa432729f6842fa9c0eba7f4516f3d29f6be5b0ef104c067a0f500eaae08b`),
`0x4172D0` (`ev_7416732e5ddb2c88b50b181dd03e0174d4a03174c372300a0418ef32e1759ed0`),
`0x417300` (`ev_79540d8d5822dff565452891ae8c954224c8056aa5e976853c8d1cb56b0015f2`),
`0x411CB0` (`ev_0131eccd249610d135d1e183e25d987aeb4f3dedb5e9b93b594ac67cba943c57`),
and `0x411BC0` (`ev_40aab79f0d489e81741aa5146771e2063d9ad57e41aa0fbfab173c26081a6315`).

`plan_legacy_file_read` models the exact-byte helper at `0x411560`. Its input
handle slot is one-based and selects table entry `slot - 1` (entries are
`0x104` bytes apart). A successful `ReadFile` returns true only when the byte
count equals the request; a short read invokes panic format `0x4D1AC8`. An API
failure calls `GetLastError`, invokes panic format `0x4D1AE8`, and returns
false. The OS result and error code remain observations. Evidence:
`0x411560` (`ev_584073b2b3ba4728853bb7f0a2be2531a803ad3995c2d9744f562551c0133f64`).

The helper's surrounding legacy file table is now traced through its basic
operations. `0x411430` opens a path read-only with shared-read access and
existing-file semantics, then registers the native handle through `0x4112E0`;
its public result is the registered zero-based slot plus one, with zero for
open failure. `0x4112E0` serializes table updates with the file-table lock,
allocates a slot (up to the observed 32-entry capacity), copies the path into
the corresponding `0x104`-byte entry, and stores the handle. `0x411500` closes
the handle selected by a one-based slot and clears the slot. `0x411540` obtains
the selected handle's file size. Evidence: `0x411430`
(`ev_e9257df4eff5efd11561a68b6fa130ea95915724e654742e7c370109b3545803`),
`0x4112E0` (`ev_ec8127e800b6ee4dd67725698a64ce19a31997c47c91dbebbb207e7ebf21dc4a`),
`0x411500` (`ev_18c743cf27c6345f56bb5973df0e05964ee488ebdfbb0052ffbae65dc9618727`),
and `0x411540` (`ev_e48d9751255df52552f6d1994ac8d996d3006a341fed929b241d8d557d44b6b7`).
The `0x411540` wrapper passes the selected handle to `GetFileSize` with a null
high-word output pointer; its low-word result is modeled as an input from the
OS call. The `plan_legacy_file_size` projection applies the same one-based
slot-to-table-index conversion used by reads and closes.
The C++ projections `plan_legacy_file_open`, `plan_legacy_file_register`, and
`plan_legacy_file_close`
now preserve the `CreateFileA` parameters, lock/unlock sequence, allocation
behavior, one-based slot
conversion, panic-on-full branch, and close/invalidate/caller-zero effects.
For a full 32-slot table, occupied-slot observations are required to choose a
free entry; native handle values and OS close outcomes remain outside the
projection. The registration path copy uses the observed `0x100`-byte path
area without a length guard before writing the handle at entry offset `0x100`;
the projection reports when `path.size() + 1` exceeds that area.
