# Legacy VxD device and allocator path

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed statically with Ghidra 12.1.4 through REA; no device was opened.

## Open and startup behavior

`0x416080` calls `CreateFileA` on the observed string `\\.\vexed.vxd` with
desired access `0xC0000000`, share mode 3, creation disposition 3, and flags
`0x04000000`. It stores the `CreateFileA` handle in `DAT_004D2F20`; on success
it calls `0x4160C0`, then returns whether the handle differs from
`INVALID_HANDLE_VALUE`. Its sole direct caller, `0x411F10`, discards the
return value.

`0x4160C0` uses the executable image base returned by `0x412C50` to walk its
PE section table. For names `.text`, `.rdata`, or `.data`, it calls
`0x416140`, a one-instruction stub that returns 1. No section bytes are
changed by the stub. Thus this successful-open helper does not itself install
or patch a driver. `0x4161A0` later closes the handle and restores the global
to `INVALID_HANDLE_VALUE`.

## Device-control interface

`0x416220` wraps `DeviceIoControl` on the handle. The observed control codes
are passed as literal values, not produced by a `CTL_CODE` macro:

- Code 4 requests an allocation. `0x4161D0` supplies a four-byte input size
  and receives a four-byte output value.
- Code 5 releases an allocation using a four-byte input value.
- Code 1 registers the legacy timer from the worker entry at `0x414C30`; code
  3 unregisters it on worker exit. Failed calls report the corresponding
  timer-register or timer-unregister error strings. The worker is created by
  `0x414BA0`, which records its callback and interval in the per-thread table.
- The startup timer check at `0x414840` launches a worker named `test_timer`
  with callback `0x4149E0` and interval 100 ms. That callback sets a startup
  flag. The caller polls it up to 30 times with 200 ms sleeps, reports
  `Tried to create MMTimer, did not run` if it never becomes set, then stops
  and joins the worker through `0x414CF0`/`0x414B50`.
- The worker sets floating-point control state and thread priority 15, then
  repeatedly measures elapsed time around its callback. If the callback took
  less than the configured interval it sleeps for the remainder; otherwise it
  yields with `Sleep(0)`. After 60 overruns it resets the counter,
  sleeps 20 ms, and records an overrun for exit logging. It checks its stop
  byte at thread-record offset `+0x19` between iterations, unregisters the VxD
  timer on exit, and marks its thread-table state. `0x414CF0` clears the
  configured interval and enters `0x414B50`, which sets the stop byte, resumes
  a suspended worker when necessary, waits for its handle to signal, and then
  clears the thread record. The worker's interval clock comes from `0x413BD0`,
  which selects the configured high-resolution counter path or falls back to
  `QueryPerformanceCounter` or `timeGetTime` according to global state.
- Code 2 is called after a 15-second `WaitForSingleObject` timeout, with a
  four-byte zero input and a four-byte output initialized to `0x12345678`.
  The direct caller discards the returned value.

The resolved timeout call chain is `0x425880` → `0x4C5A80` → `0x4C5830` →
`0x4153A0` → `0x416300`. The last helper's return value is discarded before
the caller invokes `0x411000`. Codes 1 and 3 are direct calls inside the
thread-entry block at `0x414C30`, which Ghidra has not recognized as a
separate procedure; the whole-text disassembly confirms both calls.

## Allocator behavior and portable replacement

`0x414110` creates a Windows heap with `HeapCreate(1, 0x01400000, 0)`.
`0x414290` selects the device allocator only when the device handle is valid
and `DAT_004D24B8` is nonzero. `0x414170` requests payload size plus four bytes.
If the device allocator returns a non-null pointer, it marks the allocation
size negative; otherwise it falls back to `HeapAlloc`. Both paths fill the
allocation, including its four-byte header, with `0xA3`, then store the signed
size marker in the first dword and return the pointer after that header.
`0x414390` reads the marker: a negative value selects device release (code 5),
while a positive value selects `HeapFree`.

The existing program already has a heap fallback. A portable reconstruction
can use its host allocator for both allocation and release, while preserving
the four-byte size marker and `0xA3` fill if it needs binary-compatible
allocator behavior. `include/viper/legacy_allocator.hpp` and
`src/legacy_allocator.cpp` model that path choice and metadata; the portable
program does not need to open or emulate `vexed.vxd` to obtain memory. The
timer control calls are part of the observed thread lifecycle, but the device
driver's implementation and its timer response semantics remain unavailable.
The code-2 response value is ignored on the resolved timeout path.

`include/viper/legacy_timer.hpp` and `src/legacy_timer.cpp` model one worker
iteration's stop check and scheduling decision. The caller supplies the
signed elapsed-millisecond difference measured around its callback. On-time
callbacks sleep for the interval remainder; late callbacks yield, with a
20 ms backoff after each group of 60 overruns. Device access and thread
creation/join remain outside this portable decision model.
