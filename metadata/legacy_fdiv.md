# MSVC 1998 FDIV compatibility initialization

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.

`__fpmath` at `0x4BC260` calls `0x4BC290`, then `__ms_p5_test_fdiv` at
`0x4BE370`; it stores that return value directly to `DAT_004E9754` at
`0x4BC26A`. It next calls `__setdefaultprecision` at `0x4BE300` and clears
pending floating-point exceptions with `FNCLEX`. Evidence:
`ev_c8b7fa6a07f3bb117330256e850982d888c4cb01df8cabf2abb4f36e6fb83d7b`.

`0x4BE370` first calls `__ms_p5_test_fdiv` at `0x4BE320`; a nonzero return
immediately returns 1. The probe divides the hard-coded double at
`0x4150017EC0000000` by `0x4147FFFF80000000`, multiplies the quotient by the
second value, subtracts that product from the first, and compares the residual
with `1.0`. Its result depends on the target x87 execution. Otherwise
`0x4BE370` dynamically loads `KERNEL32.DLL` and
resolves, in order, `GetProcessAffinityMask`, `GetCurrentProcess`,
`SetThreadAffinityMask`, and `GetCurrentThread`. A failed lookup returns 0.
It queries the current process affinity mask, then walks bits 0 through 31 in
ascending order. For each set bit, it pins the current thread to that processor
and calls `0x4BE320` again. The first nonzero result restores the original
process affinity mask and returns 1. If no processor probe returns nonzero,
the original mask is restored and the function returns 0. The Windows API
results and each per-processor arithmetic-probe result are runtime
observations, not values inferred by this portable source model.

`evaluate_msvc_p5_fdiv_test` records that ordered decision flow from an
`MsvcP5FdivProbeObservation` without loading libraries, invoking APIs, or
changing thread affinity. `initialize_msvc_fdiv_compatibility_flag` models
the store performed by `__fpmath`; callers can pass its result as
`fdiv_compatibility_enabled` to arithmetic models such as
`advance_ncar_sample_dynamics` and `update_scene_wheel_auxiliary`.

`divide_with_vc98_fdiv_compatibility` shares the recovered common
finite-normal significand-table and three-bit gate approximation. It is used by
NCAR derivative/ramp divisions and the `0x462600` normalized cross-product
components. It does not reproduce exceptional/subnormal paths or exact x87
control-word and 80-bit rounding.

Evidence:

- `0x4BE370`: `ev_b92bc673db5c616d1eef1b7ddaf6b45235c5306cc3547ea907fcc298d18abaf9`.
- `0x4BE320`: `ev_d0c0ff586ee027a39e6e5d88bc248808c2b424b496ea3932d32bc9c7ee98f982`.
- `0x4BC260`: `ev_c8b7fa6a07f3bb117330256e850982d888c4cb01df8cabf2abb4f36e6fb83d7b`.

The Ghidra pseudocode for `0x4BE320` simplifies the result to constant zero,
while its complete assembly performs the x87 division, multiply-back,
subtraction, and compare against 1.0. The model treats the returned bit as an
explicit observation because reproducing this test requires the target's x87
execution behavior.
