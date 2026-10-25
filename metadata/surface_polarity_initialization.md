# Surface polarity initialization and shared call stream

## Artifact and scope

Static observations below come from `race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`, analyzed
as x86 PE with Ghidra 12.1.4. These observations describe code and call paths;
they do not establish a particular runtime's clock samples or thread schedule.

## Initialization path

- `0x417270` calls `0x419380`, then a 10,000-unit timing setup, then
  `0x417350`. Its resolved callers are the interactive entry `0x401460` and
  dedicated-server entry `0x49D3E0`. Both reach polarity initialization before
  mode dispatch/main-loop work. Evidence: `ev_cdc15079c08a329e4bf0c562853e45eab5cc47bb166c33cc32bcb46693f2fef5`.
- `0x419380` registers/gets the named `"random"` critical-section slot via
  `0x4150C0`, stores it in `0x4F0A9C`, then calls `0x4193C0`. The lock registry
  is not a random-number generator. Evidence: `ev_bed9b066666fffb4d298ccd5da2ce834f37e9c5d7580c80056e44a6c75e41928`.
- `0x4193C0` calls `0x413BD0` twice. After each call, `CDQ` sign-extends
  `EAX`, replacing the helper's `EDX` high word; `IDIV 0x7530` therefore takes
  the signed low-32-bit timer value modulo 30,000. The first sample is pushed
  before the second, so initializer stack argument 0 is the second remainder.
  This exact reduction and push ordering are modeled by
  `make_ncar_surface_polarity_seed_pair_0x4193c0`. The table initializer
  writes 97 float entries at `0x4F0910..0x4F0A90`, then initializes phase
  `0x4F0908`, increment `0x4F0A94`, wrap value `0x4F0A98`, indices
  `0x4F0AA4/0x4F0AA8` to `0x60/0x20`, and initialized byte `0x4D3C58`.
  Evidence: `ev_3bb02f1aa8da6bfea97f61f57806c157b2294c12161f786d8864577cf427a295`, `ev_e4863482e00c2d90193abc28b0d55365db2fc169b52bc487a057b46678bd7999`.

  Evaluate the initializer's stack offsets at each read. Let `S` be its entry
  ESP. At `0x419446` and `0x419456`, before lock-call arguments are removed at
  `0x41945A`, ESP is `S-0x30`; `[esp+0x34]` reads `S+4`, argument 0 (the second
  timer remainder). Later, after `add esp,0x0C`, ESP is `S-0x24`; the read at
  `0x419489` of `[esp+0x2C]` reads `S+8`, argument 1 (the first timer
  remainder). This confirms both timer inputs and their order. Applying the
  later ESP value to the earlier read would produce an incorrect stack trace.
- `0x413BD0` selects calibrated TSC, QPC, or `timeGetTime()` according to
  timer setup globals. `0x413610` establishes those modes and `0x416360`
  reaches that setup. The exact runtime samples depend on machine timing and
  are not available from static analysis. Evidence:
  `ev_72f206f0e4759b827c8836afbe8d15284052fcb72d65d2e809e89e3afb7e0409`,
  `ev_5ec0098d72e412d56f258a7378c59cc52938c007f5c90b824a72bc5892f6ad2c`.

## Table generation facts and current limit

The initializer is deterministic for its two signed seed arguments. It uses a
24-step binary-fraction recurrence for each of 97 entries, x87 intermediates,
`__ftol` truncation toward zero, and binary32 stores. The instruction-level
trace maps the loop's 32-bit product, x87 accumulator, half-step, and compare:
the candidate product uses 32-bit `IMUL` wrap semantics; `__ftol` temporarily
sets x87 rounding control to truncate before `FISTP`; `FCOMP ST2` compares the
loaded 64 against the accumulated candidate and its carry flag selects whether
the current half-step is added. The step begins at 0.5 and is halved 24 times.
Constants used include 1/177, 177, 1/169, 178, 169, 179, 53, 1, 64, 32, and
0.5. Exact generated output has not been checked against a native table
fixture. The x87 register-stack trace is unresolved: `0x4BD45A` jumps to the
descriptor-driven `fmod` CRT dispatcher at `0x4C0580`; the adjacent
`0x4BD464` sequence is not reached by fallthrough. The dispatcher saves both
operands without popping them, then selects class- and exception-specific
handlers through the descriptor table. Its first handler pointer at
`0x4E98E0` targets `0x4BD464`, whose `FPREM` loop ends with `FSTP ST(1),ST`,
leaving one remainder and reducing stack depth by one. This is a likely normal
finite-input path, but the status-class index selecting that slot has not been
verified; therefore the initializer's complete x87 stack trace and exact
`std::fmod` equivalence remain unresolved. The startup precision path is also
partly resolved: CRT entry `0x4BD690` calls
`0x4BF720`, which invokes `__fpmath` and `__setdefaultprecision` (`0x4BE300`).
Its `__controlfp(0x10000,0x30000)` call sets x87 precision control to 53
significand bits. The pre-initializer game setup `0x415C60` changes
exception-mask/infinity-control fields, but its mask does not include
precision or rounding control. The other precision setter, `0x415C80(1)`, is
reached through `0x447980` → `0x4014D0`, after `0x417270` has completed the
initializer. Thus binary64 precision is supported by the observed startup
path; the rounding-control bits and initial x87 stack state are not explicitly
established by these routines. Do not claim byte-for-byte table parity until
the port is checked against a native fixed-seed table fixture. Evidence:
`ev_e4863482e00c2d90193abc28b0d55365db2fc169b52bc487a057b46678bd7999`,
`ev_1f0144ba5ff39e36ced30debe4cd8e2dd002c63683863cfffa1cf6bf82c65226e`,
`ev_2b0070a9b6408f68a767b443f65869aaaa4cdf495898834d6094addf0835c787`,
`ev_bd81caa3c16b9850abb706f99758614c9bd9b6f5d94e30edee61fc0aa582e737`.

The `fmod` descriptor and handler pointer are directly observed in
`ev_831f99655f66f55f6b5d1d67f4e9d8ab614b346d99a3c2366f48895f5edf84f2`;
the `__trandisp2` class-table dispatch is observed in
`ev_4818ba019dd88b819e85fc77487ad6e489a537923d6706150559fb44f4fb5d58`.

The precision-path call-chain evidence is `ev_5a5d630562475ee9e0a0e94a7961357c5b3b2c0e46c6817cbd917899342dd71b`
(CRT entry), `ev_528ea60e8a455ee3e00b8b275c96ca8152c35467fa67f6bd80760ceba03d9755`
(`__fpmath`), `ev_b9e5367c63490dd289f874fc6d75fefba7b58c74818342adc82c55589f88b940`
(`__setdefaultprecision`), `ev_ebdb653dcf9cd568c625e8922d6b1f09c817fd8fb6dcfa5e34c4ef126130df5c`
(`0x415C60`), and `ev_67e0c02bf7c8335ad0fa252f59c3e96f8b324a5018876ad6da55254b6e4aa0b9`
(`0x447980` call chain). These establish the precision-control path, but not
the initial rounding mode or a native generated-table output.

## Runtime consumer order

`0x419620` advances the process-global state under the registered critical
section. `0x4193F0` calls it and scales/converts its result. The wheel update
path `0x431150` calls `0x442530` four times sequentially per car update, but
at least 16 other resolved direct callers also consume the same stream. Calls
must not be modeled as independent per-wheel generators. Thread scheduling
can affect interleaving, so matching a run requires capturing or reproducing
the shared call order. Evidence:
`ev_3afbbf14b159826caa99a4d8315bfd22f427d866e42c6231e4e5775a75488608`,
`ev_6b1334751a6337d3165d366b0c495d29963640f958a57149aa3dfa596205c872`,
`ev_57280023502a2833f346abb76e73c8e606ca8e74e86c5ff1b9e352026e217206`.

## Implementation boundary

The repository models the per-call `0x419620` transition, `0x4193F0`
conversion, and the wrapper's signed low-dword seed reduction/order. The live
session still does not own the clock source, table initializer, or shared
process-wide stream. Next exact steps are to finish the x87-compatible table
generator, capture native tables for fixed seed pairs when the original disc
can be mounted through its intended path, model the timer source behind an
injectable clock service, and route reconstructed consumers through one
locked session-level stream while recording unresolved call-order differences.
