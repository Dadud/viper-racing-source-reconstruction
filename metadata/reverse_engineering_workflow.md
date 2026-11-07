# Evidence-led reverse-engineering workflow

This workflow adapts practices used by incremental game decompilations such as
OpenRCT2 and matching-build projects such as Pokémon Emerald to this Windows
game reconstruction. It is for observable behavior parity; it does not require
identical source code or machine code. Original assets and captures remain
external to the public repository.

## Practices adapted from other game reconstructions

The OpenRCT2XP decompilation notes describe replacing one original procedure at
a time, retaining address-based names while semantics are unknown, using
call-flow and observed effects to identify functions, and clearly marking
intentional behavioral differences. OpenLara's project roadmap emphasizes
matching update cadence, engine architecture, and individual entity behavior
before layering in changes. Ghidra's Function ID documentation provides a
separate way to recognize known library routines from function hashes. These
are complementary practices: function-level work and explicit unknowns first,
runtime behavior checks where feasible, and recognition tooling only as a
lead that still needs validation.

Applied here: reconstruction work is organized around one native subsystem or
one observable outcome at a time; raw addresses and artifact hashes stay in
evidence notes; parsers preserve opaque fields instead of guessing semantics;
and development-only behavior is labeled separately from reconstructed
behavior. For each integrated change, the record distinguishes static binary
evidence, reconstruction-only consistency checks, and controlled original
comparisons. When the original cannot be run safely or reliably, that remains
an explicit acceptance blocker rather than being replaced with a guessed
golden result. For a reference example of the per-procedure discipline, see
[OpenRCT2XP's decompilation and contribution notes](https://github.com/AWBuchanan7/openrct2xp); for
engine-cadence and behavior goals, see [OpenLara's roadmap](https://github.com/XProger/OpenLara/issues/353); for
function-hash identification, see [Ghidra Function ID](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/FunctionID/src/main/doc/fid.xml).

When another project has already documented the same game's assets, pin its
commit and treat its format claims as a second evidence stream. Check the
reported fields against this project's supplied bytes and native call path,
preserve differences in confidence, and write a small parser around the
recovered layout instead of importing an entire unrelated toolchain. This was
applied to TEX and FNIM using the
[Viper Racing Modding `experiments` reference at commit `e8435dd`](https://github.com/HerbFargus/viper-racing-modding/tree/e8435ddce817eb37502d1dd1080b64ee27f44515).

For powertrain semantics, the
[Viper Racing source port at commit `b72d263`](https://github.com/HerbFargus/viper-racing-port/tree/b72d263)
provides an independent Engine/Clutch/Transmission/Differential/Shaft model.
Use it to help name and navigate behavior only: its standalone executable
targets v1.0, while this reconstruction targets the locally supplied v1.1
`race.bin`. Confirm layouts and calls against the local binary before adopting
any behavior; the port is not a drop-in runtime replacement.

The same reference also closes an important visual-rendering gap: its pinned
`GRAF` findings describe `track.grf` as the rendered scene graph, with typed
nodes, per-chunk corner/material/face ranges, and multiple materials within a
chunk. `track.bpp` is collision/terrain data and cannot stand in for that
visual scene. The reconstruction now has a bounded GRF chunk parser, traverses
the linked node graph, and maps every active type-3/type-4 node to a parsed
chunk by the source center offset and its declared array counts across all eight
stock tracks. It submits recognized material ranges through the perspective
renderer; BPP remains the collision source and fallback view. Native visibility
and LOD selection are not implemented, so drawing all mapped chunks may show
geometry the original would cull or select differently. The format reference
remains secondary evidence for unknown node behavior. The original race-camera
comparison, native lighting, alpha sorting, and full presentation acceptance
remain open.

For format tooling, use the reference's strongest validation pattern: compare
parsed fields to independent per-object exports where possible, then require
an unchanged parse/serialize path to reproduce the source bytes exactly.
These checks establish parser accounting and preservation; they do not prove
that the original renderer interprets every field the same way. For runtime
claims, use a calibrated capture instrument and one-variable controls, and
discard tests whose control cannot fail or distinguish the competing
hypotheses.

## Work one behavior at a time

For each behavior that blocks an acceptance gate:

1. Pick one concrete outcome, such as one input changing one wheel response,
   or one camera mode producing a defined view.
2. Follow the original binary's callers, callees, branches, and state accesses.
   Record raw addresses, instruction ranges, offsets, and tool evidence before
   assigning semantic names. Unknown indirect calls and uninitialized values
   stay explicitly unknown.
3. Record the original observation that distinguishes the competing
   explanations. If existing evidence cannot distinguish them, design the
   smallest bounded capture that can. Do not modify original assets or user
   settings for an experiment; use a verified isolated copy where a change is
   required.
4. Implement only the behavior supported by evidence. Link code and regression
   coverage back to its evidence record. Keep approximations marked and fail
   closed when required inputs are unknown.
5. Compare at the same simulation tick or presentation event. Save the input
   schedule, raw capture identity, selected fields, comparison output, and
   tolerances. A passing local regression establishes implementation
   consistency, not original parity.
6. Update the acceptance ledger and progress notes in the same change as the
   implementation. State what the evidence proves and what remains unknown.

Use `function_evidence.template.md` for a native behavior and
`scenario_comparison.template.md` for an original-versus-reconstruction run.
Store private evidence outside Git and commit only safe metadata, digests,
small derived summaries, and references that do not expose original payloads.

For tick-indexed physics CSVs, use `tools/compare_physics_traces.py` with an
explicit integer key and one or more numeric field/tolerance pairs. It aligns
exact tick keys, rejects duplicate keys and missing/non-finite values, records
input hashes and per-field max delta/mean/RMSE, and returns a failing exit code
when samples or tolerances fail. Tolerances must come from observed variability
and be recorded in the scenario report; the utility's numeric pass is not an
original-parity decision.

## First playable race gate

The first integrated scenario is the documented Viper/Bemidji Quick Race:
640x480 reference, 3 laps, Easy AI, Arcade/Sprint, and seven opponents, with
matching car, controls, and race options. Capture one bounded original run and
one reconstruction run using the same timestamped controls. Keep the
reconstruction's ideal-line motion disabled. Compare input delivery, wheel and
body state at matched tick ordinals, contacts/surface observations, camera/HUD
frames, lap crossings, standings, results, and return to setup. Existing
synthetic-input or uncontrolled original captures can guide instrumentation;
they are not numeric golden traces.

The original's host startup and ordinary input constraints are recorded in
`reference_capture_protocol.md`. If they prevent a required observation,
document that exact limitation and design a bounded experiment to resolve it;
do not substitute route motion or invent a physical rule.

## Acceptance vocabulary

- **Implemented:** code exists; parity is not implied.
- **Locally verified:** deterministic tests or reconstruction-only captures
  cover the implementation's stated behavior.
- **Original compared:** a controlled observation is paired with the original
  under a recorded scenario and tolerance.
- **Accepted:** all scenario criteria pass and evidence is linked from the
  acceptance ledger.

Keep the first race gate ahead of expanding to all vehicles and tracks. After
it passes, apply the same evidence loop independently to opponents, other race
modes, frontend/settings, presentation, audio, career/save, replay, and modern
multiplayer. Original network interoperability remains out of scope.
