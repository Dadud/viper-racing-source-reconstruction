# Vehicle pair registry and proximity callback

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Addresses below are image virtual addresses. This is a static reconstruction
from Ghidra analysis of the shipped x86 binary; decompiler output is not
original source code. Field semantics are limited to what the data flow
supports.

## Registry layout and lifecycle

`0x41f120` allocates a registry for the active vehicle count and publishes its
pointer through `DAT_004D45B0`/`DAT_004D45B4`; `0x41ea70` clears those globals.
The constructor at `0x41e9b0` sets the registry count at `+0`, allocates a
20-byte record for every unordered pair at `+8`, stores a row stride at `+0xC`,
and allocates a row/block table at `+0x10`. `0x41eab0` initializes the table
with shared pair-record pointers for both directions. `0x41a310`, called from
`0x45b6b0`, derives the active count and initializes the registry.

Each pair record is initialized by `0x41f190`:

| Offset | Observed role |
|---|---|
| `+0`, `+2` | two vehicle ordinals (16-bit) |
| `+4`, `+8` | position delta X/Z (B minus A) |
| `+0xC`, `+0x10` | velocity delta X/Z (B minus A) |

The initialization function zeros the four floats. The writer path is
`0x41AEB0` → `0x419E80` → `0x41EC00` → `0x41EC40`: it gathers registered
objects' body X/Z position (`+0x5C/+0x64`) and velocity (`+0x234/+0x23C`),
copies the two vectors into ordinal rows, then writes partner-row minus
selected-row position into pair-record `+4/+8` and velocity into `+0xC/+0x10`.
Accessors `0x41f0a0` and `0x41f0e0` expose the oriented values, reversing signs
when queried from the second ordinal. The exact coordinate basis and units are
not established by these operations alone.

## Proximity and threat scoring

`0x42adb0` reads both pair values. Its call path normalizes the subject
vehicle's planar velocity (`+0x234`, `+0x23C`), rotates that direction through
`0x42a630`, and projects pair data into forward/lateral components. Thresholds
gate a near-pass branch and a separate positive-forward branch. In
`0x42b040`, a second consumer resolves the other vehicle's row in the registry,
computes relative position and velocity against a sampled route segment, and
may pass candidate values to `0x42b3e0`.

The producer and consumers establish the first pair vector as relative planar
position and the second as relative planar velocity. The selected coordinate
basis, units, and complete callback semantics remain unverified. `0x42b3e0`
stores a selected candidate in fields around `+0xEDC` to
`+0xEF4`, with replacement rules based on sign, magnitude, and a flag; this is
consistent with retaining a nearest/strongest threat candidate but does not
prove the original field names or AI response.

The per-car update helper at `0x42a8a0` visits at most three pair records from
the registry's third table layer. It invokes the car's virtual callback at
`+0xECC` with each pair-record pointer, and calls `0x428a90` when that callback
returns nonzero. `0x428a90` clamps the car field at `+0x1050` upward to 7.
This establishes that pair records enter a native per-car callback before the
rest of the update; it does not establish which concrete vtable target is
installed for every car type.

The writer also sorts the middle pointer row with `0x41EE90` and the last row
with `0x41ED20`; this registry model deliberately leaves those orderings to the
separate ordinal-lookup resolver because the second comparator uses additional
projected-distance logic. ACAR function `0x429e90` manipulates a circular per-car buffer addressed from
`+0x1054`, with `+0x1058` as its slot count and 24-byte entries. The helper
`0x428b10` computes a wrapped entry address from that base, count, current
entry, and signed displacement. The callback path also queries four wheel
subobjects and updates per-entry flag bytes. These operations connect the AI
update to route/response history, but are not the shared pair-record producer.

## Reconstruction status

The native constructor and lookup layout now have a source model in
`include/viper/vehicle_pair_registry.hpp` and
`src/vehicle_pair_registry.cpp`, with a focused regression in
`tests/vehicle_pair_registry_regression.cpp`. The model reconstructs pair
allocation order, three shared lookup layers, row-stride byte offsets, and the
observed sign reversal in accessors. It also models the static row-to-pair
delta writes from complete caller-supplied rows, without performing either
native sort. Participant rows remain unavailable to the production session:
the player body is not advanced by production integration and opponents only
have route-derived poses. The live route-opponent controller does not consume
this model. Do not treat it as runtime-validated physics or proof of original
AI policy. Next evidence needed: establish coordinate units, capture the full
callback caller and live rows, and compare controlled original-game runs
against the source runtime before integrating behavior.

## Evidence

- Registry initialization: `0x41f120`, `0x41e9b0`, `0x41eab0`, `0x41f190`.
- Participant-row and pair-delta writer: `0x41AEB0`, `0x419E80`, `0x41EC00`,
  `0x41EC40`.
- Pair accessors: `0x41f0a0`, `0x41f0e0`.
- Consumers and candidate selection: `0x42adb0`, `0x42b040`, `0x42b320`,
  `0x42b3e0`.
- Pair dispatch and ACAR response-history path: `0x42a8a0`, `0x428a90`,
  `0x429e90`, `0x428b10`.
- Caller population and global references: `0x41a310`, `0x45b6b0`, and the
  cross-references to `DAT_004D45B0`.
- Pair callback dispatch: `0x42a8a0` (`ev_16f7f8484a4d47a87caf8e6dd00ae2a2747a2e488626040b995bbc053d297bf9`),
  `0x428a90` (`ev_5907d153d93267f5bd294d2550cd17f74e6f63a69e73c679fdd6398f61857f0f`),
  `0x429e90` (`ev_a782385f42d81f8b92eded84ba2b914aade117a667fc70c44f3d2221ae779e16`),
  and circular-buffer index helper `0x428b10`
  (`ev_5fe8f92f19bf062e27beb6d2b20221b1b603a99728547928ac2741e215cea4a2`).
