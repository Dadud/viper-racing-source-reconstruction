# Reconstruction notes

This is a source reconstruction written from shipped PE code and data. It is
not the original source and does not yet reproduce the game. Addresses use the
preferred image base `0x400000`.

## Binary roles

- `GameRoot/Viper Racing.exe` is a 32-bit GUI bootstrap executable. Its entry
  point at `0x4024C0` calls the startup checker at `0x401200`.
- `GameBin/race.bin` is the 32-bit runtime module. Its entry point is `0x4BD690`.
- Both binaries have no exports. Full disassemblies are retained in `disasm/`.

## Launcher control flow established from disassembly

1. `0x401200` creates a named semaphore and exits early if creation fails or
   the last error indicates the named object already exists.
2. It checks the first startup switch and invokes helper `0x401860` if set.
   It then checks `-no3dfx` (`0x40624C`) and `-nocanary` (`0x406254`). The
   check table contains routines `0x401640`, `0x401670`, `0x401690`,
   `0x4019C0`, `0x4017C0`, `0x4017B0`, and `0x401730`; the canary switch skips
   this list. The `-safe` flag (`0x406260`) is stored for later launch logic.
3. It calls helper `0x401000` to choose a module name, formats a path/command
   string with helper `0x402210`, then calls `0x401880` to resolve/check the
   data directory. That lookup uses `HKLM\\Software\\Sierra On-Line\\Viper
   Racing`, value `DataDirectory`; helper `0x40192C` falls back to the
   executable directory plus `Data`. Failure displays the string at `0x40627C`.
4. When startup proceeds, `CreateProcessA` is called via IAT `0x4091B0` at
   `0x401472`. The executable and command line are prepared separately. The
   parent waits 2500 ms, checks the child exit code, and dispatches result
   codes through a jump table. Exact helper formatting and argument semantics
   remain under analysis.

## Startup check details

- `0x401640` queries the OS version. `0x401670` calls a system query with
  parameter `0x13`; exact API name/meaning is not yet confirmed.
- `0x401690` calls `GlobalMemoryStatus`. It compares memory values against
  `0x01700000`, `0x01F00000`, and `0x02700000` (23, 31, and 39 MiB) and emits
  different warnings. Do not reduce these to one minimum-memory requirement.
- `0x4019C0` reads DirectX setup version data and compares a DWORD at offset
  `0x10` against `0x00040005`; failure displays “This product requires
  DirectX 5.0 or better.”
- `0x4017C0` asks a translation/resource helper for IDs `0x41` through `0x5A`,
  formats `"%sdata\\%s"`, and tests candidates. If none pass, it reports
  “You must have the %s CD inserted to play.” The helper predicate and exact
  meaning of resource IDs still need tracing.
- `0x401730` runs conditionally when a global flag is set. It calls
  `GetDiskFreeSpaceA`, multiplies sectors-per-cluster, bytes-per-sector, and
  free clusters, then checks against `0x01400000` (20 MiB).
- Startup result codes map to messages including DX requirement, unsupported
  3D texture mapping, unsupported 3D video card, and video error. The precise
  producers of those result codes remain to be mapped.

## Engine graphics evidence

- `0x458470` calls internal wrapper `0x4BAE26` to create a DirectDraw object.
  Failure logs “Unable to create DirectDraw Object” and calls fatal cleanup
  with reason 2.
- On success, it calls the object's `QueryInterface` with GUID data at
  `0x4C8AC0` (consistent with `IID_IDirectDraw2`), releases the original
  interface, and logs “Unable to get DirectDraw2 interface” if that fails.
- `0x458940` queries using GUID data at `0x4C8B40`; failure logs “Can't find
  IDirect3D2”. The GUID and result type have not been independently resolved;
  the message alone does not establish the queried interface.
- `0x458E55` logs “can't query directdrawsurface3” on a surface interface
  query failure.
- `0x44E080` logs “Unable to initialize DirectDraw” if helper `0x44E460`
  fails. `0x4BAA77` is an intro/movie path with a separate DirectDraw error.

The C-shaped wrappers in `src/engine_init_notes.c` show the confirmed
DirectDraw/DirectDraw2 stages and a generic secondary `QueryInterface` call.
They do not establish a complete renderer sequence, and the later interface
identity remains unresolved.

The engine entry point at `0x4BD690` establishes an SEH frame, calls
`GetVersion`, stores extracted version bytes/words in globals near `0x4E99DC`,
performs CRT and environment setup, scans past the executable token, and
directly calls application-owned startup function `0x411F10`. Ghidra's
entry pseudocode does not show a performance-counter call. The whole-text
disassembly also shows a separate callback fragment in a gap between Ghidra's
entry body ranges that calls the disassembled handler code at `0x4C1300`.
That code maps floating-point exception codes to internal runtime values and
dispatches a category-8 callback. The startup handoff and its direct helper
leads are documented in
`metadata/engine_application_entry.md`.

The entry-called `0x4C14F0` walks the environment block, allocates a pointer
array, duplicates whole strings whose first character is not `=`, and
NULL-terminates the array. `0x4C1850` selects a dynamically resolved routine
and includes a wide-to-multibyte environment conversion path. `0x4C3AF0` is a
dynamic error/translation reporting function, not the application startup
handoff. The ASCII executable-token helper in
`src/engine_crt_reconstruction.c` is only an approximation; the binary also
checks multibyte lead bytes.

## Files and evidence

- `src/launcher_reconstruction.c`: startup control-flow fragments and
  reconstructed checks.
- `src/engine_init_notes.c`: DirectDraw object and interface acquisition.
- `disasm/game_.text.asm`, `disasm/engine_.text.asm`: whole `.text`
  disassemblies.
- `metadata/pe_report.md`: PE layout, imports, hashes, and entry points.
- `metadata/strings.md`: printable strings with RVAs.
- Candidate block starts are in `metadata/*_function_candidates.csv`; they
  include branch destinations and are not verified function boundaries.

## Remaining work

The largest remaining task is engine logic: trace the entry point at
`0x4BD690`, classify large functions, and follow game-state, resource, physics,
and rendering paths. For the launcher, resolve helper routines `0x401000`,
`0x401880`, `0x40192C`, `0x402210`, `0x4023D0`, and the startup error-code
producers. Names in reconstructed C describe observed behavior and may change
when those helpers are fully understood.
