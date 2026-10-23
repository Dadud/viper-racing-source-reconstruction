# Engine application entry evidence

## Artifact and method

- Artifact: `GameBin/race.bin`
- SHA-256: `f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`
- Architecture: x86 PE, preferred image base `0x400000`
- Analysis: Ghidra 12.1.4 through REA; functions were analyzed individually.
- Entry function: `0x4BD690`, body ranges `0x4BD690..0x4BD7D4` and
  `0x4BD7FC..0x4BD815` (non-contiguous Ghidra function body).

## Observed entry behavior

The decompiler output for `0x4BD690` shows the following sequence:

1. Establishes an x86 structured-exception frame.
2. Calls `GetVersion` and stores extracted bytes/words in globals near
   `0x4E99DC` through `0x4E99E8`.
3. Calls internal routines at `0x4C1EA0`, `0x4C1CC0`, and `0x4C1CB0`.
4. Reads `GetCommandLineA`, builds environment-related state through
   `0x4C1850`, invokes `__setargv`, and calls `0x4C14F0` and `0x4BF720`.
5. Skips the executable token, with a quoted-token path that checks multibyte
   lead bytes, then reads `STARTUPINFOA` and the current module handle.
6. Calls `0x411F10` with the module handle, a zero second argument, and the
   remaining command-line text. This is the identified application-owned
   startup handoff.
7. Passes the returned value to `0x4BF750`.

The entry pseudocode does **not** call `QueryPerformanceCounter`; its version
globals come from `GetVersion`. The whole-text disassembly also shows a
separate callback fragment at `0x4BD7E3..0x4BD7EF` that calls `0x4C1300`.
That fragment falls in a gap between the two Ghidra-recognized entry body
ranges, so it is absent from the decompiled entry body. Treat the disassembly
as evidence for this callback path and keep its exact SEH role separate from
the normal application handoff at `0x411F10`.

The code label at `0x4C1300` is not a Ghidra FunctionManager procedure entry;
it is present in the whole-text disassembly. Its visible code looks up a
registered runtime callback, maps observed floating-point exception codes to
internal values `0x81`, `0x82`, `0x83`, `0x84`, `0x85`, `0x86`, and `0x8A`,
then calls the callback with category `8`. The source fragment
`src/engine_exception_reconstruction.c` records the mapping only.

## Application handoff at `0x411F10`

Ghidra identifies a contiguous function body `0x411F10..0x412046`. Its
pseudocode stores the module handle and command line in globals, recognizes
the `-dedicated` switch, and gates startup through `0x4120A0` and `0x412050`.
The non-dedicated branch calls `0x412270`, `0x416080`, `0x416360`,
`0x412120`, and `0x401280`; on a successful later condition it may destroy a
window and calls `0x401460`. Cleanup paths include `0x416450`, `0x4161A0`, and
`0x412110`; the shutdown call order and both handle-close helpers are
documented below. Effects of the cleanup subsystem callees remain open.

`plan_application_startup_handoff` now composes this observed control flow
from explicit OS/helper outcomes. It records the module and command-line
globals, dedicated-mode/port extraction, both early gate results, branch
calls, conditional frontend entry, shutdown and finalizer reachability. The
model now composes `plan_config_directory` and its observed directory-creation
results. Invalid executable paths reach the modeled `ExitProcess(6)` branch;
successful path derivation gates command-line parsing, while directory-creation
failures are retained because the native caller ignores that helper's return.
The modeled exit path prevents later frontend, shutdown, and handle-finalizer
calls from being marked reached. Successful config-path setup now feeds the
existing `0x401280` parser model, with directory-change success supplied as an
OS observation. The model does not perform native window, semaphore,
filesystem, or process operations. Evidence:
Ghidra dossier for `0x411F10`, `ev_1dfe423eb4dea801ca131c6ee81df3a348a4718c84b1adc88b40741dda97339a`.

The adjacent `0x416080` call opens `\\.\vexed.vxd` with `CreateFileA`,
access `0xC0000000`, share mode 3, `OPEN_EXISTING` (3), and flag
`0x04000000`; it stores the returned handle at `0x4D2F20`. A valid handle
enters `0x4160C0`, which scans PE section names for `.text`, `.rdata`, and
`.data` and reaches a stub returning 1 for matching names. The handoff model
now records the open parameters and whether that scan is reached. Evidence:
`0x416080` `ev_bc505c04e68af48095b20e9f33bfd2d2c046e2257197f0620aeed430fe7df582`,
`0x4160C0` `ev_4412c28011e4dfa27b0396e1d044c460b28b82aaebbbe52fdf658b8ecf46a9b4`,
`0x416150` `ev_8a1858527d3c8f2a720fbd454c21c8a096b8a84db16e3f58e0f43339c1592d06`,
and `0x416140` `ev_c73ca016fda41ccdce9c269516b636be4da85ed8719aa44ddd6158b4780edd5e`.

Strings directly referenced by this function include `-dedicated` and
`Long User Directory (%d), I am scared.` These support the switch and error
path observations, not broader feature claims.

### Command-line switch scan at `0x401280`

Ghidra identifies `0x401280..0x4013FF` as the command-line parser called on
the successful non-dedicated path. It scans each character of the remaining
command-line string and treats either `-` or `/` as a switch prefix. The option
comparisons for `location`, `tri`, and `grid` are case-sensitive `strncmp`
checks; the one-letter options are case-folded to lowercase.

- `-location...` sets mode global `0x4CF5B0` to 4 and passes the bytes after
  the eight-letter option to `0x4016B0`.
- `-tri` sets `0x4DC1B4`; `-grid` sets `0x4DC1B0`.
- `-dPATH` passes `PATH` to the `__chdir` wrapper at `0x411990`. Before that
  call, the parser writes a NUL over the first following ASCII space, so the
  remainder of the mutable command line is removed. Failure is reported with
  `Can't change dir to %s`.
- `-ub`, `-ud`, and `-ur` set `0x4CF5B0` to 1, 2, and 3. Other characters
  after `-u` call the invalid-programmer-flag logger but do not change the
  parser's forced-success low byte.
- `-z` sets `0x4CF5BC`, which the parser first clears on entry. `-1` calls
  `0x44E120`, which sets `0x4D9CF0`.

The parser does not clear the mode, tri, or grid globals on entry; these values
persist unless the corresponding switch writes them. The portable model is in
`include/viper/startup_command_line.hpp` and `src/startup_command_line.cpp`.
Evidence: Ghidra 12.1.4 function dossier for `0x401280`, evidence
`ev_17a7182e5d8149ce261c2ea04d841c258d57dc02f4afd70949ec0ba7dacc7a4e`; helper
dossiers for `0x4016B0`, `0x411990`, and `0x44E120`.

### Frontend initialization at `0x4014D0`

The non-dedicated mode-0 route enters `0x4014D0`. It calls `0x447980`,
`0x4482B0`, and `0x4479F0(2)`, then queries the registry key
`Software\Sierra On-Line\Viper Racing` value `CDDirectory` through
`0x4015E0`. If the lookup succeeds, it appends the literal `intro.avi` to the
returned directory bytes and calls `0x4BA9E0`; the binary appends the filename
without inserting a separator. The registry helper requests at most `0x104`
bytes and uses the API status as its success result; it does not check the
registry value type. The append has no explicit remaining-buffer check, so the
portable plan flags output paths that exceed the 260-byte local buffer. It
then writes twelve floats at `0x5081A8`
through `0x5081D4`, with 1.0 at offsets 0, 0x10, and 0x20 and zeros elsewhere.

The remaining call order is `0x46AA00`, `0x418500`, `0x46B7D0`, three calls
to `0x417740` with `common.res`, `ui.res`, and `drivers.res`, then
`0x406420`, `0x40D110(1)`, `0x4714E0`, `0x49B0F0`, `0x4B6310`, `0x41C880`,
and `0x412E10`. The portable plan records this sequence and exposed arguments;
subsystem internals remain separate reconstruction targets.

Evidence: Ghidra 12.1.4 function dossiers `0x4014D0`
(`ev_db320c0ccd525a298f3e116e49227e6dac1a1e031b3187dbe10574f939c3bd0f`)
and `0x4015E0`
(`ev_61435825dbf9831335ca237ab03e9d756fbb0bcb4c209caddef720efc85a295a`).
The model is in `include/viper/frontend_initialization.hpp` and
`src/frontend_initialization.cpp`.

## Direct startup helpers and frontend loop

### Shared panic reporter at `0x411000`

`0x411000` supplies the literal prefix `Panic : ` and forwards the caller's
format pointer and variadic arguments to `0x411020`. The latter copies that
prefix into a 255-byte stack array followed by a one-byte local initialized to
zero, appends the formatted message with unbounded `_vsprintf`, then checks
the adjacent byte before attempting any report output.

If both `0x4ECFD8` and `0x4ED010` are nonzero, it brackets three calls to the
registered log dispatcher (`0x410D90): separator, report, separator.
`0x4151A0` and `0x4151F0` acquire/release the selected module's logging
critical section. If that logger gate is false and `0x4D1810` is zero, four
calls to `0x4111C0` do nothing; if the flag is nonzero, it displays the text
with caption `Last-Resort Log` and flags `0x30`. All paths reach
`0x416020`, which writes 1 to `0x4D2978` and then executes
`MOV byte ptr [EAX], AL` after clearing EAX. The null write is observed, but
its process-level exception handling is not established.

The source-shaped rendering is in `src/engine_panic_reconstruction.c`.
`metadata/engine_panic.evidence.json` preserves the function dossiers and
the exact terminal instruction observation. Evidence IDs: `0x411000`
(`ev_33e3e536e1f2727e9ac6c24e63969e9720ddc0d6590e3ad31c2ce3978cc79c58`),
`0x411020`
(`ev_aaf9730aacfda36e266417948c8961a94c16ed928594b9ab352fa6090a6db793`),
`0x4111C0`
(`ev_48133e65ee4ff152ce9c60173fb1cd7ce7828a1c9645eaf42677f170125cba8a`),
`0x410D90`
(`ev_6882a9b094665a9921e0544eb99565ff5917fc56cb6132adc849d80de95e991f`),
`0x4151A0`
(`ev_30848d2367ab0429c70e1db9091fa1fe515b8cead56ef6f34957ecb9f85ce484`),
`0x4151F0`
(`ev_67a14dbb54d6a14a4540f8fb92941bf2cc48a97ff6a72c361a93c1b8b93c8ec5`),
and `0x416020` (`ev_499072ef61decba04cbbd0268958ec48ac187e0f76b939dd341d51fea4cc41ea`).
The lock wrappers' direct API calls are confirmed by `0x415350`
(`ev_b66a696aa46af4cb0e33fc52cfd6ed987d9bc64c3b87b53d364f7f3fd8350d26`)
and `0x415360`
(`ev_3e3b5afd5ba59c3e54ebd8c7c5c223aac9418126d2681fa67f57a97a615a9133`).

### Shutdown sequence at `0x416450`

When `0x4D2FBC` is nonzero, `0x416450` calls, in order, `0x416C70`,
`0x4144E0`, `0x413080`, `0x413CC0`, `0x412BE0`, `0x414430`, `0x415C20`,
`0x411230`, `0x416890`, `0x410DD0`, `0x411280`, `0x4149F0`, `0x414FA0`, and
the thunk at `0x413BC0` (target `0x4165CF`). It then clears `0x4D2FBC` and
returns the thunk result. When the flag is zero, it calls panic helper
`0x411000` and returns that helper's result without entering the cleanup list.
The portable `plan_startup_shutdown` records the branch, call order, return
value, and flag mutation; subsystem cleanup effects remain separate targets.

Inside `0x416360`, the `0x410AA0` result is not a free observation: its
analyzed implementation forces a successful low byte after selecting logger
callbacks, so the caller's syntactic failure branch is unreachable through the
observed implementation. The initializer proceeds to `0x416680`, `0x4154A0`,
and heap creation, whose `0x414110` outcome remains an external observation.
That helper calls `HeapCreate(1, 0x1400000, 0)`, initializes its secondary
allocator and clears two counters on success, and conditionally logs
`MemBegin: using VXD allocator.` when `0x4161C0` reports that allocator active.
The portable initializer result records these nested effects and parameters.
The preceding `0x414840` helper registers a `test timer` callback at 100 ms,
polls a completion byte up to 30 times with 200 ms sleeps, reports
`Tried to create MMTimer, did not run` if it never fires, then stops the timer
and clears the byte. Its portable `timer_probe` records that bounded wait and
panic condition; worker-thread creation and callback scheduling remain explicit
observations.
Evidence: `0x416360` `ev_ac5a552b3d8fa6002ade17479626397ad2c9e110105697b013c1bd83ebae79ac`;
`0x410AA0` `ev_ae19eca17c27a122dc3cd91004d429fb6622232725618a620c3d517d96298536`;
`0x414110` `ev_108db34feb7562c8b6ce7636aae5f5f67bf8329302dd0896c61f1dcf61e74310`.
Timer evidence: `0x414840` `ev_7338b0a9717f74448ff5a1ac8958bd3b6b51eddc4295e69366f9dcb871ca276e`,
`0x414BA0` `ev_03feeed66afda725b5400ede1ba208981da65cabdf9212eb5bef45e0306ca745`,
`0x414E70` `ev_c25acb9bac08cd931ed87043fd63eba24394f23a24b201cd20b334472080206e`.

The following `0x4111F0` registers the `File` named lock at `0x4D1A50` and
initializes a 32-entry file-handle registry beginning at `0x4ED020`, stride
`0x104`: byte offset 0 is cleared, and the handle dword at offset `0x100` is
set to `0xFFFFFFFF`. This is not a task table: `0x4112E0` stores a path in the
record and its Windows handle at `+0x100`. The portable registry state now
tracks the allocation count, paths, and handles. Evidence:
`ev_0952de40d83f81e75e4ac2bb342f3789018867ad9407ba3ee860b675e05bdfb3`.

Startup exception handling calls `0x4154A0`, which installs the unhandled
exception filter at `0x415680`, masks floating-point exceptions through
`0x415C60` by ORing the prior control word with `0x7F` under mask `0x8001F`, and
opens `c:\\except.log` through `0x4112A0`. That helper requests write access,
allows read sharing, uses `CREATE_ALWAYS`, and registers a successful handle
through `0x4112E0`; a failed open returns index zero. The model keeps an
unobserved handle distinct from an observed failed-open sentinel.

The adjacent `0x4154E0` helper opens the executable read-only, creates a
read-only mapping, and maps its view. It finds the greatest
`PointerToRawData + SizeOfRawData` across PE sections and returns
`mapped_base + maximum_raw_end` when bytes remain after that boundary, keeping
the view, mapping, and file open. If no trailing bytes remain it logs that no
mapfile exists and releases all three resources. Failure branches preserve the
observed close order. `map_executable_overlay` models this from section raw
ranges and explicit OS-operation outcomes; pointer values are opaque tokens.
Evidence: `ev_229ab9287b7ad22e09f11e6a2a3ba3390272f0d8faab61cddb9671f6de5206c0`.

The named-lock helper `0x4150C0` holds the manager lock while searching up to
16 entries with case-insensitive name comparison. A duplicate triggers panic
but processing continues; below capacity the registered count advances, while
a full registry reuses the first null-pointer slot or reaches its panic path
when none exists. It allocates the underlying critical section through
`0x4152C0` and stores the name/pointer pair. The portable
`register_named_critical_section` models those branches and reports the
otherwise invalid `-1` index store without performing an out-of-bounds write.
Evidence: `0x4150C0` `ev_0b14f12122d4b002313b409ba3e5744723b355b465bbee9767a4740b8eee92fb`,
`0x4152C0` `ev_e4e68c82dc0f62e5aa3bdad1ee38a720532ad2b8a71714ea32157752012d154b`.
The startup initializer now invokes that model in observed order for names
`task`, `File`, `Log`, `Mono`, `BG`, and—after successful heap creation—`Mem`,
carrying one registry and critical-section pool state through all six calls. The
strings were recovered from the mapped bytes at `0x4D25E4`, `0x4D1A50`,
`0x4D18BC`, `0x4D18C0`, and `0x4D24BC`; address-map evidence IDs are
`ev_0d940f6dc067168c2d83671fb19744ea087ae5b148c800d713a5d72b951c1645`,
`ev_474227efd03674698a6fdb00eb8564fa135324a4b0f3fd1cc251cc74162596ce`,
`ev_2df9ac9f8f6d09c1b2000d011a8bb437a3065dce9d63c94974f06bbc6fa1bd12`,
`ev_9020df01d326979ae5f719dcdf46cfdcf73bb6bf927a70ad275989859105f156`, and
`ev_115063572a9011cd19565fa8a909a7c467ae904cf5bdb35ad426f7bd5290ab28`.

After `0x410AA0` returns, `0x416680` registers the `BG` lock, creates a
`BGTask` worker using callback `0x4166D0` and parameter `0x10`, then stores
`0x416720`, mode 2, and active value 1 in globals `0x4EFFF0`, `0x4EFFF4`, and
`0x4D32A8`. The model records the thread-creation outcome and one-based task
index as observations because they come from `CreateThread` and the task
allocator. `BG` bytes at `0x4D32B0` map to file offset `0xD1430` (mapping
evidence `ev_eb30f7e02894ea328760826b67ade5a4af2949efa97d0db75179cb3ce66315c4`).
Function evidence: `0x416680`
`ev_b7e437fd564e292789739383c428f4da7507d2db5e0351880af30a9e297e8c23`.

Evidence: Ghidra 12.1.4 function dossier for `0x416450`,
`ev_5db23abcca7bda2b42e089dc76f68508ee984166231759bb6d9114193c902c0c`.

### Handle finalizers at `0x4161A0` and `0x412110`

`0x4161A0` checks the VxD file-handle global `0x4D2F20` against
`INVALID_HANDLE_VALUE` (`0xFFFFFFFF`). It calls `CloseHandle` only when the
global differs from that sentinel, then writes the sentinel back. The close
result is ignored. By contrast, `0x412110` always calls `CloseHandle` on the
single-instance semaphore handle at `0x4D1CB8`; it neither checks nor resets
the global. The portable handle-finalizer functions preserve those different
rules, including a null handle reaching the unconditional close call.

Evidence: `0x4161A0` dossier
(`ev_db54add9bc56423bf72e7e1987b755ddc153d4594962c2a66acc6b4c184ed3f1`)
and `0x412110` dossier
(`ev_ff3a04cfee74840fb68cd018c0c88381aaed1aa97877e184d0e1f7f5a85bfc58`).

### Config directory derivation at `0x412120`

`0x412120` calls `GetModuleFileNameA` with the module handle stored at
`0x4D1CA8` and a 260-byte buffer. A zero result logs `Can't get module file
name`; the common failure tail logs `Game not installed properly--can't find
registry information` and calls `ExitProcess(6)`. On success it removes the
executable name using the last backslash, then requires the preceding path
component to compare case-insensitively equal to `\Data`. If that check
fails, it logs `Can't derive config path (%s)` followed by the same fatal
message and exit code.

For a valid `...\Data\executable` path, the routine removes `\Data`,
appends the observed `\Config\` suffix, and writes the resulting path to
`0x4EF0C0`. It passes this path through `0x412260` to `0x4119E0`, which
attempts `CreateDirectoryA` for each ancestor from the application root toward
the config directory. `0x4119B0` treats `ERROR_ALREADY_EXISTS` (183) as
success. The `0x412120` caller ignores the directory helper's return, logs
`Config Dir: %s`, and forces its own low byte to one after a valid path was
derived. The portable plan records candidate paths; `evaluate_config_directory_creation`
models the helper's low-byte result from observed API outcomes and leaves it
unknown when those observations are incomplete. The outer initializer ignores
that result.

Evidence: `0x412120` dossier
(`ev_5fb5acdedd1a8a3c9bc59cc0444910d097ac73efe2af1f244c4b4f7ab6120340`),
`0x412260` wrapper (`ev_977018d0bfe112fc06ef27de2c82124685689b02afbb734cc9662e0785a57a30`),
`0x4119E0` path walk (`ev_764bebdc892bc7c2805213dc064a99e6fdc7e1799c018c28680f7c343b4951bf`),
`0x411A40` path prefix helper
(`ev_f47fb045e7ade51936e15cd7c03dd54080113dc9d59067a2c063a08916acee88`),
and `0x4119B0` directory creation check
(`ev_97e3bc0d86e9e81cd4a358a2d01373d1b976726f6e1677952c754639e18a3ad6`).
The source model is `include/viper/config_directory.hpp` and
`src/config_directory.cpp`.

- `0x4120A0` creates the named semaphore `MGI Viper Racing 1998`. A null
  handle reaches panic helper `0x411000`, after which the observed code still
  reads last error. On `ERROR_ALREADY_EXISTS`, it searches using class
  `Viper_Racing_Window` and title `Viper Racing`, restores a found window with
  `ShowWindow(..., 9)`, closes the newly returned handle, and returns a value
  whose low byte is zero. Other last-error values return with low byte one.
  The caller tests that low byte before proceeding.
- `0x412050` creates a second named semaphore, `MGI Viper Racing 1998
  Canary`. Only a successful creation followed by `ERROR_ALREADY_EXISTS`
  returns low byte one; that branch returns without closing the handle. A
  newly created semaphore is closed and then the warning message box is
  displayed, while creation failure also reaches the warning. Both warning
  paths mask the result's low byte to zero. This establishes the observed
  named-object check, not the complete copy-protection behavior.
- `0x412270` queries screen metrics, registers class `MGILoadingWindow` with
  zero class/extra styles, cursor resource `0x7F00`, and background resource
  `6`, then creates a 300-by-`0x175` window centered with integer division.
  The creation uses extended style `0x80` and style `0x80800000`. Only a
  successful class registration reaches `CreateWindowExA`; only a nonnull
  window handle reaches `ShowWindow(..., 5)` and `UpdateWindow`. The title is
  passed through pointer `0x4D1EA0` and remains unresolved. `plan_loading_window`
  records the numeric geometry and branch effects.
- `0x416080` opens `\\.\vexed.vxd`; its caller discards the Boolean result.
  On success, `0x4160C0` checks the executable's PE section names and calls a
  stub that returns 1 without writing section bytes. The global handle is
  closed by `0x4161A0`. The VxD's useful role is in the optional allocator path
  described in `legacy_vxd.md`; ordinary allocation already falls back to the
  Windows heap.
- `0x416360` sequences several initialization helpers, including
  `HeapCreate(1, 0x01400000, 0)` through `0x414110`. That routine logs when the
  VxD allocator is available; otherwise allocation uses `HeapAlloc`.
  Its `0x4D2FBC` already-set guard and low-byte results control early panic and
  cleanup routes. After `0x413610` and `0x414F80` pass, it calls `0x414840`,
  `0x4111F0`, and `0x410AA0`. The logger initializer `0x410AA0` forces its
  return low byte to one, so this sequence continues through `0x416680`,
  `0x4154A0`, and `0x414110(0x01400000)` without an intervening failure gate. A
  successful heap call reaches `0x412430`, `0x413C80`, `0x412F90`, `0x4144C0`,
  and `0x4168F0`, then sets `0x4D2FBC` and forces the return low byte to one.
  Heap failure calls panic and `0x410DD0`, then common cleanup. The portable
  `plan_startup_initialization` records these observed call sequences and
  return gates; helper side effects remain separate models.
- The `0x413610` gate selects its calibrated TSC path only when `0x4165F0`
  confirms the EFLAGS ID bit can change, CPUID leaf 1 exists and reports TSC
  (EDX bit 4), and `0x4137E0` sees exactly one logical processor. It always
  queries performance-counter frequency. Frequency failure returns low byte
  zero on the TSC path; the QPC fallback installs its callback addresses and
  forces low byte one even if the frequency query fails (in that case it sets
  the stored frequency to zero). After a successful frequency query both
  paths query the counter; the fallback stores the low frequency divided by
  1000 at `0x4EF310`, records the counter at `0x4EF318`, and forces low byte
  one regardless of the QPC return. Its callback addresses are `0x4135F0`
  at `0x4D235C`, `0x4135E0` at `0x4D2350`, `0x4D234C`, `0x4D2354`, and
  `0x4D2358`, and `0x413600` at `0x4D2360`. The calibrated branch records its
  initial RDTSC value at `0x4D2370` before querying frequency. When frequency
  succeeds, `select_clock_backend` now composes the calibration calculation and
  exposes both stored outputs in its result.
  `select_clock_backend` models these conditions and side effects and is now
  the source of `0x416360`'s first initializer gate.
- On the calibrated path, `0x413610` samples the low 32 bits of `RDTSC` around
  one QPC-frequency interval. It records the low 32-bit QPC overshoot beyond
  that interval, computes `cycles_delta / (1 + overshoot / QPC_frequency)`,
  truncates through `__ftol`, divides by 1000, and stores the result at
  `0x4D2364`; `0x4D2368` receives `0xFFFFFFFF / result`. The `+1` is the
  observed double at `0x4C7440`. `calibrate_tsc_clock` models the unsigned
  low-word cycle wrap and integer stores. Its long-double intermediate
  approximates x87 precision and does not reproduce the Pentium FDIV correction
  performed when `DAT_004E9754` enables it. A zero final divisor is reported as
  invalid; the native path would fault at the subsequent integer divide.
- The `0x414F80` gate obtains its pointer through `0x4152C0` and `0x4152E0`.
  The allocator uses 16 records with base `0x4EF9D0`, stride `0x1C`, and a
  one-byte used flag at record offset `0x18`. It allocates sequentially while
  the count is below 16, then scans for a free used flag; no free slot reaches
  panic helper `0x411000` and returns null. `0x4152C0` calls
  `InitializeCriticalSection` for an allocated record. `allocate_critical_section`
  models the slot choice, flag, panic branch, initialization call, and returned
  storage address. `plan_startup_initialization` now consumes this pool state
  at the `0x414F80` gate; exhaustion reaches the allocator panic and then the
  caller's second panic before its cleanup thunk. Pool release paths remain
  separate.
- `0x4014D0` initializes shared resources and references `intro.avi`,
  `common.res`, `ui.res`, and `drivers.res`. `0x401E10` is called repeatedly
  by the frontend loop and dispatches a returned state value to handlers at
  `0x40A410`, `0x40A570`, `0x4AA460`, `0x47B4E0`, `0x406950`, and
  `0x41C870` (state 7). The numeric states' user-visible names are not yet
  established.
- `0x4805C0`, called by `0x401E10`, constructs menu labels from the keys
  `Main_Menu:DefaultPlayerName`, `PlayerName`, `SingleRace`, `Career`,
  `Multiplayer`, `Replay`, `Options`, and `Exit`. It also references
  `t_main.stp` and `player.stp`, then returns the selection result from
  `0x472B30`. This confirms the main-menu contents, but not the mapping from
  each returned value to its displayed action.
- `0x401460` selects top-level frontend branches. Its state-0 path runs
  initialization, the repeated `0x401E10` dispatcher, then resource cleanup;
  other branches include substantial handlers at `0x402BE0` and `0x402AA0`.
  Their screen/mode labels still need to be recovered from data and runtime
  state evidence.

## Evidence limits

Decompiler pseudocode is an analysis representation, not original source.
Indirect and unresolved computed flows may be absent from reported call edges.
The frontend dispatcher is now a concrete path for continuing analysis, but
it is not yet the race simulation loop. Race simulation, rendering, and
subsystem boundaries still need separate tracing. Address references use the
preferred image base and are specific to the recorded binary hash.
