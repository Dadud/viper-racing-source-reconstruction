# Physics-task object loading and car callbacks

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA; addresses below are image virtual
addresses.

## Array population

`0x420480` allocates `DAT_00507D0C` with `0x800` bytes, sets the object count
`DAT_005081E8` to zero, and repeatedly calls `0x4224D0` with the next four-byte
slot address. The count increments after each successful read and construction.
`0x4224D0` reads an object record, calls factory `0x422580`, invokes its virtual
`+0x2C` method, conditionally calls `0x426F20`, and stores its pointer in the
supplied slot. `0x426F20` assigns an object-local index at offset `+0x34` and
advances a global index by the object's virtual `+0x18` result; it is not the
main entity-registry insertion routine.

Before populating that object array, `0x420480` obtains the track-state record
from `0x45C2C0` and passes it to `0x426000`. That helper switches on the DWORD
at input offset `+0x18` (decompiler expression `param_1[6]`): selector 0
allocates `0x7DC` bytes and calls `0x43C870`; selectors 1 and 3 allocate
`0x68` bytes, initialize with `0x43BD10`, then install vtables `0x4C7810` and
`0x4C7880` respectively; selector 5 allocates `0x48` bytes and calls
`0x43C360`. Other selectors call panic reporter `0x411000`. The helper
unconditionally invokes virtual `+0x14` at its shared exit, including after
allocation failure and after a returning panic reporter, where the selected
object pointer is null. After the record array is loaded, `0x420480` invokes
virtual `+0x0C` on the selected service object. This service object is separate
from entries created by the later `0x422580` record factory.

The `0x420480` callsite gets this pointer from `0x45C2C0`, whose body calls
`0x45C270` (a one-instruction `RET`) and returns `&DAT_0053B17C`. The loaded
image has 64 zero bytes at `0x53B17C..0x53B1BB`, so the selector at
`0x53B194` is zero in that image. The address has one direct read reference,
at `0x45D9E2`; no direct store reference was found. This supports selector 0
as the initial value for the unmodified image, but does not establish the
runtime value after possible indirect writes or external configuration. The
checkpoint parser at `0x45D950` also reads `0x53B194` as a nonzero/zero gate;
it routes nonzero to `checkpt1.mod` object creation and zero to direct
checkpoint registration. Evidence: `0x420480`
`ev_b3969ce0ed7661c09bc1c4b6c36050333512037ece3893fdfadd017abdb7146f`,
`0x45C2C0` `ev_ca0884b64d4548bff8d6219dc665401342d4cd1ec98fbeb98f786962d6a0c71a`,
`0x45C270` `ev_ec36da9553ac764a0b44b729da46baf10d3517d5e3e66d261cc897327136dfca`,
`0x45D950` `ev_ab55f8e4f7c5ea92fb027cd8511e93de40b12f39448f9c864954b4d6384f697d`,
and image bytes `ev_f51e0d331c83886c2106979a08931ff88b0d78e00750b51833c78743e8a52320`.
The direct-reference query IDs are `ev_43a650d41912a222fb848e398a66cd6c4211b6116b89b57dd3766736547a7895`
for `0x53B194` and `ev_c607999ce4132d70fefbe5f5a19dbba1452a1954d3bfd4a5c7e710e48dfcef1e`
for the returned base `0x53B17C`. The 19-record session bundle is
`metadata/physics_track_selector_source.evidence.json`.

The selector-1 and selector-3 vtables share the observed status methods documented in
`frontend_dispatch.md`: `+8` stores the mode byte as 3, `+0x0C` checks whether
fields `+0x44`, `+0x48`, and `+0x4C` are nonzero, and `+0x14` clears fields
`+0x54`, `+0x5C`, `+0x60`, and `+0x64` before storing mode 1. The gameplay
meaning of these type numbers and the status byte is still unknown.

The factory recognizes the following object FourCC values and constructors:

| FourCC tag | Value in the x86 binary | Constructor | Allocation |
| --- | ---: | ---: | ---: |
| `BALL` | `0x42414C4C` | `0x43ABC0` | `0x480` |
| `ACAR` | `0x41434152` | `0x4285D0` | `0x1080` |
| `GCAR` | `0x47434152` | `0x4278F0` | `0xF00` |
| `CHKP` | `0x43484B50` | `0x43A620` | `0x84` |
| `OBST` | `0x4F425354` | `0x436860` | `0x4B0` |
| `NCAR` | `0x4E434152` | `0x438E40` | `0xF38` |
| `PCAR` | `0x50434152` | `0x4381E0` | `0xEF0` |
| `STAT` | `0x53544154` | `0x437C60` | `0x6C` |
| `WOBL` | `0x574F424C` | `0x436ED0` | `0x4B4` |

These four-character labels are the readable ASCII interpretation of the
numeric tags in the factory comparisons; they are not established class names.
`NCAR` calls base car constructor
`0x42FF90` before installing vtable `0x4C8238`. `PCAR` uses the constructor at
`0x4381E0` and installs vtable `0x4C8188`.

`physics_task_object_factory_descriptor` and
`plan_physics_task_object_factory` in the source project encode this exact
dispatch table. Allocation success, constructor return, and whether the shared
panic reporter returns remain explicit observations. The factory evidence is
`0x422580` (`ev_16e9359e1fff260aa38cf33c56d1c882043ea11712c8fa31761a929ff9f41f37`).

`plan_physics_stat_task_constructor` projects the derived constructor selected
for `STAT`. After common base initialization it installs vtable `0x4C8100`
and copies the 12 DWORDs from record offsets `+0x08..+0x34` to object offsets
`+0x38..+0x64`. A nonzero DWORD at record `+0x38` calls the unsupported
collision-volume reporter and returns the object before allocation. Otherwise
it allocates `0x30` bytes for one child and calls `0x42D460` on success with
record floats at `+0x3C`, `+0x40`, and `+0x44`; the child stores those as its
three position fields and their maximum at `+0x28`. The parent stores the
child pointer at `+0x14` and increments its count at `+0x24` even when child
allocation fails (in that case it stores null). The allocation address remains
an optional observation, and the meaning of the collision flag and `0.2`
constructor constant is not inferred. Evidence: `0x437C60`,
`ev_01bdfcf3294422a00cd7127b25cfba0f4fc434611917e1b2ff1d26f673977ed1`;
common base initializer `0x4375C0`,
`ev_1bd5688db76155c26f088bb709fc8bce8430eb03d9df862ba2786ca8ef1b6ae0`; and
child box constructor `0x42D460`,
`ev_e8bea9ef634610ace27b2bc375673e9184ebccd218aeb9e9e60d0c9688f83277`.

`plan_physics_wobl_task_constructor` models the WOBL record's shared-resource
link. It first records the record `+0x1C` value at object `+0x4AC`, then
performs the `0x420970` resource-ID search between the observed lock helpers.
The first matching resource supplies 12 DWORDs copied to object `+0x47C..+0x4A8`.
Its intrusive node receives the object's `+0x38` address at node `+8`, then the
object address at node `+4`; the constructor clears object `+0x4B0` and mirrors
the 12 DWORDs into object `+0x38..+0x64`. If no resource matches, the caller
dereferences the null lookup result after the initial `+0x4AC` write. If the
matched resource has a null intrusive node, the 12-word copy has happened but
the link and trailing object writes do not. Evidence: `0x436ED0`,
`ev_c32449a435e472d676ccf1523ef95a86af06499d25f251773a15954fa26b50db`, and
resource lookup `0x420970`,
`ev_1bb2651b902d18f7d9e14701002e84f4ff795cdcc7c2b339e4c053022f73db7b`.

`plan_physics_gcar_task_constructor` now covers the GCAR constructor's large
sample buffer and the bounded tail of its source initialization. It records
the base-car field copies at `+0xEC8` and `+0xECC`, singleton assignment at
`0x508AD4`, allocation of `0x23280` bytes, and the `0x960` calls to
`0x40EC80` when allocation succeeds. That helper only returns its input, so the
new buffer remains uninitialized until optional sample data is copied. The
`0x427A40` tail's fixed resets and the failed-source defaults are projected;
on source success the supplied header words map to `+0xED8`, `+0xEE0`, and
`+0xEF8`, while its cursor goes to `+0xECC`. The payload copy requests
`count * 0x3C / 4` DWORDs, records the captured prefix, and flags null-buffer,
truncated-observation, or allocation-overrun cases. On complete nonfaulting
paths the `+0xEEC`, `+0xEF0`, `+0xED4`, and `+0xEE4` final writes are preserved.
The base-car constructor `0x43DFB0` and the source helper's native data contents
remain outside this projection. Evidence: `0x4278F0`,
`ev_0e875134732b6d19b3ec2e544a4b320f6bcf6805a3ce9a755180727ed75d5e02`;
base constructor `0x43DFB0`,
`ev_1351d1ca9c7235b1a460df854f730aed6d44f570052e7efdbfb3dcbae022fbc9`;
identity helper `0x40EC80`,
`ev_dbf6f28c6f21285954746f955b387d97130255708255cb41461ae17ceb839963`;
tail helper `0x427A40`,
`ev_8dc6849321e0690e91aa1094409d0e760a24dfede861de14162468f44c8b8ee9`; and
copy helper `0x427BF0`,
`ev_3952797030559a14e1424ff087668b8901ca5e3a635f95d2c69d3497e61e520f`.

`plan_physics_checkpoint_task_constructor` projects the `CHKP` record's two
3D endpoints at `+0x08..+0x1C`, their delta, and the derived basis at object
`+0x38..+0x58`. The constructor normalizes the segment, a cross-product vector
from `0x434BF0`, and its perpendicular, then rotates that 3x3 basis about Y
using the record's degree value at `+0x24`. It stores the endpoint midpoint at
`+0x5C..+0x64`; the transformed endpoint vectors are also returned as local
intermediate observations. It finally makes the unguarded global backend
virtual `+0x34` call; a null receiver is reported after the preceding writes.
Host trigonometry, x87 rounding, and Pentium-FDIV behavior are approximations.
Evidence: `0x43A620`,
`ev_28f1d49ae0dce63fb5c471cd65da48e9a4cd1a72b586c3932cbffb431e7d1bf2`;
cross product `0x434BF0`,
`ev_e8ed20d85f950e46855973603f0e96e9f313df3c2c611318b39daa5958f7d632`;
matrix rotation and product `0x402D10`,
`ev_180ccb60ee7d72a02b914e750a3ca48640f177ef8fecba92ab9545b87d3c3023`,
`0x402DD0`,
`ev_405f1cc8b926abf27c24c930e07896bb34b49ce6a13cc1135c6cf7f01299b64a`;
and endpoint transform `0x42FBF0`,
`ev_d5fb1607dd5445a7120632c22341841fcaad0ab5c8cfde4dba0d94e68a6e7bc0`.

`plan_physics_ball_task_constructor` projects the `BALL` derived constructor
after common car initialization. It installs vtable `0x4C8300`, copies record
`+0x34` to object `+0x47C`, and copies position words from record `+0x18`
through `+0x20` to object `+0x5C..+0x64`. It requests a `0x40`-byte sphere
child. On allocation success, the `0x42BE10`/`0x42BDB0` chain receives the
parent basis pointer, `SPHR` tag, record resource ID, parent pointer, radius
`record[0x2C]^2 * record[0x28] * 0x432F3264`, and length
`record[0x24] * 0x3CD013A9`. The child also gets a zero local center and the
observed default fields. Allocation failure stores a null child but still
increments parent `+0x24`, sets `+0x474`/`+0x478`, and writes the parent pointer
to `0x5092C0 + 4*record[0x34]` with no bounds check. The parent count starts at
zero through the common car base constructor. Evidence: `0x43ABC0`,
`ev_fe4303e4cc4be39e47d4bda2ba909240ddc666997bd535debd396e7ad14b07de`;
common car base `0x43E490`,
`ev_16211305309cb04932d13ad5a1d369ad4e4c7e92d881a1ec107054c31cc4ab19`;
sphere constructor `0x42BE10`,
`ev_a85d856838f6b80e0b2072601beae8b93a48d13b1723a85331744fa717352b10`;
shape base `0x42BDB0`,
`ev_01f840b4961b50377964104f0d7ca185ed6d911474f9ef4af2f0cc750ba839ad`;
registration write `0x43ACB0`,
`ev_a40b27210cfa01829d2330ce81b645dcd0ceddf95f1cffb7e3b596c472cd78e9`;
and constants at `0x4C82D4`/`0x4C82D8`,
`ev_23f2d49bf497261bdffb6dc6bc7a09e2a129e0fcdd29e75e6b1b69792c48e181`/
`ev_c67ed9fed1533cc24bef5d603aed5d0e8e15c059d4f400611c14c87928ac4a40`.

## Fixed-step update dispatch

`0x421050`, reached per physics step from `0x4207C0`, iterates
`DAT_00507D0C`. For a non-null object it checks virtual `+0x2C`; if active, it
calls virtual `+8`. It later calls virtual `+0x0C` for every non-null object.
The earlier array initialization and factory calls show that car objects
constructed from `NCAR` or `PCAR` records are in the same array traversed by
this per-step dispatch.

The vtable bytes at `0x4C8238` give `NCAR` virtual `+8` target `0x439200`; the
bytes at `0x4C8188` give `PCAR` virtual `+8` target `0x4384D0`. Their
disassembly conditionally calls `0x431150` (NCAR: `0x4392BF` and `0x439336`;
PCAR: `0x43872F`). That method updates four wheels and then calls rigid-body
integration `0x43ECE0`. These edges link the fixed-step task to car body updates
for the corresponding active/state branches.

The main entity registry is populated separately. World parser `0x45CAE0`
creates a scene object for each car record through `0x463920` or `0x466840`,
then appends it through `0x45C320`/`0x45C2E0`; that scene object's vtable
`+8` target is `0x463EB0`, not the physics car's vtable target. Its observed
direct call graph does not reach `0x431150`. These are distinct per-car
instances created from the same record, as traced below. Indirect scene-update
effects remain unresolved. Virtual `+0x2C` and class-specific conditions can
suppress the fixed-step car callback.

## Shared source record and car identity

World initialization at `0x45B6B0` calls the world-record loader `0x45C630`
before it sorts the scene objects and starts the physics-task callback through
`0x425540`. The `obj_car` parser at `0x45CAE0` builds a `0x1E4`-byte record,
sets its tag from the car variant, and stores the incrementing car-record index
at record offset `+0x194`.

The scene constructor `0x463920` calls `0x463510` with that same record.
`0x463510` calls `0x425720`, which writes `record[0..record_size)` into the
buffer allocated at `0x425420` (`DAT_00508754`) through its cursor
`DAT_0050879C`, then advances the buffer cursor. The derived scene constructor
`0x466840` calls `0x463920` as well. After this world-load pass, the physics
callback's initialization state calls `0x420480(DAT_00508754)`. That routine
deserializes the written records through `0x4224D0` and constructs their
physics objects through `0x422580`.

This establishes that the physics task consumes the same serialized car
records produced while creating the scene objects. The ordinal at record
offset `+0x194` is therefore shared: scene constructor `0x463920` copies it to
scene-object offset `+0x1C8`, and physics constructor `0x42FF90` copies it to
physics-object offset `+0x510`. The two systems create distinct objects from
the same record, so their per-car identity correspondence is established. The
scene object's direct `+8` path still does not show a direct call to
`0x431150`; class-specific physics callbacks perform the known wheel/body
integration.

`plan_physics_track_service` projects selector dispatch, allocation size,
constructor/vtable choices, panic behavior, and the final unconditional
virtual `+0x14` call. For selectors 1 and 3, the initializer is a void helper
and the pointer is installed directly after it, so no constructor return value
is inferred. Allocation/constructor outcomes remain observations. Evidence:
`0x426000`, `ev_2b5d1097e10c597fa911959cea8832d4fc90daf1d5a108167731b95dc8c346ae`.

The array loop is projected by `plan_physics_task_array_population`. It records
the fixed `0x800`-byte allocation, the unconditional first reader call, count
increments after successful reads, and termination on the first false
low-byte result. A bounded sequence containing only successful observations
is reported as exhausted rather than assumed to be the real end of the input.
The binary makes no allocation-null check before passing the global array to
the reader, and it makes no explicit 512-pointer capacity check; both paths
are surfaced. This loop projection does not model the preceding global reset
stores or the later resource lookup/service callback. Evidence:
`0x420480`, `ev_c7a8122ef79fb8e7d7e9e166e4bee5d0461c382879d95c4f0a01ddde1748d040`.

The `0x426EB0` helper called between array closure and the `MIL` lookup is
projected by `plan_physics_task_table_init`. It allocates a fixed `0x200000`
bytes, computes unsigned `stride = previous_stride + 11`, divides that fixed
size by the stride, and clamps the quotient using the signed comparison
against the observed maximum-entry global. It stores the chosen count and
stride, then allocates `count * 4` bytes and fills `count & 0x3FFFFFFF` DWORDs
with `0xFFFFFFFF`. The following byte-fill loop is present in the decompiler
output but starts its counter at zero, so it performs no iterations. A zero
stride faults at division; a failed sentinel allocation faults if the fill
count is nonzero. The primary allocation result is stored without an immediate
check. Evidence: `0x426EB0`,
`ev_8ad67858bad8d450cc1f946396d635333ab6ff9fb0bab2e840908a23c7fd763f`.

After that helper, `0x420480` calls `0x4C59B0(0x004D494C,
DAT_00508784 + 0x7DC)`. The wrapper allocates `0x40` bytes and, on success,
calls `0x4C54B0` with the type and payload size. A returned object exposes its
registry index at offset zero; the wrapper stores the object in
`DAT_0056AFB8[index]` and returns `index + 1`. Allocation failure calls the
panic reporter and returns zero if it returns. Constructor synchronization
failure also calls the reporter; when it returns, `0x4C54B0` returns the
object despite not establishing a usable index, so index/registration effects
remain observations in `plan_legacy_resource_registration`.

Once this lookup returns, `0x420480` unconditionally calls virtual `+0x0C` on
the track service object and then sets `DAT_00508199` to 1 and
`DAT_00508198` to 0. The observed `+0x0C` targets by selector are `0x43CB20`
(0), `0x43BEE0` (1 and 3), and `0x43C3C0` (5), read from the selected vtables.
Selector 0's callback at `0x43CB20` loops 16 times, zeroing DWORDs at object
offsets `0x78C + 4*i` and writing `i + 1` at offsets `0x74 * (i + 1)`, then
returns 1. The selector-1/3 callback is the observed three-field predicate at
`0x43BEE0`; selector 5's callback at `0x43C3C0` returns 1 immediately.
Registration
evidence: `0x4C59B0`,
`ev_e930a70e159405d16b4cd7c9a7a3b1de2fa678ee7a4e1adcb188594195d84263`, and
`0x4C54B0`, `ev_9df77eb507cefea6dc26ef5ad85565a10ece271069287790c58fb47988f77a23`.
Vtable evidence: `ev_da7a9eecb92a8e16ecdcb18f826acf7c967db879b937a9da55ddd357534fa107`,
`ev_df5368c57a4556a9236aa457936eb2bc99814ecbab560077cd4c7f5e25535296`,
`ev_4a3b3471c71109794a217ce08c7d76dcc872daec61a4b5cebf0b58bca7848b56`, and
`ev_501298c41629653b4e6893f86e75d49d6c87fd00fefc02ca7280dc6adf931518`.
Selector-5 callback evidence: `ev_12218e96b303370f87ea8aa9eae066440e7b821b27b6701a855daf135f2f718f`.

`plan_physics_track_post_load` evaluates the recovered virtual callbacks:
selector 0 returns true after producing its 32 ordered object writes; selectors
1 and 3 return true exactly when all three observed fields at `+0x44`, `+0x48`,
and `+0x4C` are nonzero; selector 5 returns true unconditionally. It reports
the binary's unguarded virtual call on a null receiver as a fault path.
Selector-0 instruction evidence includes the loop body at `0x43CB2B`,
`ev_3bf36b339017898001211451ad0ec8f331bd41892f644820d8631605cfe89ba0`,
the 16-count branch `ev_c515eaa655c2808f8e60358069f50e83c28c71067553d6d9de0d687aa1144f3b`,
and return `ev_9945f2492c4aa9d15123120706d9d8b2c1a674a7fbcfa6a81d639e10c8752acb`.

`physics_task_initialization_global_stores` returns the 61 literal DWORD
stores in `0x420480` source order. This includes the 16-DWORD zero range at
`0x507D70`, three 3x4 identity matrices, the three-word zero range at
`0x508640`, scalar reset values, and the final `0x508199 = 1` / `0x508198 = 0`
flags. Allocator and resource-helper return values are dynamic and are
intentionally not synthesized as literal stores. Evidence:
`0x420480`, `ev_c7a8122ef79fb8e7d7e9e166e4bee5d0461c382879d95c4f0a01ddde1748d040`.

## Alternate-time command handler registry

`plan_physics_command_handler_registry` projects the fixed eight-entry command
table used by `0x426A10`. The track initializer at `0x434B60` registers command
IDs 5 and 7 to code-fragment targets `0x430FB0` and `0x431110`; `0x4360D0`
registers IDs 1 and 3 to `0x436190` and `0x436170`. The first constructed
object of the `0x4285D0` class registers ID 4 to `0x4280D0`; its final
destructor unregisters that target. Registration overwrites the selected slot
after a no-op helper call. Unregistration scans IDs 0 through 7 and clears the
first exact pointer match; failure calls the panic reporter. This model reports
out-of-range registration attempts without reproducing the binary's unsafe
global write. Handler targets are Ghidra code fragments without FunctionManager
entries, so their full parser semantics remain partially open despite the
fixed lengths and effects recovered below. Evidence: `0x427470`,
`ev_f88622415ae97c159571f7ac6910b18ee12f140e1c995a7cd05e1989de3a4a7a`;
`0x4274A0`, `ev_0f228205c6358b981b2e03eb65dbcd340d41bdf1cee40cc52500c390ab44e945`;
`0x434B60`, `ev_63ff2becf352a2b81ae06544d220ebcb9045eca84dedc5fa64eb89bf7fb7ac48`;
`0x4360D0`, `ev_4c48bb7817e4d2f6e653dedd1aa07dcbf82079f99d461e7ee9b8d3dbd5675fa5`;
`0x4285D0`, `ev_576efd2d3278724c54a566ca1ed90b015dac9a8b004fdebc71de16b8ac988112`;
`0x4289B0`, `ev_4b8fff18e11f37b984b900aa1ad1a058de057c0e4955647e0a31c6782d76016e`.

Raw instruction probes now establish fixed payload lengths for every observed
handler: ID 1 returns 20 bytes (`0x4361CF`, `ev_94208488aa10bec84d30ee55105c6e862b5010108143312e3d1f9ee29050f53d`),
ID 3 returns 28 (`0x436181`, `ev_05f73c96c04cc7eebc279075b4be9fc8e8fad50f8a26c3070b1837b8619e34f5`),
ID 4 returns 44 (`0x4280EC`, `ev_f05c354c77a4fef46c2d2b92695eea26b880e6133c2ba9b4d188ef5451fb2076`),
ID 5 returns 14 on either branch (`0x4310E9`, `ev_3e9d59326d3f57feb7d4b00447d9c153ed9c4ae3c05be1eb7f4a2f5dea488ef2`,
and `0x4310F8`, `ev_72a580a15e81e3aa03672710fa376dc1be2753b71d88d0899557ac2323d13309`),
and ID 7 returns 2 (`0x431127`, `ev_8e7691e554ee6eed9078c8b6dfd06708fe3113886d8f7d9df7ff7714e7ba4627`).
`plan_physics_alternate_time_path` now uses these lengths when the handler
observation does not supply one; unknown registered IDs still require an
observed length. Other recovered effects include ID 3's 28-byte append through
`0x420A30`, ID 7's signed-byte task lookup and reset callback, ID 5's indexed
task lookup and either reset or virtual `+0x3C` path, and ID 1's eight-entry
record ring/update callback. Those object and queue mutations remain separate
runtime observations. Helper evidence includes `0x420930`,
`ev_33c4dd341cf4668cfab882395344a1216dc9a03679d4cae15b7d16397591e57d`,
`0x420A30`, `ev_8e78c887ec99b926a8d90ea87f3bc5ce7e5a3645d4aed7f63e5e9662d6c838fb`,
`0x433920`, `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`,
and `0x435FE0`, `ev_04253f3643c884dbc389a7b25838c9b860f180b996d81c377f063b4e569269d0`.

The command-5 non-reset branch also has a recovered packed-float conversion.
Payload bytes 0, 1, and 2 become the first callback vector using
`byte * 10 / 128 - 5`. Unsigned little-endian words at byte offsets 3, 5, and
7 become the second vector using `word * 30000 / 32768 - 15000`. The function
stores both three-float vectors in stack locals and passes their pointers to
the selected object's virtual `+0x3C`; source field names/units are unknown.
The x87 intermediates are exact for these integer inputs and binary constants
before their binary32 stores. Evidence: byte extraction/scaling at
`0x430FD2`/`0x430FD8`/`0x430FDE`/`0x430FE3`/`0x430FF0`/`0x430FF5`/
`0x430FFD`/`0x431004`/`0x43100E`/`0x431014`/`0x431018`,
`0x431053`/`0x431057`/`0x43105B`; word extraction/scaling at
`0x43105F`/`0x431065`/`0x43106B`/`0x431075`/`0x43107D`/`0x431089`/
`0x43108D`/`0x431093`/`0x43109D`/`0x4310A9`/`0x4310AB`/`0x4310B7`/
`0x4310C0`/`0x4310CA`/`0x4310D0` and stores
`0x4310D6`/`0x4310DA`/`0x4310DE`. Constant bytes at `0x4C7D4C` include
15000.0, `0x4C7D50` is 5.0, `0x4C7DAC` is 2.0, and `0x4C7DB4` is
1/32768; constant bytes `ev_4ebc09219a12a72ddda1ed043d218530375ad7294b2f00f18f348adf758b8b9a`,
scale instructions `ev_fbe92dd581bed6cf7cffd3a59b1ea730c05655a0df744e547da77f08136dc510`,
`ev_dcbd6922c0ee82493dca193996db8b55ae54135c50c67e126f319840573b2215`,
`ev_7a964e141626e837aacbb94352c5c156bfa6d4b34f64cc41d023f1963fc56771`,
`ev_f5909875d06a69abda028d8254803df126ed4dcc727dd9b586527beed92ee63d`,
`ev_0f06fe8f8d2281cb4bfa44e9ae4431088b68a88e1331b829b47752fb9c6f799b`,
`ev_4cff5cdbc3444b49bded397833c11a1c2bc8d102d4758e91269875223806a5de`,
`ev_4ba92a92986232183afb82c2b49881cd91251150110428f13fc1be387f0325f4`,
and output stores `ev_914b8a05f821131cc766ed6936489a4261cb2770ed46e2e98b0e312abb2dab88`,
`ev_bcfcc9f88e7c411e8680b34e083b9519b58391a4b78926559e560d4eddd98af2`,
`ev_f2d301772b26e5c6c7b3413ada8086f59eafada91e6cfa9c99d08297b6ab2490`,
and `ev_90da2c23b6239a79afa68bd5d15fa5f9c56ae504a51615bbdd71060542562597`.
Pointer construction/push order (`word` vector as stack argument 0, `byte`
vector as argument 1) is confirmed at `0x4310BC`/`0x4310C2`/`0x4310C6`/
`0x4310C7`: `ev_bb945392cd34a41e17c7698a59bb4bfcf0726c394b0eaf28ef4f25f2da83fa2b`,
`ev_daf399f245be14df9146d73ee162ef8fa27beb2243d386801db58a5fae47d9eb`,
`ev_0c4151fd0dd1cf2dc322b40b8a8516551ba1922570c118c853cb69407151bfd9`,
and `ev_27d135dea58c6cf970bf47c5e625b76a777054c4fd4b0c3ee308e5e88b8d07b4`.

`plan_physics_time_command_payload` now turns those fixed-size payloads into
bounded command effects. ID 3 models the strict `queue_size + 29 < 0x400`
append gate, its leading record-type byte 0, and the 28 payload bytes copied on
success. ID 4 reads a DWORD and records the panic call unless it is 0 or 1.
IDs 5 and 7 sign-extend their index bytes (offset 13 and 0 respectively),
resolve through the 16-entry registered-object table at `0x507D70`,
and report the unguarded null/out-of-range callback path; ID 5 distinguishes
context 2's reset from its virtual `+0x3C` callback. ID 1 exposes its current
signed eight-slot ring index and, when the slot's nested target is present,
the three payload DWORDs and scalar bits passed to `0x435FE0`. Its next cursor
uses signed remainder after increment, and negative or greater-than-seven
indices are marked out of range. These are portable effect projections: they do
not mutate game object memory or the event buffer.

The `0x435FE0` callback is invoked for the selected ring slot. If the pointer at
slot `+0x0C` is nonnull, it writes the three vector DWORDs to slot offsets
`0x1C..0x24` and compares the incoming scalar with the nested target's float at
`+0x08`. An ordered unequal value replaces that scalar and sets target flag
`+0x2C`; equal or unordered (including NaN) comparisons leave it unchanged.
For either outcome it sets target `+0x29` and clears `+0x2B` and `+0x2A`. A null
pointer at slot `+0x0C` makes the helper a no-op. The projection reports scalar
comparison results only when the observation supplies the prior scalar bits;
vector and flag effects require an observed nonnull nested target. Evidence:
`ev_04253f3643c884dbc389a7b25838c9b860f180b996d81c377f063b4e569269d0`.

The new raw-fragment pass confirms handler identity and return sizes without
assuming these ranges are standalone functions. ID 7 at `0x431110` sign-extends
payload byte 0, calls `0x420930`, passes its result to `0x433920`, and returns
2. ID 5 at `0x430FB0` sign-extends payload byte 13 and performs the same object
lookup before selecting the context-2 reset or decoding the two vectors for
virtual `+0x3C`; both exits return 14. ID 4 at `0x4280D0` accepts values 0 and
1, calls the panic reporter for other values, and returns 44. ID 1 at
`0x436190` passes the selected 40-byte ring slot and four payload words to
`0x435FE0`, increments the global cursor, and wraps using signed remainder by
8; it returns 20. Evidence for ID 7: `ev_837423e1a56fc40c868b2d151c59b94f1593746825abd27ff809dab1fce2ffa2`,
`ev_a12a0bb689f3f47109c406d2c63806318b1153469278c82eae4cff55e216cd20`,
`ev_56d269e90510481762cb2a09582e190fabb1ac2a7405b9cb5d18049e92047e1e`,
`ev_01bfe992ced09333d2a7e024f5bfc56fb73d4db1653d0bea438524744fbf9733`,
and `ev_8e7691e554ee6eed9078c8b6dfd06708fe3113886d8f7d9df7ff7714e7ba4627`;
ID 5: `ev_6de4ecfc6c91245fd8fdd129661631424281d9226a455ff82efa2e423826c693`,
`ev_c80fc56e42c431cdad3c232b175e7333dbefb4d13fedf4f7a7a77c3a1a0bf6fa`,
`ev_762257a81c5c9baa0411630e0619a358cdaeac52597305af1e67b7b96b99e604`,
`ev_dabbf16b8fb504f389c7ada2d81605a15cd6c65187fb661378e8a88bf538855b`,
and `ev_d4c9fa398b50182e33ecf12edac23b0fdbf00e8394afd16ba32e901982c39904`;
ID 4: `ev_4a9cbfcbdf485e2813f39965f1f39d30f5e862199ebc2c8823120234b2a52e33`,
`ev_64eb7a9b1aa5e39689bd223c03e504f6f0420cc3d88a5b54c5b66930332a4f7c`,
and `ev_dc097dbe21ba191cbba52c849ca4dfb3c608fbf4d747d7d6295bbae1e5aa11c9`;
ID 1 ring/update and signed wrap: `ev_0200252991fdd1d457d9ee2a2d5f3607073212220aacac0b59f6a0cca8799f72`,
`ev_a3f0e7ff8b4fd06b28a8925495ded26a53a05e8f80df370e70518d3fcfb761a7`,
`ev_0c520f6618e6e0e61074ef95117db990c21e80d3ca51953e48e96ae1515b072f`,
and `ev_12db4dd6bde982a7d974bfdd91c5d9b73733a59871cfc4f9eaeabf27ec84bfbd`.

## Record-read projection

`plan_physics_task_record_load` models one iteration of `0x4224D0`. A negative
record length ends the array without reading payload bytes. For a nonnegative
record length, the stream helpers bounds-check the 8-byte record header and the
declared data copy. A failed bounded read sets the stream error byte and leaves
the cursor unchanged; it does not copy beyond the captured span. However,
`0x4224D0` does not inspect the error byte and continues toward factory dispatch
with potentially uninitialized stack input. The portable projection reports
`incomplete_payload`, the error flag, and that indeterminate factory attempt,
then stops before simulating its consequences. On complete input it delegates
FourCC allocation/construction to the factory model. It then records the
binary's unconditional virtual `+0x2C` call—even if allocation or a constructor
returned null—without executing an unsafe dereference. A true virtual result
triggers `0x426F20`; a non-null object is then stored and the reader reports
success. An unknown FourCC returns from the factory only if the panic reporter
returns; otherwise the reader never reaches the virtual call.
When `0x426F20` runs, it first copies the current task-data cursor into the
object's `+0x34`, calls its virtual `+0x18`, then advances the global cursor by
that callback's returned byte count. The projection accepts the old cursor and
callback size as optional runtime observations; it can report the object offset
even when the returned size is unavailable. Evidence: `0x4224D0`,
`ev_3daf18319a2e5bb333165deb4e9ee8040b81efdb7404bcbb769d852d8969eb61`;
bounded read helpers `0x4C5E10`,
`ev_3dd8fd5732e7cf0d9fc5f647fe988c0844ab55644e80d3444d55bbc33bfa13bb`, and
`0x4C5EA0`,
`ev_2b7eb80d4db3779c05923323444c42dc9163c3d697bc87c890779316eb4c25fb`;
and `0x426F20`,
`ev_01f6bab75271ba29cbd44ddd6feb3a0213b51640ad44436defd19214f505b5cd`.


## OBST constructor projection

`plan_physics_obstacle_task_constructor` models the derived constructor at
`0x436860` after the shared car base constructor. It projects the derived
vtable, timer-active byte, yaw-only basis, record position, and cached 12-DWORD
basis/position copy. The timer value at `+0x4A8` remains an optional runtime
observation returned by `0x425790`; the planner does not invent it.

Shape selectors 0 and 1 allocate `0x40` and `0x44` bytes respectively and
dispatch to child constructors `0x42BE10` and `0x42C400`. Both reserve one
child slot and increment the count even when allocation fails; selector 2
emits six 0x30-byte geometry records, increments the geometry count for each,
then allocates a `0x40`-byte sphere child. The six records preserve the observed
direct-write versus `0x4372C0` helper paths. An unrecognized selector creates
no derived shape child but still completes the shared cache copy. Allocation
addresses are supplied as observations. Evidence for the constructor:
`ev_b4bfa4e7c97b76f2a9c1c6c76bcfbfac735c0a01b7354aac27d1afba84dc362e`;
timer helper `0x436EB0`: `ev_79004fd497de281d1e0b97eabded5bc795dcc3133ce0e8baa7e5d88b98fd7df7`;
geometry helper `0x4372C0`:
`ev_29b5f991b46ddd11e833ac2abe53b20dcc50e00ef7e78739f4990b1c2a8fb142`.

## ACAR task constructor projection

`plan_physics_acar_task_constructor` projects the derived constructor at
`0x4285D0` after its shared car base. It records the derived vtable, first-
instance handler registration gate (`0x508DCC`), seven initial clears at
`+0x1054..+0x106C`, AI profile pointer and selected profile word, and the
profile-name mismatch report path. The selected driver flag is represented as
a byte-valued write at `+0xF54`.

When model data is present, the constructor allocates `descriptor_count *
0x18`, stores the result at `+0x1054`, and calls `0x429440` even if allocation
fails; a positive descriptor count then exposes the null-buffer fault. The
helper's per-descriptor writes are modeled below. It then allocates
`runtime_entry_count * 8` bytes at `+0x1048` and calls `0x42BB60` once per
entry when the allocation succeeds. The final setters compute `+0x4F8` from
`+0xF0C - +0xFCC >= 10`, fill four `+0x6B0`-family slots from the signed
`+0xF10` threshold or selected-driver byte, and set four `+0x6B8`-family slots
to 2 with stride `0x1A4`. The constructor then sets `+0x534` and calls
`0x428790`. Evidence:
`ev_becb0bf54909e1ef5b2a86dfd6fd63e34803446f8fdf780106218d38455f3971`.
The setter bodies and their stride helpers are directly analyzed in
`ev_7f2a7638cef433bcdbf414083f784135b5b01cd65d2b69716ca37eb6939c7e6e`.

The `0x429440` helper is now expanded through `0x429010` and `0x429510`:
each 0x18-byte descriptor clears its first two fields, either copies a pair
from the optional blend source or writes zeroes when the source is absent,
clears its flag byte, maps the linked model node's `+0x1C` scalar through the
observed clamp/scale helper, and stores the node pointer. Node links, blend
pairs, and allocation outcomes remain explicit observations; truncated input
is reported instead of being filled with invented zeros. Descriptor writes
are relative to each allocated 0x18-byte entry; the separately supplied base
address identifies each entry's location. A known null model lookup, failed
nonempty descriptor allocation, or null link before the required count stops
the constructor projection at the observed fault. Evidence for these helpers
and the selection gates: `ev_921603fb8c35029f239b604139468c416af59e42234608c9cf737abd33242e2e`.

`plan_physics_acar_runtime_initialization` expands the constructor's
`0x428790` follow-up. It projects the selected configuration and profile words,
shared-state pointer calculation, timer observation, fixed state constants,
and the descriptor-derived threshold (`+0x1044`) and normalized value
(`+0x1030`) when their source fields are captured. The raw-bit gate deciding
whether the derived threshold replaces profile `+0x98` is preserved. The
`0x429690` path copies `+0xF0C` to `+0xFE4` and calls the `0x41D170` model
sync. That helper copies four task vectors when the model has records, then
selects reset (`0x41D1D0`) or curve update (`0x41D4F0`) from model flag `+0x28`.
The caller performs a second unconditional `0x41D1D0`, modeled as clearing
model `+4` and invoking the `0x41D3A0` curve-distance projection with limit
`10000`. A null
model receiver is reported as a fault before that second call. The initializer
clears one DWORD per eight-byte runtime entry; a null buffer with a positive
count is a fault. On the surviving route, `0x428590` installs callbacks
`0x42B7B0` and `0x42AD80` at offsets `+0xEC8` and `+0xECC`. Evidence:
`ev_e59da5bb7d388e42e9580e51de21b72b204617a3d813fa9aadea6a314ba39804`,
`ev_a84dec33e9d04da3fa72f6439d3c2e599ec3eeab3e6027410a8d32adb0d2ac42`,
`ev_f4bfcbadc58c411dafa520fe96978c340227d8b44f58d8472908504b58b48201`, and
`ev_631cd4c82a9fbea5316c43d028233aa692352fb60df19af0264577d9376e064a`.

`plan_acar_curve_tracker_update` now projects the `0x41D4F0` branch selected
by `0x41D170`. It evaluates the linked segment's cubic Hermite position and
normalized tangent, applies the 150-unit squared-distance reject, performs up
to ten tangent-error/denominator updates, and follows segment links when the
fraction crosses an endpoint. It reports null-link faults and capture gaps
without dereferencing native addresses; x87 precision and the VC98 FDIV path
remain explicit limitations/inputs. The optional curve observation composes
into the ACAR runtime initialization plan only when the non-reset curve branch
is selected. Evidence: `ev_44cc376596921a8b71472de9e75f93d2def8b1a228f10049dc831a15505294df`.
The portable inner link walk is bounded by the captured-node set so non-finite
fractions on cyclic captures cannot hang the reconstruction; this capture
bound is a modeling safeguard, not a recovered game loop limit.

`plan_acar_curve_distance` models the reset/seek helper `0x41D3A0` and its
initial selector `0x41D290`: nearest eligible directed segment selection on a
circular list, linear position/tangent projection, ten Newton updates,
accumulated segment length, the caller's maximum-distance clear, negative
fraction clamp, and the same-node backward-progress cursor clear. It returns
missing-link/capture states instead of following arbitrary pointers. Evidence:
`ev_44cc376596921a8b71472de9e75f93d2def8b1a228f10049dc831a15505294df` and
`ev_631cd4c82a9fbea5316c43d028233aa692352fb60df19af0264577d9376e064a`.

`plan_acar_curve_lookup` projects helper `0x42AD20`, called from fallback
route routine `0x42B040`. When its
`+0x1069` gate is set, it loads the selected descriptor's `+0x14` cursor,
uses model fraction `+0x1064`, and calls `0x41D3A0` with the circular head at
`+0xFE8` and the caller's target point and distance bound. It returns the
updated cursor/fraction, distance result, and the wrapper's nonnull-cursor
boolean. Missing descriptor captures and null descriptor/node faults remain
distinct outcomes. Its returned position distance and cursor validity are
consumed by `0x42B040`; runtime descriptor contents remain supplied inputs.
Evidence: `0x42AD20`
`ev_c2d39ffcfd59d893ca7728fa13e858caae86a3962e967ae6233a1450b625eccb`,
`0x41D3A0` `ev_631cd4c82a9fbea5316c43d028233aa692352fb60df19af0264577d9376e064a`.

`evaluate_acar_driver_callback` models the registered wrapper at `0x42AD80`:
it invokes `0x42ADB0` first and calls `0x42B040` only when the first result is
zero, returning success when either result is nonzero. The direct `0x42AD89`
call confirms that `update_acar_driver_ai_candidates` models the first route
inside this wrapper. That routine applies the raw signed `+0xF10` threshold,
resolves the
two route vectors through the sign-selecting `0x41F0A0`/`0x41F0E0` accessors,
normalizes the car's XZ velocity, and uses `0x42A630`'s `(x,z) -> (z,-x)`
rotation for lateral projection. The `0x42B320` stateful directional candidate
arbitration and `0x42B2E0` forward maximum update are applied to an explicit
state object. `0x41B050`'s global registry lookup result remains an input; the
projection also reports the selected lookup index. Exceptional float encodings
and x87/FDIV precision remain approximate. Evidence: `0x42ADB0`
`ev_ee52cfef3c65309b0c2f076b3e9dfbf236dd69da033939ddfc6563d3e7a20407`,
caller query `ev_789a3d59c544672d0999d5cb6936ed19a57c3adb71fb3275324a09e6fae3db84`,
`0x42B320` `ev_1bfaaf935fc2a8875249c5e2a09ad93fd52f8f9735a719570ffc9913a9b0831`,
`0x42B2E0` `ev_6243204e406a2c5f93f17fafc8df3ad2d4ad60cbb31f582599cd5463d48a10c9`,
`0x42A630` `ev_ee340f9fd04c639504bdf5f8df1695d4203229c416704574d0dc7773ba75183b`,
`0x41F0A0` `ev_468b3e59ecafd5890c7c64c5a7f41829f26efd1cdddfa0ab637ec81742d1f76`,
`0x41F0E0` `ev_204234122e042862454f8ded6d9907c797f279c85edb9ac9655fcabfa2bc0701`,
and `0x41B050` `ev_278b8fb16ac39bb1b38acaa720b2ca9490c3e235c60d947151f3c3b41f16bccf`.
The wrapper call and branch sequence is supported by instructions `0x42AD80`
`ev_72a01f70afb7437130321395a3591dd048be93c5f9781d0617dfde72c72292ca`,
`0x42AD89` `ev_3b2cc9e45cb543f86fab43b3b64471c22d63172c73e7e206b53b56a14aab6363`,
`0x42AD90` `ev_d99f444141352b03eb457e8bc2f5d6c474fe1779dce6429bdabc27181fc3388`,
and success return at `0x42ADA5` `ev_774cc7af43f607868f1e4d126f3e17c64cfb42dae3cc6e39b95c716363075025`.

Two terminal effects of `0x42B040` are now separately projected. The
`update_acar_secondary_candidate` helper models `0x42B3E0`'s strict range
minimum at `+0xEF8`, paired raw payload stores at `+0xEFC/+0xF04`, validity
flag `+0xF08`, and replacement-seen flag `+0xF09`. The
`apply_acar_recovery_state` helper models the `0x42ACB0` fixed writes
(`+0xFD0`, `+0xDE0`, `+0xDE4`, `+0xDE8`, `+0xDEC`) and its `0x4330F0`
steering step: signed route choice, raw-bit lower clamp, `+/-0.24` approach,
[-1,1] bounds, scale by `+0xDCC`, and the `+0xBE4` wheel-pair destination
branch. These helpers are not yet wired into the full `0x42B040` route gate;
its terrain-relative geometry and selection conditions remain open. Evidence:
`0x42B3E0` `ev_abc606ef5bbc526eae79eea29582655c19911c98386f694c35896c1749055482`,
`0x42ACB0` `ev_6f6fdcddecfea72cbd9ae79a348a1b32db98bd7b51faaaf6478c7ffaefd7150a`,
and `0x4330F0` `ev_d2ff91300e7fe5989a96b1723e67a7f9a91b12d91f1f6259430123810fba6691`;
the immediate setters are `ev_985b150bd698d190e02732dbb8ae7b75ef0248366c9a97b122fdf5308da4f4ce`,
`ev_6be9d7eab90202728f763356e39fdd44ba8d52b2e8afa6db7937b0c5908da3fa`,
`ev_c4b47e34375fbf4bd3cb556c06f630cca74cfbc46dce80a77aed30a155152827`, and
`ev_4e237485e955a5ada3411b37c141b93778bcfda352b438d4c0414812405a213a`.

`plan_acar_route_fallback` now composes the major `0x42B040` gates. It bounds
the `0x42AD20` curve search by `4 * +0xF10`, evaluates the selected linked
segment with `0x42A580` Hermite weights and normalized tangent from
`0x41FD80`, computes target-relative lateral/progress projections, and applies
the raw threshold comparisons for remaining distance, route alignment, and
the `+0xF14` lateral tolerance. The final progress-bit branch selects the
`0x42B3E0` minimum-candidate update or the `0x42ACB0` recovery/steering update.
The linked nodes' `+0x3C` values, target-route table row, selected-driver
route orientation, and vehicle state remain explicit observations. Missing
node/scalar captures report `input_incomplete`; no native pointers are read.
Evidence: `0x42B040`
`ev_5ba2192d1ee356002d5e7559588da142bf016877e8f1c911f588ec3bf8052c74`,
`0x42A580` `ev_6b56f4cc7104146b456d6f99f2d11682aa093e37d97212c39ced7d5e5eb78005`,
and `0x41FD80` `ev_d77fad4cfa1a3a294bad814d28b62842dcfb5e7b14fc476f4c9c0b09adbb131e`.

`evaluate_acar_driver_callback_with_fallback` now composes the registered
`0x42AD80` wrapper with both modeled helpers: it runs `0x42ADB0` first and
returns immediately on its nonzero low byte, otherwise it executes the full
`0x42B040` route-fallback projection and returns that result. The earlier
wrapper overload remains available for callers that only have a captured
fallback Boolean. If the fallback reaches missing required observations, the
composed wrapper leaves its return value unknown. Evidence: wrapper instructions
`ev_72a01f70afb7437130321395a3591dd048be93c5f9781d0617dfde72c72292ca`,
`ev_3b2cc9e45cb543f86fab43b3b64471c22d63172c73e7e206b53b56a14aab6363`,
`ev_d99f444141352b03eb457e8bc2f5d6c474fe1779dce6429bdabc27181fc3388`,
and `ev_774cc7af43f607868f1e4d126f3e17c64cfb42dae3cc6e39b95c716363075025`.
