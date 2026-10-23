# Interpolated physics object update

## Wheel record update (`0x444240`)

The update consumes two five-byte wheel records and an interpolation fraction.
The portable projection is `update_ncar_interpolated_wheel` in
`include/viper/ncar_state.hpp` and `src/ncar_state.cpp`. It exposes the direct
fields and records two downstream helper calls without inventing their writes:

| Record data | Object field | Projection |
|---|---:|---|
| byte 2, current and next | `+0xE8` | linear interpolation scaled by `1/64` |
| byte 0, current and next | `+0xAC` | interpolated `(byte << 8) * 0.0003834952 - 6.2831855` |
| byte 1, current and next | `+0x124` | linear interpolation scaled by `200/128`, then `-100` |
| byte 3, current and next | `+0x11C` | linear interpolation scaled by `10/128` |
| byte 4, current | `+0x120` | copied; bit `0x40` also sets `+0x20` |
| parent velocity `+0x234..+0x23C` | wheel `+0xBC..+0xC4` | three floats copied |

The routine then calls `0x443F60` and `0x437BA0`. Their independent models are
`advance_wheel_animation_basis` and `transform_body_local_point`, respectively;
the wheel projection here leaves their additional input state explicit.
Evidence: `ev_20a7335a3ddc3113b52072caa357d8f447779c1dca79f17854b811d17e5af898`.
Host-side interpolation uses extended intermediates, so exact x87 register
rounding is approximate.

## Parent update (`0x432420`)

The parent interpolates its rotation/velocity through `0x43F3A0`, updates
scalar fields and flags, runs four wheel updates at `+0x554 + i*0x1A4` using
five-byte records at `+0x24 + i*5`, and stores the signed record byte at
offset `0x23` into `+0xD68` and `+0xD64`. Its input records are 0x3E bytes but
are not the decoded `NcarSampleRecord` history format; keep their layouts
separate. Evidence: `ev_5414f1bab7fbe8468b429851feddd8e526f06a75951618dba0a23d22e5f02da1`.

The direct subset is exposed as `update_ncar_interpolated_object`. It models
the record ushort at `0x1E` into object `+0xD70` (`value*14000/32768 - 2000`),
the byte at `0x20` into `+0x500` (scaled by `256*0.00007324219`, then `-1.2`),
bytes `0x21` and `0x22` as interpolated setter values at `+0xDE0` and `+0xDE4`,
OR of the two record flags at `0x38` into object `+0x04`, the current record's
`0x100` flag into `+0xE28`, and the first record's signed byte `0x23` into
both `+0xD68` and `+0xD64`. The reset helper call is reported when the combined
flags clear bit `0x20` and the caller supplies a nonzero object `+0x48C` gate.
When reached, the result also reports the reset's direct clears at `+0x48C`,
`+0x48D`, eight DWORDs beginning at `+0x48E`, and four wheel damage bytes at
`+0x574 + 0x1A4*i`, plus five resource-group passes and the backend virtual
call attempt. Resource rows use the existing bounded
`reset_ncar_resource_groups` projection. For each pass, it records the wrapper's
resource-entry `+0x10` descriptor-pointer write and preserves the supplied value
when known; the wrapper clears byte `+0x15` without validating the pointer.
Evidence `ev_ae086326d3d80f4eb2d8606a0bf9df800b98782b106605629bb331998281af4c`,
`ev_caf16edf9d5a790cf3c58c81f9d5ff085a19a55dc8656c4c46eb65f3599c2e48`, and
`ev_b20f5f32088b215e5828d9bd6f313682a2f55deafe67c0cc32995b1e250ad56a`.
The object at `+0x478` uses the
`0x42CCE0` vtable `+0x04` target, which walks its `+0x4C` child count and calls
each child's virtual `+0x04`. The constructor populates this group with sphere
children; their `+0x04` target `0x42BEB0` transforms the local point at `+0x20`
through the parent 3x3 basis and translation, storing the world point at
`+0x2C`. `plan_ncar_reset_backend_group` now projects these ordered child
positions and reports short observations and null-pointer fault paths. This
transform law matches the existing `transform_body_local_point` model. Evidence:
`ev_514b6fa842f491d8b5dbf5501d24b00bdf300a85b95442fe058a447d66bd4279`,
`ev_b019ddbfe2f563e2ebf314797f920fdf745610827e5aeb33e0c03e387ff57abd`,
`ev_d9537c7f3a1e3382945b1a9b4302c86c7ecb8165af2eb6a59d72b06488c4509c`,
`ev_9643dd2fa27fbb6f9cd07a255baf362c9c9be3618494bc912af88d28c0c7dc97`,
`ev_a957a079d7cb019a7d9f11b607222893011d8db06a2f29fc193137619c0e6eeb`,
`ev_b7111322c7466b221124e121d8cab88868bba9c08172a082bac2b328eff60544`,
`ev_335fb86f44d864d6e55a6429d95223162f4e9611db8cac9834f7b3ad05ee98f9`,
`ev_c1d6507e02539da1c66183b9a8e281a1c02ac71baa5ca424f08e9b76123db77b`,
`ev_a85d856838f6b80e0b2072601beae8b93a48d13b1723a85331744fa717352b10`,
`ev_b66a737cbdb951b62024d64c01713cf560bbec332f50271801727399f2c24055`,
`ev_fbd640d5fbdc72a165c76c2dee2639411e71c3beaf9416d13c51fc2bd9d7ee93`,
and `ev_f3e69307c431f783075d86dbeed730ee32736a6159e31f8b9633cd65593d0208`.
When supplied the six `0x4509B0` mesh bounds and the three car-record offsets,
the plan derives the constructor's ordered 12 local sphere points itself,
including the observed `+2.0` extent cap and body-position shifts. Incomplete
vertex spans remain explicit.
The group constructor initializes its `+0x50` radius to zero. Each child add
updates it to the maximum of its prior value and `sqrt(x*x + y*y + z*z) + r`.
The 12 constructor children use radii `0.4` for entries 0–5, `0.6` for 6–7,
and `0.5` for 8–11. `plan_ncar_reset_backend_group` now derives that bound from
the same local points; floating-point rounding may differ from the original
x87 sequence. Caller-supplied children without observed radii set the
`radius_inputs_incomplete` flag instead of receiving constructor-only defaults.
During each generated child insertion, the constructor's virtual `+0x1C`
callback writes tag `0x6D` to child `+0x18`. After all 12 insertions, the group
callback writes the same tag to group `+0x18` and propagates it to all children,
for 24 child-tag writes total. The projection reports the group tag and the
propagation count separately. Those writes assume child allocations succeed;
the native null-allocation path dereferences the null child before this setter.
Evidence `ev_8acf1ec6b270eee028640de6232795e9cb4514dc185de471fa9408a4c1755a32`,
`ev_79088dcbbdc750d8dd4b9af1c312a439df19f7d1296ea864ce0b276044210fe9`, and
`ev_251c0d417d75951173f6db0cac2e213eb6a43f9ae7e3892d7b61bbb111a336ca`.

The separate object-initialization path `0x433AA0` invokes backend group
virtual `+0x14` (`0x42D0A0`). Its child callback `0x42CC10` calls `0x42BF20`
to clear byte `+0x38`, clears child byte `+0x88`, then restores child local
position `+0x20..+0x28` from its constructor copy at `+0x64..+0x6C`.
`plan_ncar_backend_group_refresh` projects these writes and retains unknown
constructor positions instead of substituting current coordinates. Evidence:
`ev_61ffe05c018e4f962129e9e15238a38cda80b3f8f565bd7d66bba903a47db603`,
`ev_a3edc180d35423e1da66e11a8136e73fdcc626dd5eeb7c6fcdf6f7cb659b56f2`,
`ev_2346d3e4b93bc951df62e61e529ddbc932a33987a9cb5e8cac6d183b41adcf22`, and
`ev_91ff43123834f5d91d1bb523fad7a7bf365bc2f74b1a70c5d7698af942a9b037`.
When the child span is omitted but constructor geometry or mesh bounds are
available, the refresh planner now derives the same 12 children and saved
positions as the constructor; allocation success is marked as an assumption.
`plan_ncar_object_initialization` now composes that refresh in `0x433AA0`'s
direct write order: flags and constants before the virtual call, the copied
12-float transform and clears before `0x4331E0`, then the remaining clears and
two scaled values before `0x4338C0`. `0x4331E0` zeroes `+0xCBC` and copies it
to `+0xD64`. `0x4338C0` marks each `+0x98 + 0x30*i` child flag, resets the
four wheel slots through `0x441B90`, performs the `0x433920` resource/object
reset and backend `+0x04` call, clears `+0xC0C`, then performs a second backend
`+0x14` refresh. The child flag count is bounded by supplied storage capacity;
the ordered slot and object field writes and both backend calls are projected.
Evidence: `ev_61ffe05c018e4f962129e9e15238a38cda80b3f8f565bd7d66bba903a47db603`,
`ev_ef3f6561fe17c3e1792508c7b1e36db310029321c68de866642a978194a7b883`,
`ev_4d766d96cc096b5309ab9cc09bd834cb5ebb54c48351ef07b01746dd1b022393`,
`ev_fb90814359e3f3a42e262dd8130e8c47646bbba841228f159107d0d9f6fc4b91`,
`ev_64947ff4e28fbdd3ac7435354d31fbc7ae4077144462f6e1393822fc720240ab`,
and `ev_a9ab24f7615066d8087ef3cbe99ebca5584692be6009f0aeebb7859806ff3450`.
Evidence: `ev_dd332da171bfd341a47517194c444647a9d0e5c33e934c709db81942b19d120f`,
`ev_7b0433b3ba1bb352def91e7726dd2c36abda7a8fb9ede2d11123a1c73f90a2b1`, and
`ev_f2bb786a74070a51b02f0626e35bf0593a3b38ab0f124dd60bb1a06abe4d65dd`.
Other record-dependent fields are intentionally not inferred from this subset.
Three floats at record offset `0x12` are also interpolated into object velocity
`+0x234..+0x23C`; their Euclidean magnitude is exposed at `+0xDDC`. The parent
plan composes all four wheel projections from record offsets `0x24 + 5*i`,
passing its interpolated velocity into each wheel result.
The model also records the `+0xE40`/`+0xE49` clears and helper `+0x470` clear,
the `+0xDF0..+0xDF8` velocity copies, and the `+0x630/+0x7D4` versus
`+0x978/+0xB1C` signed scalar placement selected by the caller-supplied
`+0xBE4` byte. Record byte `0x3A` feeds `+0x50C`: zero current byte forces
`1.0`; otherwise the current value is scaled by `1/128` and either interpolated
or held according to the `0x200` flag in the second record.
The same flag selects which record is passed to the rotation converter in
`0x437650`; `update_ncar_interpolated_object` derives that selection directly
instead of requiring it as an independent caller input.
It now also projects the transform written to `+0x38..+0x64`: each endpoint's
three ushort angles become a Rodrigues matrix, the `+0x200`-selected endpoint
is blended with the first, and the resulting three basis rows are normalized
and rebuilt by the observed cross-product sequence. The trailing three floats
are interpolated from record offsets `+0x06`, `+0x0A`, and `+0x0E`. Legacy
FDIV behavior is routed through the existing compatibility approximation;
x87/libm rounding remains approximate. Evidence: `ev_9afebfd1baa1cad8101c71f4ac20399b0c4ef1b8ef7f6fca2ca7cb5147dadd12`,
`ev_9908ffa0e67a6e59d1e3de0f410d952d88805616c367519a42a0d1bf4024856e`,
`ev_e8ed20d85f950e46855973603f0e96e9f313df3c2c611318b39daa5958f7d632`, and
`ev_19ba195243bd9450229ee5b69f82713490cc5c875592c92e4aa7d599b264b27c`.

The actual direct caller `0x427B10` is composed by
`update_ncar_interpolated_state`: it saves/restores object `+0x04` around the
inner update and derives `+0xED0` from the linear interpolation of the two
records' velocity magnitudes. Evidence: `ev_4056ddfc4f12323d76158052c3dfd33d1ed4cdb27387a46500af0aae76dfb767`.
