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
| `+4`, `+8` | signed planar pair value A |
| `+0xC`, `+0x10` | signed planar pair value B |

The initialization function zeros the four floats. Accessors `0x41f0a0` and
`0x41f0e0` return the corresponding two-float value with signs reversed when
queried from the second ordinal. Consumers therefore see oriented pair values
rather than an unordered raw record. The offsets are observed; the exact
coordinate basis is not established by these accessors alone.

## Proximity and threat scoring

`0x42adb0` reads both pair values. Its call path normalizes the subject
vehicle's planar velocity (`+0x234`, `+0x23C`), rotates that direction through
`0x42a630`, and projects pair data into forward/lateral components. Thresholds
gate a near-pass branch and a separate positive-forward branch. In
`0x42b040`, a second consumer resolves the other vehicle's row in the registry,
computes relative position and velocity against a sampled route segment, and
may pass candidate values to `0x42b3e0`.

The strongest supported interpretation is that pair value A is relative planar
velocity and pair value B is relative planar position: the proximity consumers
use them as such in route and motion projections. The selected coordinate
basis, units, producer writes, and complete callback semantics remain
unverified. `0x42b3e0` stores a selected candidate in fields around `+0xEDC` to
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

ACAR function `0x429e90` manipulates a circular per-car buffer addressed from
`+0x1054`, with `+0x1058` as its slot count and 24-byte entries. The helper
`0x428b10` computes a wrapped entry address from that base, count, current
entry, and signed displacement. The callback path also queries four wheel
subobjects and updates per-entry flag bytes. These operations connect the AI
update to route/response history, but do not write the shared pair-record
vectors identified above; the pair-state producer remains unresolved.

## Reconstruction status

This registry has no live equivalent in the current source runtime. The
route-opponent pacing controller is an authored prototype and does not consume
this native pair state. Do not treat these static findings as runtime-validated
physics or as proof of original AI policy. Next evidence needed: identify the
native pair-record writers, establish both coordinate bases and units, capture
the full callback caller, and compare controlled original-game runs against
the source runtime before integrating any behavior.

## Evidence

- Registry initialization: `0x41f120`, `0x41e9b0`, `0x41eab0`, `0x41f190`.
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
