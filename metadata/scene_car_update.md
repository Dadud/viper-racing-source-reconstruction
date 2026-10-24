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

The tag 1 callback at `0x468450` now has a separate bounded projection in
`plan_scene_car_effect_tag_one_handler`. The `0x45BAE0` context gate requires
both globals at `0x509B74` and `0x53A4C4` to be nonzero. It examines the slot
selected by the current cursor in the 32-entry, `0x1C`-byte pool: timestamps
at or newer than `now`, or at least 0.5 seconds old, are reusable; a slot
with `timestamp < now < timestamp + 0.5` is retained and causes no cursor or
record writes. On reuse, the cursor advances and wraps, velocity is scaled by
0.5 with `0.1 * speed` added to y, the origin and current time are stored, and
three `0x468420` draws perturb velocity in x/z/y order using the scaled x
velocity. Missing slot-time or random observations are explicit statuses;
x87 intermediate rounding and exact RNG values remain approximations/inputs.
Evidence: callback bytes `0x468450..0x46859F`
(`ev_260c6486a94a1e7634d421d893fa63f67d2bea3b4bee5390d42c336909377fa6`),
registration bytes at `0x468270` (`ev_c35340ae650d7672fa05295f87d9d5e77cadf42dbb61f60c6a4749983fd5b494`),
context gate `0x45BAE0` (`ev_f826a09a5e60377e67248315096dc8878299c6c63f8ce1a1dc6641a129eb912f`),
random-range helper `0x468420` (`ev_878fa81da8d63d96b130a5a8c9968c52402b863cc2430660810a4bc2358dfeb2`),
RNG converter `0x4193F0` (`ev_6b1334751a6337d3165d366b0c495d29963640f958a57149aa3dfa596205c872`),
and constants at `0x4C94F0` (`ev_ab6e2845b1b14c8a15f96a8fe418220c158b4387c355ae9b1b11b0605b93df83`).

The tag 2 callback at `0x468290` is now projected by
`plan_scene_car_effect_tag_two_handler`. When the shared `0x45BAE0` gate is
open, it calls `0x461AC0` for the current one-of-sixteen context and advances
that context cursor modulo 16 before checking the emission timer. The helper's
child-object call receives the manager's existing `+0xC4` position, a zero
velocity vector, and mode 0. This happens before the timer check and before a
new payload origin overwrites that manager vector. The actual helper writes
are available through `plan_scene_car_effect_child_initializer`; its final
registration call remains external. Emission proceeds only when
`now > previous_emit_time + 0.5`; it then
copies the payload origin and current time, clamps `payload[+0x1C] * 0.01` to
[5, 7], and computes vertical scale `payload_velocity_y * 0.6 + clamped_scale`.
It produces sixteen points: x/z bases are payload velocity x/z scaled by 0.6,
then receive jitter in [-0.6, 0.6] times the vertical value; y is a multiplier
in [0.8, 1.2] times that vertical value. The x87 write pipeline stores
each y component at the start of the next loop iteration (and flushes the last
one after the loop); component masks preserve partial writes when supplied RNG
observations run out. This bounded projection does not model `0x461AC0`'s
randomized child-object fields inline; those fields have a separate projection
in `plan_scene_car_effect_child_initializer`, and `0x461650`'s quad geometry
is projected by `plan_scene_car_billboard_quad`. The final common renderer
queue/device mutations remain external. Evidence:
callback bytes `0x468290..0x46841F`
(`ev_c2cac3248c1a63da85831ae40530abbb100df6ab94e47f402974dc32797378dd`),
constants at `0x4C94B4..0x4C94FB`
(`ev_1c3604802daed4cbcc7c909199173a45a6ab733a56facfc417b296c22fcc258a`),
context gate `0x45BAE0` (`ev_f826a09a5e60377e67248315096dc8878299c6c63f8ce1a1dc6641a129eb912f`),
initializer `0x461AC0` (`ev_9d509939ffb7af7f87606397c1ed9e1e7eb001c378380eff367128b96553b858`),
and random-range helper `0x468420` (`ev_0adb01115cbb64610af2ba80c7adc608b25c8da6c1dbf6e36d7fd36a5b61a9da`).

`plan_scene_car_effect_child_initializer` projects the `0x461AC0` helper used
by tag 2 and the wheel-effect path. It sets the active/mode bytes, copies the
position with `+0.2` on y, samples a `[0.75, 1.75]` lifetime, derives current
time from the observed `0x425790` counter times `0.016`, clears `+0x54`, and
forms the three velocity components from input velocity times `0.3` plus
three ordered jitter samples (`[-0.01, 0.01]`, `[0.01, 0.03]`, then
`[-0.01, 0.01]`); the first/third samples feed z/x respectively, while the
middle sample feeds y. `0x461650` is recorded as a call with the
observed opaque arguments and scale 1; its quad writes are projected
separately, while common renderer queue/device effects remain external.
Evidence: `0x461AC0`
(`ev_9d509939ffb7af7f87606397c1ed9e1e7eb001c378380eff367128b96553b858`),
random-range helper `0x461BD0`
(`ev_62fbb1add8c5be568ef2f3f807af41519711a3cbbc782f0c1db3f13418508838`),
clock helper `0x425790` (`ev_4394f76cfa93de76984db3bbd98bb3696a7ab8b4159da55951abcbc5f7aabaef`),
and registration routine `0x461650`
(`ev_2ffba83fd83f8daaf91a2409f11f5d18a0d3696efd9e7ebe3f8e95a4e7b42721`).

`plan_scene_car_billboard_quad` projects `0x461650`'s four vertex records and
atlas UVs. The `__ftol`-converted frame must be in [0, 7]; the tag 2 child
initializer passes frame/size 1.0, yielding half extent 0.85. U coordinates
select one of eight horizontal cells with 1/128 gutters; V uses base values
15/128 and 1/128, offset by 1/8 for nonzero mode. Each vertex has a 0x20-byte
stride, with x/y/u/v at offsets 0, 4, 0x18, and 0x1C. The routine forwards
the two opaque caller arguments to `0x44F750` with flags 7; common renderer
queue processing at `0x44F9C0` remains outside this geometry plan. Evidence:
`0x461650` (`ev_2ffba83fd83f8daaf91a2409f11f5d18a0d3696efd9e7ebe3f8e95a4e7b42721`),
`0x44F750` (`ev_bd4f35ac48c918544966d4212753f4f620c06c42d4a8180b73d1b0bee859b852`),
and `0x44F9C0` (`ev_b6b2346770ffedb63c33c61d359265f9344b7c36431aaee691df3a0bdef5b9a6`).

`plan_scene_renderer_refresh` projects the shared `0x44F9C0` update path.
The routine always stores its descriptor-block pointer at renderer state
`+0x10` and its byte argument at `+0x15`; flags independently gate the other
work. Flag 1 walks each descriptor's signed vertex range, reads 8-byte source
rows, subtracts the descriptor center from all three signed-short coordinates,
and writes 6-byte destination rows. Flag 2 walks the current binding list in
descriptor order, reuses or allocates 0x2C-byte nodes, resolves names against
the renderer resource table, projects resource load/release and handle-cell
effects, writes the four observed node fields, then releases and frees leftover
nodes. Names are compared case-insensitively and stored names are bounded to
the observed 16-byte field. Allocation and resource-load return values are
explicit input observations; incomplete spans stop or limit the projection
instead of fabricating memory contents. The flag-4 reset case is the already
documented unconditional pointer/byte stores with no flag-1/2 work. This model
describes CPU-side queue state and resource lifecycle; it does not reconstruct
the device/backend renderer. Evidence: `0x44F9C0`
(`ev_b6b2346770ffedb63c33c61d359265f9344b7c36431aaee691df3a0bdef5b9a6`),
resource resolution `0x44FBC0`
(`ev_3f709d251610dddb4f5078f98a2c8c5ad682d359afdab0bd5e97c043a3d75ccd`),
identity wrapper `0x450D00`
(`ev_28c9ed2c5e137c1681b99f606fcb80ac4aef97bd1d1549ecef402c7a446f70a`),
load/release wrappers `0x447D10` (`ev_cbb3100486c3b2dffe144d5620aa439003e4421e2ad2122abbabd07456ee7eda`)
and `0x447D50` (`ev_db25c0cd3b56e43beb331eb6b94a5ce9288572ad01be3f4df3938f8d5ece494e`),
and pool allocation/free `0x419830`
(`ev_0dc5d78cd998d9b70f89abfbb69efbb990b83a70dfc53827510fe782daaa6add`)
and `0x419870` (`ev_d113bf8ac577ac505673e2d0a5faee32c788c9493e67f3c92fed386be43f422a`).

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
and index count 6 to `0x452700`; the device wrapper calls vtable slot `+0x78`
with primitive type 4, vertex type 2, and flags 8. Each portable draw operation
now records both wrapper addresses, the device vtable offset, all fixed draw
arguments, and that the native call's return is ignored. It preserves the raw
vertex/index data and call order; actual device execution and rasterization
remain external.
Wrapper evidence is `ev_fd752a426d357596f80fec5d14eecf72530238d9cc9ba11a13bc177b359cdd0d`
and `ev_3bc4747a9027358eddde04b53dcc349045ec7c294a3eb68c08995f1f37fa069f`.
The two wrapper dossiers are preserved in `metadata/scene_resource_draw.evidence.json`.

Texture setup is projected beyond the manager's `0x448E30` call. It builds a
3x4 identity matrix, expands it to a 4x4 identity, and installs texture
transform state 1 through `0x450930` and `0x4576C0`. `0x4525F0` caches the
requested texture handle and has a same-handle/dirty fast path; otherwise it
conditionally ensures resource access (`0x454030`), checks surface
availability (`0x4540E0`), or disables texture state through `0x458BC0` for
handle -1. Every state-changing path ends in the shared render-state sync at
`0x457AF0`; a same-handle clean cache returns without synchronization. The
portable plan records those helper calls and their supplied observations,
leaving the wide synchronization routine as an explicit external action.
When observations are absent, the plan marks the affected texture-enable
result unknown instead of inventing one. Evidence: `0x448E30`
(`ev_5da79e3cff7f863d870c48566dbe08f9a3f61c0b09172eda531589958fda3697`),
`0x4525F0` (`ev_e95b1e27ff8cfc4cac81db0ccf522627f8e97a766f2bcfdd4195ebf6388cd25b`),
`0x450930` (`ev_6eb86044cae5f35952468bbfcb9e0b37918891d26aec828fefb87fe254972345`),
`0x4576C0` (`ev_9e6567552ca91fff6a31de778c4eaf00b555786e24d2c02517d898dd726f0e31`),
`0x458BC0` (`ev_3f3f2bd8815c0b4e866d99d3d2664fe6e74bbada0fe5591eae3d2a6a67145519`),
`0x454030` (`ev_a2a6baf96c22d225e9fa69666420e0c92f37ad873f9fc5df698caa35f0640a95`),
`0x4540E0` (`ev_cf04f42b829b5299ec30d322202c94128ff598a559bb372e0cee19e0403a5c4b`),
and state synchronization `0x457AF0`
(`ev_36bffbe7c16da85c8d5227529f165fda21d05a1c9de08bc856eac49d2d9d8622`).

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

The generic render observation can now retain the resolved `+0x14` target.
`observe_scene_wheel_resource_manager` records `0x4623F0`, while leaving the
generic `+0x10` and `+0x0C` return values as observations; when the generic
dispatcher reaches `+0x14`, its action carries that native target address.
This links the renderer's object walk to the already modeled wheel-manager
frame method without inventing behavior for the manager's earlier gates.

### Scene-car virtual `+0x18` callout

`plan_scene_car_callout` projects the scene-car entry at `0x4648A0`, reached
from submitter `0x45C090`. It suppresses the selected car, then applies the
strict squared-distance comparison against the threshold at `0x4C9384`. The
native x87 status-word test checks C0; unordered distances therefore continue
to the projection path. The portable squared-distance arithmetic uses double
intermediates and remains an approximation to x87 extended precision. Passing
the distance gate calls `0x448CD0`; its AL result is the following visibility
gate. The wrapper installs the identity transform and delegates the world
position conversion to `0x452350`/device transform state.

After projection succeeds, the callback sets its text context through
`0x449900` and looks up the scene object's ordinal through `0x45DFB0`. Gate
`0x49BE40` enables the primary formatted string using record offsets `+5` and
`+0x140`, format source `0x4DCDA4`, and draw helper `0x4D710`. The later
`0x40D250` gate is true only when both `0x4D10FC` and `0x4ECC98` are nonzero;
record byte `+0x32` must also be nonzero before the callback calls `0x44D0C0`
with object field `+0x7B4` and then draws that record string through `0x4D710`.
The portable plan preserves these ordered call gates and record offsets. Device
projection results, formatted text, and `__ftol` pixel coordinates remain
inputs because their results depend on runtime transform/device and CRT state.
Evidence: vtable/submit path and raw callback instructions in
`metadata/simulation_loop.md` and `disasm/engine_.text.asm`; helper dossiers
`ev_863e3d62425fc238f2f7d05582aadddfb0a640870bf31a34c16400574dbc1d0b`,
`ev_b66f1af6bc9ebabb616d42d22c6e9cc544095105e212c61780e913c9b146754f`,
`ev_0d1cac05b94aa5dc4c850cd384bb6e6cb97abe2a8a92e0f36bab0295f5a90e28`,
`ev_a66356af194e82eb5f16751b35cd3e4030ff0e736b2f0b30de2f2e8efeb0f482`,
`ev_e4f38a37529c74fb1934ce169ee89e3c9c8c18cf28d87e00f91d02081272917e`
(`0x44D710`), and `ev_530a782028b247007aab203e8ab52d18b606f9b47ea2ac4fffd1c8904092d1ba`
(`0x44D6A0`). The callout plan also applies the text helper's exact layout:
start x is the supplied pixel x minus four times the NUL-terminated string
length, y is pixel y minus four, and each built-in glyph advances eight pixels.
The `0x44D6A0` helper replaces control bytes below space with `#` before
dispatching glyphs to `0x44D780`; pixel rasterization is covered by the shared
built-in-font projection.

`raster_scene_car_callout` consumes the callback operation plan and sends the
primary and secondary strings through `raster_scene_dashboard_builtin_font`
using the shared color at `0x53F194` and renderer observation. It preserves the
resource operation between those text segments as an ordered zero-write marker,
so a caller can insert the separately decoded `+0x7B4` image/resource writes
without changing text ordering. It reports a text-input mismatch when the
provided NUL-terminated string lengths disagree with the callout's centering
observations. Native surface ownership and resolution of the object resource
pointer remain inputs.

`plan_scene_renderer_dispatch` projects the enclosing generic render frame at
`0x45BEB0`. The profile begin/end wrappers bracket the invocation. The active
draw gate is `renderer_mode > 0` with renderer state in `{0,1,2}`. When active,
the plan records the ordered graphics-state setup, the initial viewport call,
camera transform install, scene-state configuration, optional camera update
for mode greater than 1, registered-object walk, state restoration, original
camera transform restore, and full-window viewport restore. The camera matrix
uses the exact observed sign changes, cross-product component expressions,
and four-unit translation derived from `0x45BEB0`; floating-point evaluation
is a portable approximation to the original x87 path. Each observed object
receives virtual `+0x10` then `+0x0C`; `+0x14` runs only when `+0x0C` is true
and either `+0x10` is false or renderer mode exceeds 2. Missing list entries
report bounded observation exhaustion. A null object stops the projection at
the dereference point and leaves the transformed matrix unrestored, matching
the absence of a normal return path in that case. Device calls and virtual
return bytes remain external observations. Evidence: `0x45BEB0`
(`ev_09fe993aef1d6fb03f1196700f493dcfc2bd60404c0a3506d8dcfb99f55ee85d`),
render gate `0x45DAB0`
(`ev_81401a8af9e0eeb18fec9ad96ca384d0add788c835e9bcbb944a549d8373ce7f`),
state setup/restore `0x460210` (`ev_d209fce936e7e4639efc30f7a012a0b5598524aaeb2f8dc5fcabeeea14188f8f`)
and `0x4602A0` (`ev_430ec4dc163b2538c19071531ec8869c41b65b0481f4e8366ec0bc1e90f11c74`),
transform install `0x448940`
(`ev_60be95662910eac8c520015118ac622f7657e4ccd7acabdf920d343770908e48`),
and viewport `0x4487A0`
(`ev_da14982bed84a04d197834f48480fcfa98f9ca954740c1beefe8fc60a21f5dd0`),
state wrappers `0x448450` (`ev_fe4aee2684fc5e81c01de1bed3b45cce6e0e04511dee88fa929b505b1cda83c7`)
and `0x448440` (`ev_54c964f82dd752b84a4b4f98553b21e889bdd8b817904462870823ea14567e14`),
and mode-dependent update thunk `0x460360`
(`ev_0cb578d5e310c6ad56d2fe4d48ec2c265d75670586589316993f1931d32335af`).

The `0x448940` transform install is now followed through its device wrapper.
`0x454CA0` captures the transform's position and right-axis triples, clears
eight cached flags, and decrements each positive cache reference count.
`0x4576E0` selects transform state 2; `0x458C20` forwards that state and the
constructed 4x4 view matrix to device vtable offset `+0x68`. A nonzero HRESULT
calls the logger with `SetTransform` and the error value. The scene dispatcher
projects both the camera-relative view install and its original-matrix restore,
including separate HRESULT observations and cache effects. Matrix contents
follow the recovered row/column arrangement and position dot products; x87
rounding remains a possible last-bit difference. Evidence: `0x454CA0`
(`ev_534bd4dcc4c2e026f2da74702aca13f8cab84dfaba5e00a7986c2fc791bda683`),
transform-state wrapper `0x4576E0`
(`ev_4d69b0169b5f39b3d49a5fdbc67e846506deb88a2526bc65b0d4268bf136a914`),
device setter `0x458C20`
(`ev_7314a537d80dfad61c4893f146b1a7856a6ffafc67b9ba58cb787ecf70b763ea`),
and error logger `0x452770`
(`ev_ce89dcbcb43d460ff14a562e8771dd78dd13d069bc274061800da0423b1d44e8`).

The viewport wrapper `0x4487A0` is also expanded through its device calls.
It stores `(x,y,width,height)` and center coordinates, then `0x452100` builds
the observed 0x2C-byte viewport record and invokes device vtable `+0x44` via
`0x458740`. The record preserves the binary's exact scalar slots; the aspect
ratio uses `height / width`, with an optional supplied adjusted-FDIV result
when `DAT_004E9754` selects that path. Without that observation the model marks
the ratio as approximate. A failed viewport HRESULT logs `set_viewport`, but
does not suppress the following clear call. `0x4522C0` then reaches device
vtable `+0x30` through `0x458800` with one rectangle `(x,y,x+width,y+height)`:
flags 3 on the inset viewport call and flags 2 on the full-window restore. A
failed clear HRESULT logs `clearing_zbuffer`. The renderer
plan records both the inset operation and restore, including separate HRESULT
observations. Evidence: `0x4485D0`
(`ev_5d22efbec104b4cf539dd13ad87d0559015dec8c0d6d06e35d51a8fb7dd4481a`),
`0x452100` (`ev_b0be56d103c74e299a5c35a0e67c724d582784558fbd9d0bd87a85cf39e3ef95`),
`0x458740` (`ev_5ccbd1fe9dd998cbb0f6c2fd24e3b39b848c08f432524436a3d371c0a071e7ac`),
`0x4522C0` (`ev_c6c2785f5b203480d375f127aa69ef5d84e8ebf88971bdee0963621a08069451`),
and `0x458800` (`ev_3d45267f1ff70581eb74195da73598239192f2df640d12d185c9698c90d9720a`).

### Four-wheel callback composition

`plan_scene_car_wheel_frame` composes the four loop iterations from `0x465800`
in source order. For each index 0 through 3 it selects the corresponding
front/rear vertical offset through `transform_scene_car_wheel`, produces the
wheel transform, then evaluates that wheel's outer gates and updates or retires
its auxiliary resource state through `dispatch_scene_wheel_auxiliary`. The
caller supplies per-wheel records and the four prior auxiliary states; the plan
returns all four transforms, dispatch outcomes, and resulting states. This
captures sequencing and state carry without claiming the unknown semantics of
the input flags or the native helper side effects. Evidence is the complete
`0x465800` body (`ev_9bc59c0c0669289e8d06530e19a0d8bd4088c0dab36f2066e9d10870d81aacc6`)
and its auxiliary helper analyses retained elsewhere in this document.

## Portable model

`include/viper/scene_car_update.hpp` and `src/scene_car_update.cpp` encode the
verified numeric state classifier, exact 99-float copy, and wheel basis,
position, delta, mirrored transform, sixteen-word edge masks, reset behavior,
rising-bit count, ordered resource-event/refresh call plan, and state update
for the `0x462600` auxiliary path and the `0x4622F0`/`0x462380`/`0x4623B0`
manager initialization, reset, and cleanup, plus the `0x4623F0` manager frame
projection and constructor-driven pool sizing. It models the `0x462590`
circular allocator when a slot pool is supplied; otherwise allocation success
remains an explicit observation. `plan_scene_car_wheel_frame` now combines the
four transform and auxiliary-dispatch iterations in the observed order while
carrying each wheel's resulting resource state forward.
Its normalized divisions use the shared approximate compatibility
routine when requested. Other x87 rounding remains approximate. The event
loop and special branch are visible in assembly `0x00465868..0x00465A1D`;
analyzed evidence is `ev_9bc59c0c0669289e8d06530e19a0d8bd4088c0dab36f2066e9d10870d81aacc6`.
The helper dossier for `0x462600` is `ev_16ef11a026ee34982b250f91f5e205dce42af00d23f03c529c4bc366e81cdb95`;
allocator evidence is preserved in `metadata/scene_auxiliary.evidence.json`
(`ev_713bb6ee2b9dcfac640fa7261907bbb2f18965a88d66912fb5dc113dffcf3e39`).

The formerly opaque `0x457AF0` synchronization is now projected from the
observed 24-byte desired and cached records. When gate `0x44E400` is closed it
returns without changing the cache. When open it emits the ordered helper
calls selected by changed byte fields and little-endian integer fields at
offsets `+0x04` and `+0x10`; it also records clearing the dirty flag and copies
the full 24-byte snapshot only when any state changed. The action list keeps
the binary's helper addresses and arguments, including the mode mappings,
secondary blend factor, alpha-reference gate, and color-write side-effect
gate. Unsupported mode values retain the observed `0x411000` fallback call
without assigning it an invented meaning. This models state selection and
cache behavior, while device HRESULTs and the effects of the final wrapper
calls remain external. Evidence: sync routine `0x457AF0`
(`ev_be3fba64119c81e80dc48b62ec2ff7f65e6dc71bdfe6ac1daefcb9f2e4d619b2`),
gate accessor `0x44E400`
(`ev_b99b14d268be19ab381542ac0bf594045853f5ccd997f361312fd1f66db5e213`),
and helper wrappers `0x457F60` (`ev_485553f29ebfa97a054cefff993c5424691dee51d2b114c9bc75f14b806390c5`),
`0x457F80` (`ev_db26947b3d76b72d4aa256e75ea66041d82bd36393f2d822066d7082bcb36a18`),
`0x457FC0` (`ev_44f4afe4b1b2509ce03ab73a312e6ac0345b3166454a1379c81a15af10eff0d7`),
`0x457FE0` (`ev_4ab786f2976b004cf86660f806d91222efa994b807e80b6e3e360ed9fa2b7dc6`),
`0x458000` (`ev_c0e9ad6a4cd8810fc1549bcd6f86c70a0bc6fa3bfa8edd2aaea228694eb2510a`),
`0x458040` (`ev_e53f9318a84d4beee0904e8b457495ccd68ef67f7761af2c334a53387468fa26`),
`0x458060` (`ev_4d390d06f0e162677578089065b1ac3281437b7783283172e2bd3ec5cec4da69`),
`0x458080` (`ev_260a7195dfa95522020351936f64995a33ee748cef3ca2d9e8d62e34f608fd83`),
`0x4580C0` (`ev_722cdf4ef3a521767c860295cc551bd8ceef8c07388574ec4048b885ea190256`),
`0x4580E0` (`ev_95b32403d7de622fe1f77d5890a8338e00aed166120bbc94f09878aebd0b21c0`),
`0x458100` (`ev_2187a93266e5e8bfc05f902f7475c3f6d28d5852334ed48bf4a5d894278b7ed9`),
`0x458120` (`ev_5f7edce263f32fc27b2f71ea9ca8f8639f9a077ce8dca2feb52eaad3d6699a72`),
`0x458150` (`ev_dc50289ecf7725babc5526e2dee07d6767ff14cc331939bcf78a9952820f20bb`),
`0x458180` (`ev_8ae3a79abd9d97f9be07673384d9afea2a701f73dfaad06f87effd19085373cf`),
`0x4581A0` (`ev_6657bb1bf05a38cc2fffbddb177d1b2813cbfe1a373ec99c0c1bc8831611720c`),
`0x4581C0` (`ev_6c7ac11d83c9d3415f9cff9eff1e2ab3714eb2d0366b920195bea919403713de`),
`0x4581E0` (`ev_bdcb274e21391edb1900ba70566ea88991adbfbe805fc8c890dcfadccf353d47`),
`0x458200` (`ev_70683ba37c796cfa15f3dcf068663cf58edd2c801104a0011498f9e3d4963ad5`),
`0x458220` (`ev_1c498908c2a7db93192602dbed7f982a5a85ef3b4dd9895dbe05030ca297ac81`),
and device wrapper `0x458BC0`
(`ev_3f3f2bd8815c0b4e866d99d3d2664fe6e74bbada0fe5591eae3d2a6a67145519`).

The wheel frame's state wrappers `0x4513A0` and `0x451610` now have an
optional observed-state projection. Their selected mask is a 32-bit value at
`0x509F50 + DAT_004DA120 * 8`; the frame ORs bit `8`, clears bit `0x100`,
sets bit `0x100`, then clears bit `8`. The byte `DAT_00509B39` changes only
when the `0x100` set/clear actually flips its value. Each such flip records the
`0x455A60` notification and `0x457A20(&DAT_00509B30)` snapshot refresh. These
per-step effects are omitted unless the caller supplies the selected index,
mask, and initial enable byte. Evidence: setter `0x4513A0`
(`ev_c778fdfe9dfec169127a6ad9f74440753041d5b3fcab2c6b450461659fad5459`),
clearer `0x451610`
(`ev_6b68320e67ef48b48d993a9fadaf1d58e2f40452146d5b3037fda98ce07acf8f`),
state accessor `0x451890`
(`ev_c14d1b186c5376e69547270091a5a6811b1c060256e82b36f0408650ee8dfc91`),
and snapshot refresh `0x457A20`
(`ev_c42837495c0f84efbfe27caca7ad1b880eec14ae75246e16742027b7ceb3ddc2`).

The `0x457A20` call used by the wheel bit-`0x100` toggle also has a portable
snapshot projection. It always records the state-record pointer assignment to
`0x509B30`. When `DAT_0050D778` is set, it writes the observed inverted-byte
cache sentinels at offsets `0,1,8..14,20..23`, writes `0xFFFFFFFF` at `+0x10`,
and preserves untouched cache bytes. The wheel plan applies this refresh to its
local state/cache observations immediately after each actual `DAT_00509B39`
flip, then uses the resulting records for the following `0x457AF0` plan.
Evidence: `0x457A20`
(`ev_c42837495c0f84efbfe27caca7ad1b880eec14ae75246e16742027b7ceb3ddc2`).

The wheel's `0x455A60` toggle notification also updates a pair of indirect
render callback slots through `0x455C80`. When `DAT_0050D0A8` is false, both
slots select `0x455D90`. When true, the pair depends on the supplied
`DAT_0050D119`, `DAT_0050D2DC`, and `DAT_0050D118` observations. Each actual
`DAT_00509B39` flip now optionally records that selected pair; the callback
bodies and their eventual effects are not inferred from the pointer values.
Evidence: notifier `0x455A60`
(`ev_a6223e48f4c508515c27b7eab8eded92b5b74178b6b50b7ae10566d1e93b49c6`)
and selector `0x455C80`
(`ev_68291d80b42b224b56e5bf259e990878f9c0ee36bf8bb4dc924e36a4a38f6e7c`).

The first two code targets in the `0x455C80` default callback pair now have a
bounded row-buffer model. Each callback reads signed top/end rows from the
record at `+0x18/+0x1A`, advances source and destination by `0x20` bytes per
row, and increments `DAT_0050D304` once per completed row. Callback `0x455D90`
copies dwords at `+0,+4,+8,+0x18,+0x1C`, forms the `+0x10` word from
`DAT_0050D110 << 24 | 0x00FFFFFF`, and writes `0xFF000000` at `+0x14`.
Callback `0x455E00` accepts record modes 0–2, copies the same source words,
ORs source `+0x10` with `DAT_0050D114`, and writes `0xFF000000` at `+0x14`.
Both preserve destination `+0x0C`. The model returns ordered dword writes and
an updated bounded destination view; rows outside supplied spans stop with an
explicit incomplete status. Evidence: `0x455D90` entry
(`ev_0e240ba033f5420a5f69cd9b4d20b1dd89e697fe9b6b290ca0db94d6ab475355`),
signed row bounds (`ev_7ca24ef80c66548a58f1e801fa43f60d2e6a3a4b3f403ab2e5d816e50a509e1a`,
`ev_6491ef452681fcb8e60b38b949d42db4d053f1dbd56279c62d317b9c50506f90`),
row stride (`ev_c57ad1556fdf90a4880fb51a9b01f98e6a3d9f891ed3620035cf3fa428faf742`),
alpha-color construction (`ev_5da884dafd9d69819c0092e00040c05c2ebf79981307ec8c3c4e2e472c48c3e1`,
`ev_b84ca772fd1a6d1c9f35b8a8c55fed261c07a5ad8e5bc2d6e4e00c5e3c29d1e2`,
`ev_902de21683878c201d637fdaed827a166fb986521b3b23ac4be7500a1e19dc3a`),
and writes/counter (`ev_fe451c9b7955ec591b3c5db0702fb3c88f58d357e4c21ee821ed07507267522f`,
`ev_c392f9a6e5eae13d6a6ad39f5e4514ce500e6a59e0fa889523eaa9105d8b1241`,
`ev_59e88921d5914642dbc582f821b94a538e346a47c9dbb39e5f354901eeae8b91`).
For `0x455E00`, the descriptor mode and accepted comparisons are observed at
`0x455E19` (`ev_4cd212502f71103cc5a699781418c1be1ffcc2f1bc2f474888220cd9ee5a9920`),
`0x455E26` (`ev_c922c89fe1551a29751f580c5464a93087762de9b325222d2c01d811146b2591`),
and `0x455E2B` (`ev_c7b47c6c0892f7c73502e148800359547c5ae62ff516369d3e9491d95afce21b`).
The masked word and output stores are `0x455E6A`
(`ev_27578e6d856ad1df8a6263c98dc7929b0893c460882bcfdcdb1a736296fa0b6f`),
`0x455E70` (`ev_82da6352bac58a33861b92adf64061e7d27bd9e4c865c3d5d96cc87eb8c46ba4`),
and `0x455E73` (`ev_d66f2fe2c0bef1a69b925bf08d94346e0b7005654e140e8b083993ccd7b38e91`);
the second loop's stride and counter are `0x455E49`
(`ev_95c9538bceda0852df6ac94bbd4e7e2f1a88daabde825882008fc4f705b75c7c`)
and `0x455E76`
(`ev_74d88c40605eed9fb3c86b35a844d84a3c76a8cc902f31637627197893206bd4`).

The enabled texture path's `0x455F40` row callback now shares the bounded
copy/write projection. Modes 0–2 all copy `+0,+4,+8,+0x18,+0x1C`, OR source
`+0x10` with `DAT_0050D114`, initialize destination `+0x14` to zero, and then
write its distance-fade alpha word. The distance test compares squared
distance from the source position at `+0,+4,+8` to the global point at
`0x50D0E8/EC/F0` against `0x50D11C`; the near branch writes
`0xFF000000`, while the far branch derives an alpha from `0x50D120` and
`0x50D128` with a floor of `0x80`. To avoid inventing exact x87 conversion
results, the model takes the final per-row alpha byte as an explicit input and
places it in the output word's high byte.
If that observation is absent, the plan records the known copies, color word,
and zero-alpha initialization, then stops before the unknown alpha write and
row-counter increment. Evidence: callback entry and mode gate
(`ev_a3299de5aef5f26a510e5ce984979283578d08228c6713413244c3a6c9831311`,
`ev_6ccfaacf66eb4d956449e9d48766566ee2020e967ebf578483f028de26789fe7`),
source row copy (`ev_9db877f82490898cc6cdbcdd64ed05d64b6fb6db79f16923058719453372b716`),
color write and alpha initialization
(`ev_3d1690998a70dfb77608c8d17a95b9b4a56512469aaa053184dc4ec6d80aac99`,
`ev_f16d6517e78105902554264a867ac6575d153de9fe87a731b266d4f50a657db8`),
distance threshold and branch (`ev_a79a8349cd6137176d0cc816cb155c87bf49c204a590871117214c2c7b3cfaaf`,
`ev_0abbcd2ec8f7e1fd8980279c079aac42d151f7157fff975e125de662e896cce6`),
far-alpha calculation/floor and output
(`ev_36901fefbcafe779f31e25c86387af316a60217134fe7b96dca7578760708de4`,
`ev_e9c8735c1be6bc8e162d6e90f7aa79d98279d5d2bab0ae39fb7d627462c074d9`,
`ev_01fc371efde60a789492ffc27145b83cc4eff9b4c23093526ceb9c2910fc06c1`,
`ev_c8b7dc69c8bace1e376dc3b27b93d87ffb4d2fc98b20688ef555f90540433854`,
`ev_4d1fb796aa32e2d7540876a292dbace7447d83188244c91d5f64f7bb9c3786f4`,
`ev_f26966c30a89b953432deb3695e3965fb8fa77afdc32386d621254ce53ac84`,
`ev_2ec1aae36fc24654194530362fba20b9100853746e89a2a412ee680768d07852`),
and row stride/counter (`ev_0d5984591da78047ccb44ce997e71f50bcf8a7f404c618d0ec8e60a33762894a`,
`ev_323ef6a2a48884d33e80c59414813a71fa86054846f480620274109e64b7e4f2`,
`ev_54c1397d51e32b056d00b3690a537945ba8a4f18ce889f32925825ddd3f2893b`).

The callback `0x456230`, selected with secondary callback `0x455E00`, also uses
the palette-index projection shared with `0x4569C0`. Modes 0–1 copy `+0,+4,+8,+0x18,+0x1C`, write
the masked palette word at `+0x10`, and leave destination `+0x14` untouched.
Mode 2 copies the same fields, writes `0xFFFFFFFF` at `+0x10` and
`0xFF000000` at `+0x14`, then increments `DAT_0050D304`. Palette conversion
remains an observed input because it uses the same x87 arithmetic/table-index
path. Evidence: callback/row range (`ev_9bad490bc48c3cfc58899621ac53ae5436457a323e177b2db4eb9c5f4bbc583c`,
`ev_be818b3df6212d0c89cea7dd5f14387340bacc40796e27aa00f0ef83a10c99`), source
copy/formula (`ev_946433c75a2da8ad136ea1f681411988c3f0fa7d198ce8552b0f824ad06e43ac`,
`ev_5c9ff56605c9f2f9fb7fa0e2d791073f42e6f9be37e6834a024955046f90b28a`,
`ev_adee1c5ba173587e0d889e7fa25856553fe04db76afb399deeeda942494b59e6`,
`ev_72f831e01659ece57781562e54d15e637ad6825aeff321f1e2a04c9fd9711f92`,
`ev_41f7d79cd905f00b468d63c4a4cd5521fd2f867d8aaff3748ec33f061f95238d`),
palette lookup/masked color (`ev_05c196e12f376025c8a1d6ae4a5ba65dc98bc777037c7ca2c0ecd4d254133262`,
`ev_c9c90ab39585e6a7632416be635de3bc08b56741f653acda9488c752c173120e`,
`ev_31860bc7d0254f4cda604625345bd97c939b0151c8b630348992f10e6ec2f1c0`,
`ev_7ead392d5414f5e276302a4e9cf0a884dbeb1ef86b359c873190ae020bedaeda`), and
mode-2 color/alpha/counter (`ev_33ea4dbfb23181ba21daecc72374536e8ab3da88c051f656f68c0b6e36215240`,
`ev_573f0f2f237648640e519f1af8f095f7c592c0cd774cd1935df40ce000d40d27`,
`ev_ea9a50c5407a36863e2d29597188145f54a34b4101cad528271d7cd9a9d97541`,
`ev_d23d92a9a3faeb0dcd6ce9e6a61fc56b86a7370790b5c6f1614119fed35c5d93`).
Callback `0x4563E0` is the dual-table variant selected with second-slot
callback `0x455F20`. For modes 0–1 it copies the shared five row words,
computes the first palette color for `+0x10`, then computes a second index
from source `+0x0C/+0x10/+0x14` and coefficients `0x50D100/104/108` scaled
by `0x4C8F7C`; that table word is written at `+0x14`. Mode 2 writes
`0xFFFFFFFF` at `+0x10`, initializes `+0x14` to `0xFF000000`, then overwrites
`+0x14` with the second-table value. Both computed indices remain inputs due
x87 conversion. The bounded model preserves partial writes when an index or
table observation runs out. Evidence: entry and mode gate
(`ev_dbd7b9c5fc3e1cedebca04d705f90131404ae748727c71d7fc379419f0642487`,
`ev_acc6e7f2d4c18a37844ddcf20c352e0448f4b552c03d464ec7976004f2277ceb`), row
copy and first palette transform/write
(`ev_668f362482e9f0e91cffea927c948d918ddca85af5fda45c6e8c25830dbaff43`,
`ev_1d5ba11033aaa8011ed46fe43f6c5c88148ee4b21e12fd5a9e9c644002d1e115`,
`ev_a3fd971a9ff41774b06ba6581a64cd374b4ba9ecd28e7b95f2fa43bf6079c527`,
`ev_59ccd020932c10b2ec41a1f9177f4e902c8ea2a3b2f43ac9e4ca3f91e8cbcbf8`,
`ev_d919bda4c048cc7747e463091316e8eb4ec1f1c956b033e66cb922f30799e78f`,
`ev_a05b7a46e692d0a2bacc80c4753c3d6371a96d9866f48e39b515b0498a8416e2`),
second transform/table output (`ev_7602f86d56db2e36e5d28af06f0cb862aff372a67c9339283dc8338f7aed9e7e`,
`ev_458e8c5f1c1d548ce21d6dd3567c9e1620bcbe40d2fa58ed3142063e7946197e`,
`ev_afa674a514f3d90ed58c5027dacf6f67aeb48bf26131d8c7fe5bf958c010af34`,
`ev_63fc5d65f977d7f8002740be34c1be14406c8a29e54e8bf1a2c74a9c264e43f5`,
`ev_4fdd6045b3f4f636a8acad99bf8642396b0a17f1ecd3d21f3a63816b48bc34a0`,
`ev_176b37d1639664307e13b243cb62cc4a2971930038bdd4d81844a2a88c2435c2`,
`ev_cfac67a4dc2674b44fd4b1ab9e6391180e00546201b9a91289b572bdef5eb2c1`,
`ev_b5e3001ab85f13106e653f29f63982fe276eb04956806135c2f4c158eecc6983`),
and mode-2 ordered writes/counter (`ev_5d01648eca1c070731f49276331b01507d876f347540cbb666b98a59c7179fff`,
`ev_25783217e43bfdb26a696ed93b78a58dc1e7196c739766f403e7f52902b6a486`,
`ev_2ea5358720268173fd25911e09876f4614cb4a5d16b4cfcbe6c20489ad879d9e`,
`ev_acec99eb35253ec60ebd9c65cad803ae3e88bd595b22a1b18aa000030ea2672c`,
`ev_c7171b93c2ab8b9e6fe923568af3a7dcbc81f6ed83db2f9469090086e1f0cc7e`).
Callback `0x456D00`, selected with second-slot callback `0x455F20`, extends
the dual-table conversion with distance-faded alpha. Modes 0–1 write the
primary masked palette color at `+0x10`, the second-table word at `+0x14`,
then OR the distance-derived alpha byte into the high byte of that second word.
Mode 2 writes `0xFFFFFFFF` and `0xFF000000`, replaces `+0x14` with its
secondary-table word, then calls `0x4575A0`, which applies the same distance
threshold/fade and ORs the resulting alpha into `+0x14`. The two palette
indices and final alpha byte are observed inputs; if alpha is unavailable, the
plan retains the table word and omits the final alpha write/counter increment.
Evidence: callback and mode gate (`ev_e071224bb6bc060e73e08a508d230ef8d368509b569e2105036d6b9ce0296b68`,
`ev_915a859f0a67e221537c16e25c505b46898402d4d39d7234cfc3d6f31533dd8a`,
`ev_6374acc524943edce232b519ac0d26cb48e99f952a1cfcf896e4159852ab2012`), row
copy and secondary-table store (`ev_49b4b710db8ed1ce294caeb0109391bc6e7b986a860b7dbed0e4b2571f939943`,
`ev_bf3988fb216658556b5dfcaf2c98816f12aa11c3463452ace30f44d3313d98f2`,
`ev_4b43977c088b079b1a1b713ecb5e24f570146f59c466f6cd14499a62af0f6283`,
`ev_ad8cdb2f3245852f01a8fa6cfac7c08c8a6b14d122bf9ee21121722a7e2450a2`,
`ev_30eec3afc43700b0832d94cb8c36a83b3c33b22682f43b1c8395a07972130d29`),
inline fade threshold/alpha OR/counter (`ev_a1439b216264bef545659e0cc049f28f470ac6a2ad2558820c9d0cc6f3cbfa6a`,
`ev_46d26d05a34f2c202c3e46a85644c7b9bbae3114a75bd2166f8c1b3cc8b9c950`,
`ev_b3af9aa42004d2836648cc7dbc8c6a20575e293c35f38eb47ebaa81cbc1181a5`,
`ev_2c105eb6462058804b1679c46334261b44d01984582504cfcfd565543d6e8d43`,
`ev_e8f2324331ae598d40363191846d7446fc6d5af3ec9b1a4b8ee7a8c6440daf3b`,
`ev_6032a73d843be20d136c1193e1d3ebb54b80972f25a1d1dac66df9442b1efcd0`),
mode-2 sentinel/table/helper route (`ev_0f49e168d114b3552c850c131330b6bc9b2b4bfd529e2f7aacf32f9a90c572d2`,
`ev_ac3170b24d15c9b37c1ab4b6c0bb244b924b7a87d130f53a400862fc9d24d8ce`,
`ev_510a8d2c4e9e0015c0126651079b75dae237cbc59f44d9d8760340b6b01b6220`,
`ev_ed27feaaea8c686f9dbceed0abbeab962f9bcaabb0d2d7b40c707d6f8532fdec`,
`ev_dd2d634d6d93db67f8b7f1a367a2d978e2ffa3c1331bb1a8aeff7b4fba859be7`,
`ev_02b66a4033ddd9940f6c61277c8a46c51c2efcb26f2b5623fa21e9e4b5a623bb`),
and `0x4575A0` alpha gate/output (`ev_ad11b94bfff01d00f4de9fb169dd3a4392f2e331597b298bfac427216ac7e436`,
`ev_144d3407a7acf514313e768daf9b9dc06e3ac089ccd4614598efbbaa93f51a0`,
`ev_2dd5d5e404c384e89357f740b7c34f0eb22392b5861aebcd6e633e5c04062beb`,
`ev_1460a189168ddffd72648e6a1c56d92531a58262d317b21ae3192f5f84801f3d`,
`ev_1615b4426b3acbc7bc4b1d49a260a80accffc810aaa1ec5ca9dc04aa4b3d267f`).
Callback `0x456660` is the prelit-object variant paired with diagnostic stub
`0x455F30`. Modes 0–1 copy the same five row words and select the masked
`0x50C890` palette color for `+0x10`. Mode 2 writes `0xFFFFFFFF` at `+0x10`
and `0xFF000000` at `+0x14`, then calls `0x457460` for the row. In all three
modes, `DAT_0050D2DC == 2` gates the extra environment-mapping transform;
when enabled, it overwrites destination `+0x18/+0x1C`. The plan accepts the
final binary32 tail words as observations, preserves copied fields when the
gate is false, and reports incomplete observations after writes already
established by the instruction order. Evidence: callback entry/mode gate
(`ev_18a597454de1d8fc3499316ff9e5f9b31f13f197747407f5012ea8d4e7d3fc14`,
`ev_03e8f972bcd2a5280ffd7c8694e309d618e243a8fed25a2bbeba86fb0a5408d5`),
palette/copy stores (`ev_213f39e418aabf5a29e076117f1d6da3e60b8d3a8e37e37736bc957c4733892b`,
`ev_2f6f07c3fda497c74d8623f1cfde4a4d4b510427db4cba7cd5702942b1a74cf9`,
`ev_07cc3c65da39728787499e19e7cd6ba46ef32085713d0ddd7a1f451fea140b40`,
`ev_eee3e1e2fd9a5bd56b0cd3fcf22a5dd4b34203b54c5817043448f4074e065bfb`,
`ev_0d0a819fa673a0bf9222605dcb911673bc94b7b1e4d7c834c86e1258b52d8b76`),
mode-0/1 gate (`ev_0f49dec0d75a298593651c166a876a59c4b39cec2f5f1774736c9f065cd720b6`,
`ev_bc0cdb256f3fad080ce7751e555fa2cc51f0827e440f1ac480e48b8d66e9de4e`,
`ev_0a7f82764b5e139fac6a9bf5d27c1cc9d62b329e288260896d097fbfae37b175`,
`ev_93621b893cad1a35394bbe58f64eb0e0e19cd61a5aa32c3a179428dc752efc84`),
and mode-2 sentinel/helper (`ev_a82622673e0af3c5eec2c52e74ea3052f63e6e51134dd5b3e93965f1f40540f9`,
`ev_03e35ef53c218418aadb8cad12eb78a0c1e801319b93831b4bf41aa33f45a614`,
`ev_7f1fa902bda3bf07e7b19f576b313b0491e9bafd1dadb87363dc2a52e950bf68`,
`ev_f5d55d05ae7d34a3b31b02bc36be91495425dcd168ef71a3d43d10286a9d1611`,
`ev_ab0ba35dc704eb1003fe75c338e46f86ebf385ac5b84a036ff8bf85536cf1c65`,
`ev_ba9ad55dc560c4c93f0406e6667bada727cfaa51a5242a630f429048562aae18`).
The primary callback `0x4569C0`, selected with secondary callback
`0x455F40`, now uses the same bounded row planner. All modes copy the five
source words at `+0,+4,+8,+0x18,+0x1C`; modes 0 and 1 derive a palette index
from source `+0x0C/+0x10/+0x14`, the three `0x50D0DC/E0/E4` coefficients, and
an x87 conversion, then write `palette[index] | DAT_0050D114` at `+0x10`.
Mode 2 writes `0xFFFFFFFF` there without a palette lookup. All modes initialize
`+0x14` to zero and use the same distance-fade alpha logic as `0x455F40`.
The computed palette index and final alpha byte are caller observations, so
index conversion and x87 last-bit behavior stay explicit. Missing palette or
alpha observations stop the plan after the native writes already determined.
Evidence: mode gate (`ev_9db4680ab87b06ab685ec5b946d27b2004ae1595e1005ca355787f770848977f`,
`ev_d7f055d67db31e3cf47deecdce480f20858dae11afa069adb1c12508be1ad16b`), row
copy and palette source/coefficient reads (`ev_e1c95883bf6ae7ee28e8b7948a1f99ee937c9539055305a4b6024d735bf705cb`,
`ev_ccb331224ef1b924c3c6452b6835e82aee85527d41c714e8b7e370762146fa8d`,
`ev_2f19ef52211ab8a089dc18f8b9be52da05e9b57b648dd8da184cf524a458c129`,
`ev_86e51a69d553ad4076de81fa94dcec697548000741e888e7c3b2c1f735ba924b`,
`ev_c782eefd4af63cf27cfbf38e41ae11c04f5b8ac9240acba3963ea9789df8e6c1`),
palette lookup/mask/write (`ev_a40c0641e2ec0930db926b22b338fd2cee3af40d427c065be45f26e0fa44d056`,
`ev_55c6238a657c40d69dd1b47a0c04adcf6b34d4847ba3bf586a91908162906ca2`,
`ev_b69d5e78637fecc51cc85fba92866f288cab3ff7f7208c6d90b34cbfa4adf964`,
`ev_ada74c40d31d008e09a54d4a70af79e752e28407c3ea6c7540c5c141141127b6`),
mode-2 white word (`ev_de7009744cc96d4e9b24ec6c5304186adbe92ad703f7c0561b6757e68803b0a8`,
`ev_3cd2033e7b010ee084194ebeca2ee950d5362f11a07d12ad37718aa331654a14`),
and alpha/counter (`ev_4799b3d0c93f5a0b6b64038d723121f99beab6c284746b903114b9da4005e28a`,
`ev_d949f7362892be769c21edf90736ae48f1b11dd8352dc45c0f4ae983a9964b15`,
`ev_6e0198912148953d222fe003c55507c6d0c1fd05ec8facf045ae4a7e745f675e`,
`ev_2c3a186b8dad8766d6c4878141939f9e7e423dfc6ea6d0da6e526aa3fd5a825c`,
`ev_f78447609e79fc750a255148f553f285afe37067a98b864c0cc64010f167127a`,
`ev_6385719af1e8b8f040648a11e5e74910aae157499b8a497f7cef486e374f1fc0`).
Four other second-slot callback targets are constant-argument panic
forwarders: `0x455F20` passes `0x4DB74C`, `0x455F30` passes `0x4DB78C`,
`0x456210` passes `0x4DB7C8`, and `0x456220` passes `0x4DB808` to
`0x411000`. `plan_scene_render_panic_forwarder` reports this callback-to-
reporter mapping and the recovered format text: `light_pre_nofog_someenv: no
environment mapping of prelit objs`, `light_pre_nofog_env: no environment
mapping of prelit objs`, `light_pre_fog_someenv: no environment mapping of
prelit objs`, and `light_pre_fog_env: no environment mapping of prelit objs`,
respectively. Their strings are catalogued at RVAs `0xDB74C`, `0xDB78C`,
`0xDB7C8`, and `0xDB808` in `metadata/strings.md`. Whether a selected callback
is invoked on a given frame remains outside this mapping. Evidence pairs show each stub's
push and call: (`ev_cbbf47451c7a9b25c55418f88cd35693e7cdced5a203431b21813aa1f2a5c80f`,
`ev_a037d9e223d80e01116e93668c47a10a99cfa9f3318949a8b75ab73c40a0fa10`),
(`ev_5f6b8ef2fb5a7b7f6532cecf58c8749a9d39666abdb3776a3f56ce360704a42a`,
`ev_bdfb496302c87b96e2d44fbdb9b50d1d1e9949922e96e6aded14d6de8cbb3a3f`),
(`ev_31d473f88395ec60ab428c2b292111d983b4afa6cb27bf13547b61ddf9d9ed37`,
`ev_23cf023a0522fc3b0302a957f61d50d1a8f02701167eb84ef94a987bcea93ecf`),
and (`ev_8312f2375d61d5a115fe0c2e9f8c940ae60397056ad16caa0dfb0f393db3297a`,
`ev_d6eb78e5db4a92fb25d404f115ae24dab73b50a74378caf39898056bef4c9c54`).
