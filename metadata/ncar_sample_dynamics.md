# NCAR sample dynamics at `0x439520`

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Addresses are preferred-image virtual addresses. Evidence was collected from
Ghidra 12.1.4 against the shipped PE.

## State and update order

`0x439520` first copies 24 bytes from the current derivative at object
`+0xECC` into the history beginning at `+0xED8`. This moves the previous
derivative into `+0xEE4` and the current derivative into `+0xED8`. It then
evaluates the current derivative from the three records pointed to by `+0xEC4`:

- sample times: record offsets `+0x04`, `+0x42`, `+0x80`;
- vector components: record offsets `+0x1C`, `+0x5A`, `+0x98`;
- current derivative output: object `+0xECC..+0xED7`;
- history-derived vector: object `+0xEF0..+0xEFB`.

For times `t0,t1,t2` and scalar samples `y0,y1,y2`, the first triplet is the
derivative at `t0` of the quadratic through the samples. The second triplet
applies the same derivative weights to current, previous, and older derivative
vectors. The binary stores each output component as `float`. It then compares
the squared second-triplet magnitude with `96.23611`; above the threshold it
calls `0x422C10` for the magnitude, normalizes the triplet, and scales it by
`9.81`.

For the history-derived vector, `0x439520` stores `t0-t1` and `t0-t2` to
single-precision stack locals at `0x4395E3` and `0x4395E7` before reusing them
in its component equations. `history_derivative_coefficients` preserves those
two intermediate float roundings; the current-vector path uses unrounded x87
time differences.

Finally it decrements object `+0xF24` by `1.0` and writes
`((1.0 - +0xF24) / ((t0 - t1) * 60.0))` to `+0xF28`.

The portable implementation is `advance_ncar_sample_dynamics` in
`src/ncar_state.cpp`. It uses `long double` intermediates where the host type
exceeds `double`; it retains the observed float output stores. This does not
reproduce every x87 intermediate/store boundary, and platforms where
`long double` equals `double` gain no extra precision.

## Pentium FDIV compatibility path

The branch is controlled by global `0x4E9754`. When zero, `0x439520` uses
ordinary x87 divide instructions. When nonzero, it calls the VS runtime helpers
`0x4BC437` or `__adj_fdivr_m32` at `0x4BC9FC` for the divisions. The latter
passes 80-bit operands to `__fdivrp_sti_st` at `0x4BC8E9`, then to the runtime
routine at `0x4BC320`. That routine examines significand bits using the table
at `0x4E9770`, adjusts the x87 control word and scales operands with constants
at `0x4E9780`/`0x4E9784` on selected paths before dividing. Exceptional and
subnormal cases have additional branches.

The runtime value of `0x4E9754` is an explicit `fdiv_compatibility_enabled`
input. CRT startup routine `0x4BC260` stores the return from
`__ms_p5_test_fdiv` (`0x4BE370`) there. That routine first runs the fixed
arithmetic probe at `0x4BE320`; on the remaining path it resolves OS feature
helpers and runs additional probes. Their environment-dependent result stays
an input to this portable model; the call and affinity sequence is modeled in
`metadata/legacy_fdiv.md`. The compatibility division's common
finite-normal case checks the recovered 16-entry significand table and the
following three-bit gate, then scales numerator and denominator by `0.9375`
or `1.0625` before dividing. This is implemented by
`divide_with_vc98_fdiv_compatibility` and is used by the three derivative
coefficients, optional magnitude normalization, and interpolation ramp.
Subnormal, zero, infinity/NaN edge behavior and exact x87 control-word/80-bit
rounding remain incomplete, so compatibility mode is still an approximation,
not a bit-exact recreation of the CRT helper.

## Evidence

- `0x439520`: `ev_c26dab59ca5ea889c525e6d3f3f0e235db2cb24518d70f6f34fc2d19edd739d8`
  (function body `0x439520..0x439A07`, pseudocode and assembly).
- `__adj_fdivr_m32` at `0x4BC9FC`:
  `ev_f5bad3ad37b848f7a9d0419238a2f1aeb66badfee5a91c8005294ca5049291de`.
- `__fdivrp_sti_st` at `0x4BC8E9`:
  `ev_c38b7aa68a9d1318206be2dd1b8686ee91fbf628e4c28238bdee975af12209cf`.
- `0x4BC320`: `ev_b3a07d800bd5a3e4cdcef324289483320d551f35ecad0b3844dc60f38699a795`.
- Data mapping: `0x4E9770` maps to file offset `949616`, evidence
  `ev_cc635d5a37808f8705682d5c78f2244df407a71ad0f6c0e998ff3f84c0ea6b69`.
