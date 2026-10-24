# Scene car update reconstruction

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA; addresses are image virtual
addresses.

## Virtual dispatch path

The linear disassembly at `0x463EB0` is the scene object's virtual `+8` entry
for vtable `0x4C9420`. It computes two indices in object fields `+0x510` and
`+0x514`, calls `0x460330`, then dispatches virtual `+0x28` through the same
object vtable. The table's `+0x28` entry is `0x4668D0`, a stub that returns
false; it is not the update routine. The update method `0x465800` is at vtable
slot `+0x24`. Change notifier `0x463530` calls this slot when the selected
16-bit field changes (or the `0x425830` force-update gate passes); notifier
caller `0x45BC20` walks the scene objects and supplies the update pointer from
`0x425880`. `should_dispatch_scene_car_update` models the exact changed-field
or forced-refresh decision. Ghidra did not recognize
`0x463EB0` as a standalone function in this analysis; its instructions are
from the saved linear disassembly and the indirect targets are read from the
observed vtable. `0x460330` returns `DAT_0053C9C8`, which the caller folds
into the two indices. The distinct scene and physics objects share the
ordinal from the world record, as detailed in `physics_task_objects.md`.
The slot reference to `0x465800`, the false stub at `0x4668D0`, the
`0x463530` notifier, and its caller `0x45BC20` are preserved in
`metadata/scene_car_dispatch_correction.evidence.json` (records
`ev_0cabf68618ccbecbae7070d7d7aa193369d10cf15714106bcd017c705167058e`,
`ev_093df510e6816c1977ecb4a134261c1054ee89cc55fe186b1f1cd446efa93f3e`,
`ev_f9850233a2e11181b469ce382e050cc61f2121757d46767bcb95f7bd51841373`,
and `ev_e6da7bfaa6593e4230864899ca29fa829b312d80125fbeb59d76cf0ef8d39bc9`).

The outer dispatcher at `0x45BC20` is now projected by
`plan_scene_car_dispatcher`. Its sequence is: profiling start `0x415090`,
sixteen identity initializers at `0x45DAE0`, update/config acquisition at
`0x425880`, twelve DWORD global copies plus three scalar and one byte global
stores, byte-stream callback parsing through `0x462D90`, the counted
`0x463530` scene-object walk, the counted `0x45E010` car-record/UI-table walk,
and profiling end `0x4150A0`. For each scene object, the plan records whether
the incoming 16-bit field differs from the cached DWORD or the force gate is
set; the cache changes to the zero-extended incoming value only when the
virtual update is dispatched. Each car-record operation retains the fourteen
DWORD copy to its `0x170`-byte global row. The supplied object and record spans
stand in for process globals, buffers, and indirect effects; missing span
entries are reported, and a record count above the sixteen initialized local
records is flagged as a possible stack over-read. The UI helper callbacks and
`0x462D90`'s registered byte handlers remain separate projections. Evidence
for the outer function and composed helper paths is retained in
`metadata/scene_car_dispatch_correction.evidence.json`: `0x45BC20`
(`ev_e6da7bfaa6593e4230864899ca29fa829b312d80125fbeb59d76cf0ef8d39bc9`),
`0x425880` (`ev_e1538069c3affc323efce9289324d0a4c3baf58dc14fb8c5d14136d491dae199`),
`0x45E010` (`ev_33fcbe1712d658d58a0d103fa74bfff2dd6e2878295d41bc66ea4b9039eb295a`),
`0x462D90` (`ev_2d0096969d13bd67200d3ea8d5ba62679aea1068dd3fa19b99a9481237a22225`),
`0x463530` (`ev_f9850233a2e11181b469ce382e050cc61f2121757d46767bcb95f7bd51841373`),
and `0x45DAE0` (`ev_fc14044ec6dbfe6acc27e9edd6f2c8a73ab86b83baca33a6e0ac959e7a68311e`).

The parser and handler registry are now modeled separately. The registry reset
at `0x462CF0` clears the count; `0x462D00` appends an eight-byte
`{callback, tag, trailing}` record and increments the count without a capacity
check; `0x462D30` removes the first matching callback by replacing it with the
last entry, or calls the reporter if absent. The known registrations are
`0x468290` for tag 2 and `0x468450` for tag 1 from the scene-effects setup,
and `0x4692A0` for tag 0 from the effects setup. The two setup contexts are
kept distinct because simultaneous activation is not established. At each
payload offset, `0x462D90` scans every registered entry in order and invokes
every handler whose tag matches the current byte, passing the bytes after the
tag. No match stops parsing. Otherwise the last matching handler's return
controls the cursor: `-1` stops; any other value advances by that value plus
one. The three recovered callbacks always return fixed payload sizes:
`0x468290` returns `0x24`, `0x468450` returns `0x18`, and `0x4692A0` returns
`0x1C`; the parser now supplies those values directly, while other callback
returns remain explicit inputs. It records each callback/result and guards
missing payload or callback observations; other handler side effects and
out-of-bounds reads are not fabricated. Evidence for the fixed return sizes is
in the byte-range observations `ev_6f17bfce0c93058dae6edb87749a6a003cc769d78d31a62aa52d1fc04af5afbf`,
`ev_39ffb4a0bad9a4cf4e9bc22dccdb8a25a343e831ea5a0b6639c6597bc1c7086f`, and
`ev_c924832e2207c9f88dace42f6fb0d9e4d8cad232dfb40f54a137d7d09f5e78d9`.
Evidence: `0x462CF0` (`ev_87fc51588d2e71449560f2e725d0887ac52f4f6a0d86eaf3264f7f853b5c2d05`),
`0x462D00` (`ev_27eaa986b6a64fe0b410e9978355b330863059004d75665dbb84e16d4425f4d4`),
`0x462D30` (`ev_f2f4431afc3ee1da42fbe1f5bd88a52924b1b52d718f67fb98cf133d0635885a`),
`0x462D90` (`ev_2d0096969d13bd67200d3ea8d5ba62679aea1068dd3fa19b99a9481237a22225`),
`0x468090` (`ev_b466aa22faf505aca957c810ba12a54714f045b5f4b12bf6b0055211483b2d98`),
`0x468270` (`ev_de0ec7660bac9a8b0e9493281c0c521b099e40ef0ff3b1994920e08e1a341b64`),
`0x469300` (`ev_48b9e3596cb4dfa50d1ba5991c3ebb11ac926bf35e4073d6b395ec00385f1e90`),
and `0x469350` (`ev_1ca4fe97607c76902eab28afb2b0b7376bfe322f74243d1aa4747465b7b74e28`).

Tag 0's outer effect management is also projected in
`plan_scene_car_effect_tag_zero_handler`: `0x4692A0` compares the DWORD at
payload `+0x18` against the `+0x04` key in all 32 slots at `0x540B70..0x540BEC`.
Any match suppresses generation but does not stop the scan. With no match, the
global cursor increments and wraps with `& 31`; the selected slot then enters
`0x468AF0`. Null pool entries and an absent selected object are reported as
faults, while an incomplete 32-entry observation is kept distinct.
`plan_scene_car_effect_particle_generator` now covers `0x468AF0`'s
effect-key/origin writes, speed clamp, horizontal-motion gate, fifteen control
points and sentinel words, fifteen cloud particles with three random draws
each, and its lifetime/mode fields. The sample inputs are the integers
returned by the `0x468E80` random-range helper; missing observations preserve
the already projected control points and any completed particle writes.
FDIV correction uses the shared approximation and x87 rounding remains
approximate. Evidence: `0x4692A0` bytes
(`ev_c924832e2207c9f88dace42f6fb0d9e4d8cad232dfb40f54a137d7d09f5e78d9`),
generator `0x468AF0` (`ev_fdf72c8842997f85c97fed158fae93ba441b5016c07baf6bd8ccb813475b0a18`),
random-range helper `0x468E80` (`ev_fe4d25653a3f36764032e8f7d0a541610c8fa4c31b7b1f12e17a60771b1500ca`),
and constants at `0x4C954C..0x4C9598` (`ev_52957d613ceed6a1be2bae4856668b7b9a4ae80dfcf7b634f96b9a413b694cad`).

The wrapper measures the distance from object position `+0x28..+0x30` to the
camera globals `0x509B58..0x509B60` using `0x448C10`; the following scale
constant at `0x4D11D4` is exactly 1.0. It initializes indices `+0x510` and
`+0x514` to 4, then scans `*int(+0x28C)` records in the array at `+0x288`,
with a 12-byte stride. The first record whose leading float is strictly above
the distance supplies both indices; if none is found they remain 4. It adds
`DAT_0053C9C8` to `+0x510` and truncating `DAT_0053C9C8 / 2` to `+0x514`.
After virtual `+0x28` returns, a nonzero return value raises both indices to
at least 2. The observed vtable target `0x4668D0` returns false, so this clamp
does not occur for this scene-car class. Finally, each index at or above the
record count is replaced with `count - 1`; with count zero this yields -1 for
the nonnegative defaults. `select_scene_car_dispatch_indices` now models the
observed false-return target directly. The portable distance calculation
uses ordinary float operations and does not reproduce the helper's x87
extended intermediates.

## State and input block

The analyzed body at `0x465800` accepts a pointer to a float block. It first
copies 12 dwords from input `+4` to scene-object `+4` through `0x463570`.
Then it compares the selected ordinal returned by `0x45C1F0` with the object's
ordinal at `+0x1C8`. If they match, selected state from `0x45C200` maps numeric
0 to object state 1 at `+0x83C`, numeric 2 to state 2, and other values to
state 0. A nonmatching ordinal also writes state 0. The getters return
`DAT_004DC1A0` and `DAT_0053A4B4`, respectively; their user-facing meanings
are unresolved.

The callback compares current state `+0x83C` with previous state `+0x840`.
Depending on the transition it calls `0x447F20`, clears eight dwords beginning
at `+0x19E`, and compares sixteen incoming 16-bit values at input `+0x162`
against sixteen saved values at object `+0x19E`. For each word it stores the
rising-edge mask `incoming & ~saved`; any falling edge `saved & ~incoming`
sets the first event flag, and any rising edge sets the second. A state reset
first zeroes all sixteen saved words, so every set incoming bit is a rising
edge; reset also sets both event flags even if no source is active. When the
first event flag is set, the body calls `0x447E50` with IDs from object `+0x30C`
and `+0x308` and rectangle `{0, 0, 1, 1}`, followed by one
`0x447EE0(+0x308)` refresh. When the rising-event flag is set, the body walks
the sixteen words and each word's sixteen bits in ascending order, calls
`0x447E50` for each set rising bit, then calls `0x447EE0(+0x308)` once. The
per-bit IDs are from `+0x310` and `+0x308`. Its rectangle is
`{bit*S, word*S, (bit+1)*S, (word+1)*S}`, where `S` is the float at
`0x4C940C` (`0.0625`, raw bits `0x3D800000`). The assembly stack stores at
`0x004659E3..0x004659F1` establish the rectangle argument order. A reset can
therefore produce the special event and refresh first, then rising-bit events
and a second refresh. The body then copies
99 consecutive floats (396 bytes) from the input to object `+0x3C` and writes
current state to previous-state field `+0x840`.

The event wrappers' effects are also partially recovered. `0x447E50` calls
`0x453D90`, which ignores either ID when it is `-1`; otherwise it indexes two
records at stride `0x28` and passes their resource handles to `0x459D60`.
`0x447EE0` calls `0x453E10`, which only acts for a non-`-1` ID whose active
byte is set, then passes two row fields to `0x459CA0`. `0x447F20` calls
`0x4540A0`, which releases an active row's resource and clears its handle and
active byte. The surrounding `0x415090` and `0x4150A0` calls are empty
functions. The resource operations and ID meanings remain unresolved.

## Four wheel transforms

The callback loops four times. The source entry advances by 14 floats
(`0x38` bytes), the matrix destination by 12 floats (`0x30` bytes), and an
auxiliary object pointer by `0x2C` bytes. For entries zero and one, it adds
object field `+0x7AC` to the source vertical component; for entries two and
three it uses `+0x7B0`. The resulting position components are:

```text
x = object[+0x28] + object[+0x10]*local_y + object[+0x04]*local_x + object[+0x1C]*local_z
y = object[+0x2C] + object[+0x20]*local_z + object[+0x08]*local_x + object[+0x14]*local_y
z = object[+0x30] + object[+0x24]*local_z + object[+0x0C]*local_y + object[+0x18]*local_x
```

Each output record stores position delta against its previous position, then
writes the current position. The first nine destination floats are copied from
the scene object's basis; the resulting twelve-float transform is also copied
to a parallel block `+0xC0` later in the object. Although the callback builds
temporary rotations with `0x402D10`, `0x402D50`, and `0x402D90`, its
`0x402DD0` calls alias output
with an input; that helper explicitly does nothing for those aliases. The
observed wheel basis therefore remains the copied parent basis in this path.
When the outer scene, world, and wheel gates pass, `0x465800` chooses one of
three auxiliary-object paths. For each wheel, record field `+0x34` bit 0 is
the operational update-versus-release gate: set calls `0x462600`; clear calls
`0x462940` and retires a non-null resource. After the world gate and valid
ordinal pass, a false effect gate uses sibling release `0x462960`; when that
gate passes, the `0x425830` suppression gate can return without changing the
resource. This establishes the bit's
control-flow role; its higher-level vehicle meaning is still unknown. The
`0x462600` control object stores current
start/end points at `+0x00` and `+0x0C`, a resource pointer at `+0x18`, timer
at `+0x20`, allocator size at `+0x24`, and active byte at `+0x28`. Its allocated
resource record contains four three-float position triplets at `+0x00`, `+0x0C`,
`+0x18`, and `+0x24`. With no resource, it marks the transition active, clears
the timer, copies the supplied target into both endpoint pairs, and attempts
allocation through `0x462590`; with a supplied pool its success is computed,
otherwise success is an explicit observation. The allocator writes resource
`+0x30 = 1`. It scans the manager count at `+0x08` from its cursor at `+0x0C`,
wrapping across `0x38`-byte
slots. It reuses the first slot whose `+0x30` state is not 1, sets that state
to 1, stores the requested size at `+0x34`, and advances the cursor to the next
slot. An empty or fully occupied pool returns null without changing the
cursor. On later calls it increments the timer by the observed
`0.033333335` (`0x4C9194`). Once the timer exceeds bit pattern `0x3E124925`
(`0.1428571`) and squared distance from the saved first point to the supplied
target exceeds `0.48999998` (`0x4C919C`), it copies the resource's last target
pair back to the state, calls release helper `0x4625F0` (which writes resource
`+0x30 = 2`), and clears the resource pointer. If a resource remains, it
computes the cross product of the supplied
orientation vector and target displacement, ignores magnitudes at or below
`1.1920929e-7` (`0x4C91A0`), and scales the normalized result by `0.1`
(`0x4C9140`). The active transition initializes its two position pairs around
the resource's current point; every valid update writes target-minus/plus-offset
pairs into the resource. The division uses the shared common finite-normal
VC98 FDIV approximation when the explicit `0x4E9754` input is set; subnormal
handling and exact x87 rounding remain unresolved. The `0x462940` and
`0x462960` paths both release a non-null resource via `0x4625F0` and clear the
pointer. Release marks its pool slot state 2. The frame method draws every
nonzero slot, so a retired state-2 slot remains drawable; the allocator may
reuse it because it only protects state 1, and reset clears state 2 while
preserving state 1. State 0 is free/invisible, state 1 is live/protected, and
state 2 is retired/reusable. The portable model names these three observed
values. `0x465800`'s gates and state transitions are composed; the semantic
meaning of the sixteen input effect masks remains unresolved. Evidence: release
wrappers `ev_41e53c6d221a61e5ebeb1580c5fe96a293dcc746e4f3db0df7628cecc6ab378a`
and `ev_f2e2c15f03e097064d22bdcbc57f23a98e99198f5485aad8e3c4cb7ba3305bae`,
plus state writer `0x4625F0` `ev_a1960c8f34dd0ab936da16b6656d685abc6ddc4ba7912565b7d5869ee45621a3`.
The full current-session evidence bundle is preserved at
`metadata/scene_wheel_lifecycle.evidence.json`.

The pool manager constructor at `0x4622F0` requests `count * 0x38` bytes,
initializes each slot, resolves the `skid_tex` handle, and sets its cursor to
zero. `0x462380` resets that cursor and changes every slot state other than 1
to zero, preserving state-1 slots. The portable manager initializer accepts
the allocated slot span and resolved texture handle as observations; it clears
only each slot's `+0x30` state, leaving position bytes and `+0x34` untouched.
`reset_scene_wheel_resource_allocator` reproduces the explicit reset loop.
Destructor `0x4623B0` releases the texture handle, frees the slot array, then
calls the base destructor; `destroy_scene_wheel_resource_allocator` records
those calls in order and clears its local manager view. Destructor evidence is
`ev_b36453554f2c9a86f047e709aaa2623c5a68401b5c5b3941a3eaebf19a2aa628`.
Constructor and reset evidence are `ev_b70eb7f4fe82144e87ef91286f9c5eb5810bf511a36c9d82a0f6cfbb7529a13f` and
`ev_55c21a5ed04271fef9a5a9d8cae98e2300ab109199cee2c281ac0240308ea288`,
preserved with related dossiers in `metadata/scene_resource_manager.evidence.json`.
The upstream constructor at `0x463920` creates the manager when gated mode
`0x45BAC0` is nonzero. It uses one wheel group for modes below 2 and four
otherwise, allocating 16 slots per group or 40 per group when selector
`0x45C230` matches the selected PCAR ordinal. The PCAR constructor path in
`0x45CAE0` writes that selector from the current car ordinal; it starts at
`-1` during configuration initialization. The mode comes from the `skids`
setting parsed by `0x45B6B0`; it is behind feature gate `0x509B74`.
`scene_wheel_resource_pool_slot_count` projects that exact count. The mode and
selector accessors resolve to globals `0x53A4B0` and `0x4DC19C`, respectively.
Evidence: constructor `ev_761442b24e1e0ad9e1e15d7b5e33174a7144d92be40f035876d1cbada57a6f09`,
mode accessor `ev_bda172315a0c06d53b8059ef56da4bb6134f01b40c22f20a79cbba25f312d9f1`,
configuration loader `ev_84ee40aeb94d384d45130b03a0af4fd328d278a495bbc31bb03b8a3e3b884502`,
and selector accessor `ev_4d6e076f850a8b75b601d67a94b0b13e510467f7c719511e4d64239aaf7318d0`.

The manager frame method at `0x4623F0..0x462574` is a linear-disassembly entry
that Ghidra did not register as a function. The constructor at `0x4622F0`
installs vtable `0x4C9270`; its sixth 32-bit entry, at `0x4C9284` (vtable
offset `+0x14`), contains `0x4623F0`. The destructor at `0x4623B0` also
references the same table. Ghidra's typed incoming reference from `0x4C9284`
and the shipped image's DWORD value therefore establish this block as a
virtual method despite the missing FunctionManager entry. This identifies its
dispatch slot. Generic renderer `0x45BEB0` invokes slot `+0x14` on registered
scene objects; that caller path is detailed below. The method checks
`0x45C380`; false calls
the reset routine and returns. True brackets the slot walk with calls to
`0x513A0(8)`, `0x51610(0x100)`, `0x448E30(texture_handle)`, `0x448EF0()`,
`0x513A0(0x100)`, and `0x51610(8)`. Each nonzero slot state, including state 2,
produces one indexed draw. Its four 0x20-byte vertex records copy the four
position triplets, use UV pairs `(0,0),(1,0),(0,1),(1,1)`, and set vertex words
at offsets `+0x10` and `+0x14` to `0xFFFFFFFF`. The word at vertex `+0x0C` is
not written in this block and remains a raw input observation. The index list
is `{0,1,2,1,3,2}`. `0x448ED0` forwards the vertices, vertex count 4, indices,
and index count 6 to the device wrapper; `0x452700` calls the device vtable
slot `+0x78` with primitive type 4, vertex type 2, and flags 8. The portable
frame plan preserves those raw arguments and order without submitting graphics.
Wrapper evidence is `ev_fd752a426d357596f80fec5d14eecf72530238d9cc9ba11a13bc177b359cdd0d`
and `ev_3bc4747a9027358eddde04b53dcc349045ec7c294a3eb68c08995f1f37fa069f`.
The two wrapper dossiers are preserved in `metadata/scene_resource_draw.evidence.json`.

The analyzed direct callees of this scene callback do not include `0x431150`,
so this path is established as pose, transition, and wheel-transform update.
The separately dispatched wheel-resource rendering path is traced below.

The manager frame method is reached through the generic scene-object dispatch
loop. Constructor `0x463920` creates each wheel manager with `0x4622F0` and
registers the resulting pointer through `0x45C2E0`. That registration appends
the object to the global list at `0x539430` and increments its count at
`0x53A4C0`. Renderer `0x45BEB0` walks that list; for each object it calls
virtual slot `+0x10`, checks the global mode gate, calls slot `+0x0C`, then
calls slot `+0x14` when both conditions permit. Since this manager's vtable
slot `+0x14` is `0x4623F0`, this connects construction, registration, and
frame rendering. The manager's frame gate at `0x45C380` still provides its
internal reset-versus-draw branch. Evidence: manager vtable `+0x14`
`ev_1b91d34c52b1d19ee056410c599b67f72ae9ac2381bb2857090aa3ddda9ff43f`,
dispatcher `0x45BEB0` `ev_1ffdfd37f67d2227d0d9e780f4db2f1b5a10c67c802e855cc30aa7dc09a7342e`,
registration `0x45C2E0` `ev_f7d4a1c00acf8a61134b349e6cac89cebf193a462d77cca3b1c54eadb2070def`,
and scene constructor `0x463920`
`ev_761442b24e1e0ad9e1e15d7b5e33174a7144d92be40f035876d1cbada57a6f09`.

## Portable model

`include/viper/scene_car_update.hpp` and `src/scene_car_update.cpp` encode the
verified numeric state classifier, exact 99-float copy, and wheel basis,
position, delta, mirrored transform, sixteen-word edge masks, reset behavior,
rising-bit count, ordered resource-event/refresh call plan, and state update
for the `0x462600` auxiliary path and the `0x4622F0`/`0x462380`/`0x4623B0`
manager initialization, reset, and cleanup, plus the `0x4623F0` manager frame
projection and constructor-driven pool sizing. It models the `0x462590`
circular allocator when a slot pool is supplied; otherwise allocation success
remains an explicit observation.
Its normalized divisions use the shared approximate compatibility
routine when requested. Other x87 rounding remains approximate. The event
loop and special branch are visible in assembly `0x00465868..0x00465A1D`;
analyzed evidence is `ev_9bc59c0c0669289e8d06530e19a0d8bd4088c0dab36f2066e9d10870d81aacc6`.
The helper dossier for `0x462600` is `ev_16ef11a026ee34982b250f91f5e205dce42af00d23f03c529c4bc366e81cdb95`;
allocator evidence is preserved in `metadata/scene_auxiliary.evidence.json`
(`ev_713bb6ee2b9dcfac640fa7261907bbb2f18965a88d66912fb5dc113dffcf3e39`).
