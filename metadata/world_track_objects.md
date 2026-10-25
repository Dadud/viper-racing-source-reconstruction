# World track object records

Target: `GameBin/race.bin`, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`.
Observed with Ghidra 12.1.4 through REA.

## Track directive dispatch

`0x418F20` retrieves `track.obt` as a BATS resource (`0x53544142`),
`0x418FA0` returns its DWORD line count, and `0x418FB0` computes a line address
as `base + 0x4C + stride * index + offsets[channel]`. The portable
`TrackObjectTextTable` parser preserves the raw header and fixed-stride records;
`track_object_text_line` implements this address calculation and bounds the
string at its record boundary.

All eight extracted `.trk` files have the same layout: count at `+0x00`, raw
word 1 at `+0x04`, sixteen zero suboffsets at `+0x08..+0x47`, stride `0x101`
at `+0x48`, and records beginning at `+0x4C`. Their payload sizes exactly
match `0x4C + count * 0x101`:

| Bundle | Line count | `track.obt` payload bytes |
|---|---:|---:|
| `bemidji.trk` | 15 | 3,931 |
| `dundas.trk` | 185 | 47,621 |
| `hastings.trk` | 30 | 7,786 |
| `heaven.trk` | 87 | 22,435 |
| `kenyon.trk` | 73 | 18,837 |
| `limbo.trk` | 15 | 3,931 |
| `nfield.trk` | 65 | 16,781 |
| `uptown.trk` | 62 | 16,010 |

`0x45C630` starts reading at line index 1, leaving line 0 (for example,
`out\track.txt`) out of the directive loop. `0x45C860` also iterates from
index 1 when searching for the track checkpoint geometry.

The precomputed car-placement helper loads its line geometry through
`0x41E070`/`0x41E030`. `0x41CE80` requests resource type `0x494C494E`, requires
resource version 3, internal marker `0xFFFFFFFE`, and record stride `0x44`,
then copies the counted records before normalization. `parse_ideal_line_resource`
preserves those raw records and any trailing bytes. `0x41CF60` optionally
reverses the record order while keeping the first row fixed, rotates the row
durations at `+0x20`, builds cumulative segment keys at `+0x24`, negates the
two direction fields at `+0x0C/+0x10`, and normalizes the `+0x6E` category byte
when its first value is 1 through 4. It also creates the circular next-row
links, clears the `0xFEEDBEEF` marker from nonterminal rows at `+0x3C`, and
writes each row index at `+0x28`. `normalize_ideal_line_resource` exposes the
link indices separately from raw row bytes rather than storing host pointers.
The category pointer advances by the 0x44-byte row stride while reading at
`+0x6E`, so for rows 0 through count-2 it aliases the following row's `+0x2A`
byte. The portable model expresses that as a bounded flat-array address instead
of indexing beyond a row object, and scans exactly count-1 labels as the native
loop does. For a one-row resource, the first label lies 0x2A bytes into trailing
data; when those bytes are absent, normalization reports
`category_label_input_incomplete` rather than reading outside host memory.
`make_track_spline_index_nodes` projects the normalized `+0x24` key and `+0x20`
duration fields into `select_track_spline_segment` inputs. This reconstructs
the resource envelope and conversion; the actual numeric `.ild`/`.ili` records
remain data inputs.

The ILIN row projection also exposes X/Z position at `+0x04/+0x08` and direction
at `+0x0C/+0x10`. `select_initial_track_segment` models `0x41D290`: it walks
the circular segments, requires the endpoint direction dot products to bracket
the query point, and keeps the candidate with the smaller squared distance to
either endpoint. `project_track_position_to_key` then models `0x41D3A0`'s
10-pass tangent projection: it linearly interpolates position and normalized
direction, adjusts the fraction by residual/duration, advances through later
segments when the fraction exceeds one, clamps a negative fraction to zero,
and clears the segment when the projected key passes its supplied maximum
(10000 in `0x41D1D0`). A backward move within the starting segment also clears
the returned segment pointer. The initial target X/Z, current iterator state,
and x87/Pentium division details remain observations/approximations.

`0x45C630` opens `track.obt`, processes its lines, and calls `0x45D270` for
recognized object directives. `0x45D270` reads the directive kind after `obj`
and dispatches case-insensitively:

| Kind | Target | Observed effect |
|---|---:|---|
| `car` | `0x45CAE0` | Parse placement coordinates and construct the selected car record |
| `obstacle` | `0x45D390` | Parse and construct an obstacle scene object |
| `wobble` | `0x45D5D0` | Parse and construct a wobble object |
| `static` | `0x45D6E0` | Parse a static record unless `0x40D2F0` returns nonzero |
| `checkpoint` | `0x45D950` | Parse a checkpoint and take the global-selected construction route |

The car placement parser `0x45CAE0` scans `obj car %f, %f`; separately,
`0x45CAB0` invokes it with the placeholder `obj car 0, 0` for configured car
records. These two routes share the constructor but have different sources for
the position line. Unknown directive kinds call the target error helper with
`Unknown object type %s`.

`0x45C630` passes its `+0x28` record array to `0x45CAB0`; the loop uses the
count at that array's `+0xC80`, and `0x45CAE0` selects a 0xC8-byte row by the
current global car ordinal. The single-race setup builder `0x40A850` emits the
same row stride and puts its human/AI marker at `+0x00` and car stem at
`+0x11`. `make_single_race_car_track_line_observation` joins those observed
fields to the placement inputs while retaining the caller's current global
ordinal. The marker 0/1 then selects the observed PCAR/ACAR subtype route in
`0x45CAE0`. This is a source-level composition; the surrounding runtime record
handoff and global car-cursor state remain caller inputs. Focused loader/loop
evidence is in `metadata/car_runtime_config.evidence.json`: `0x45B6B0`
(`ev_0a57e0ccd5fb362b0a6778f010246fc2f2e43a6826c91e97a488a6073a574bea`),
`0x45C630` (`ev_1e02fa626e73cc42ec58fdf1faf4249d64af77ef9b94a2578ab2f93659368d65`),
and `0x45CAB0` (`ev_b748b4977d8abda132693c53331855d0237041041feb52d637cba35902b07381`);
the row writer is `0x40A850` (`ev_e54fa22bb11fb18061986c36b87bdd42131208f2bb6dcd1aa84d850bef29cdb0`).
The runtime's display placement planner now uses the same class distinction:
the human row is planned as PCAR and configured AI rows as ACAR. This fixes
class identity in the retained placement plans; it does not yet create complete
AI runtime records or task-stream entries.

The loader's first-token loop recognizes `end` as an early stop, ignores lines
whose first token begins with `;`, accepts `map` and `ai`, and sends `obj` lines
to the dispatcher. The `map` branch scans `map %s` into a temporary token but
does not use that token in the observed function; the `ai` branch also has no
observed state effect there. Other nonempty tokens call the `Bad token: %s`
error helper. `classify_track_directive` models these outcomes and preserves the
original object line for the downstream parser. It does not infer the broader
meaning of map or AI directives.

Before the directive loop, `0x45C630` initializes the 3x3 track basis to
identity and calls `0x45C860`. The helper scans BATS lines starting at index 1
and stops at the first line for which `sscanf("obj checkpoint %s %f,%f %f,%f")`
returns more than three conversions. With the track flag clear, that
checkpoint leaves the basis and translation at identity/zero. With the flag
set, the helper builds a 180-degree Y-axis rotation and translates around the
midpoint of the two checkpoint endpoints; the resulting translation is
`center - R*center`. No matching checkpoint calls the fatal panic path. Because
the conversion gate accepts a result with only three coordinates, the enabled
transform can use an uninitialized fourth coordinate;
`plan_track_transform_from_checkpoint` reports that case instead of assigning
a made-up value. Its trigonometric and x87 intermediate rounding are marked
approximate.

## Car placement path

After the directive loop, `0x45C630` releases the BATS resource and calls
`0x45CAB0` to construct configured cars. That routine walks the configured car
count at `+0xC80`, beginning at the global car index `DAT_0053940C`, and invokes
`0x45CAE0` with the placeholder line `obj car 0, 0`. Object directives also
reach `0x45CAE0` through `0x45D270`; the parser scans `obj car %f, %f` for X and
Z, but the observed code does not branch on the conversion count.

For each available configured car, `0x45CAE0` reads its subtype and model name
from the indexed configuration record, then calls `0x45EC20` to initialize a
0x1E4-byte car record. Subtype IDs 0, 1, 2, and 3 select `PCAR`, `ACAR`,
`NCAR`, and `GCAR` tags respectively; the `ACAR` case also calls `0x41A2A0`,
and the `GCAR` case increments `DAT_0053B1C8` and reads a timer value. Other
subtype values call the fatal-error helper. The model-specific constructor is
`0x466840` for subtype 3 and `0x463920` for the other three, each receiving an
allocation of `0x844` bytes; the result is appended through `0x45C320`.

The GCAR timer value is returned directly from `DAT_004D1214` by `0x40E670`
and copied into its constructor record. `CarTrackLineObservation` supplies
that raw value, and `plan_car_track_line` retains it for subtype 3.

`0x45EC20` loads a CARF resource, requires version 6, copies its 0x228-byte
payload, normalizes the model name to its basename without extension, and calls
`0x45E4B0` to map CARF fields into the 0x1E4-byte caller record. That helper
stores `0x1E0` at `+0x04`, which `0x45CAE0` subsequently replaces with `0x1E4`.
`plan_car_file_model_name` models the loader's exact order: copy to a 260-byte
local buffer, truncate at the last dot in the full path, then select the text
after the last backslash and copy it to runtime-record `+0x198`. It reports
source-path copies over 260 bytes and destination names over the remaining
0x4C bytes in the 0x1E4-byte record. `apply_car_file_model_name` writes only
complete, in-range results, and `apply_car_track_runtime_record` composes this
name with the CARF fields, subtype header, final size, and placement transform.
`0x45CAE0` passes configured-row `+0x34` through `0x45EC20` to `0x45E4B0` as
the 35-word interpolation/configuration table. `extract_car_file_configuration_words`
and `make_car_file_runtime_mapping_observation` read those DWORDs from a bounded
0xC8-byte row, at `+0x34` through `+0xBC`; row fields `+0xC0` and `+0xC4` remain
separate.
`prepare_car_file_runtime_mapping_from_configured_row` now connects the bounded
row extraction directly to the already parsed CARF/UPGS preparation path, so a
caller supplies the resource result, row bytes, selection flags, and FDIV mode
once instead of manually copying 35 words into the intermediate observation.
Rows shorter than `0xC8` return an incomplete preparation without touching
resources.
The field mapping includes many interpolations and subtype or version gates;
its observed stores are modeled below, while field semantics and exact floating
point fidelity remain unresolved.

The five extracted `.car` RST0 bundles each contain a CARF entry with attribute
6 and a 552-byte payload:

| Bundle | CARF resource |
|---|---|
| `exotic.car` | `exotic.cf` |
| `plane.car` | `plane.cf` |
| `sedan.car` | `sedan.cf` |
| `sports.car` | `sports.cf` |
| `viper.car` | `viper.cf` |

`0x45E2F0` compares the resource payload size with 0x228 by calling the empty
helper `0x45E470`, then copies 0x8A DWORDs unconditionally for version 6.
`parse_car_file_payload` requires version 6 and at least 0x228 safe bytes,
copies the first 0x228, and records any surplus-length mismatch. It cannot
emulate a native short-buffer overread, so undersized spans return `nullopt`.
All extracted asset samples contain exactly 0x228 bytes; they do not
independently validate the runtime conversion performed by `0x45E4B0`.
`plan_car_file_read` records the observed `0x45E2F0` sequence and outcome gates:
resource open, chunk lookup (`0x43415246`), version-6 check, the no-op size
helper, fixed copy, optional upgrade branch, and payload release. Resource
manager outcomes and UPGS contents remain caller inputs. `plan_car_file_upgrades`
models the version-1 gate, optional pointer-fixup branch, selection bytes, and
ordered offset/value writes made by `0x45E480`; writes that escape the 0x228-byte
payload are retained and flagged instead of being applied to host memory.
`apply_car_file_upgrade_plan` applies complete, in-bounds write sequences to a
caller-owned byte span, preserving native order and repeated-offset overwrites.
`parse_car_file_upgrade_payload` decodes the relative UPGS form: count at `+0`,
offset table from `+4`, target blocks based at `payload + 0x404 + offset`,
entry count at target `+0x6C`, and `(record_offset, value)` pairs from `+0x70`.
It bounds-checks every read; the already-absolute pointer form cannot be mapped
from a raw file span and remains explicitly incomplete.
`parse_car_file_resources` connects the RST0 reader to CARF/UPGS parsing: it
looks up `<stem>.cf` as CARF and `<stem>.ugs` as UPGS, using each resource's
attribute word as its version. The CLI's `--inspect-car BUNDLE STEM` command
prints the CARF envelope and parsed UPGS set counts for direct bundle inspection.
The extracted `viper.car` (`SHA-256 840ae64a76dab82d81cdcdd91547664ccac25fd265a21220cac4f43a4f8d175a`)
contains `viper.ugs` with resource version 1 and a 5,948-byte payload. Applying
the `+0x404` relative base yields 33 in-bounds sets with entry counts
`53, 1, 1, 1, 4, 1, 4, 1, 1, 2, 2, 6, 1, 0, 8, 4, 4, 5, 4, 1, 1, 2, 8, 8,
8, 4, 1, 4, 1, 1, 6, 1, 4`. The other four extracted car bundles contain no
UPGS resource. This validates the offset interpretation against shipped data;
the meanings of upgrade indices and selection flags remain unknown.

`map_car_file_runtime_fields` now models the stores observed in `0x45E4B0` as
ordered offset/value writes relative to the start of the 0x1E4-byte record,
rather than inventing names for the runtime fields.
It includes the direct header and CARF copies, every configuration-controlled
interpolation, the six-value scale product and 12-value normalization term,
the counted setup block and its trailing zero gates, and the two mode-dependent
final pairs. It leaves untouched output bytes absent from the write list. Float
results use host extended-precision intermediates; the binary's `DAT_004E9754`
Pentium FDIV correction path is flagged as approximated rather than reproduced
bit-for-bit. `apply_car_file_runtime_mapping` applies the ordered little-endian
writes to an existing 0x1E4-byte record, preserving bytes not touched by the
observed routine.
`prepare_car_file_runtime_mapping` connects the RST0 lookup result to the
mapper. When overlays are requested it plans the UPGS writes, copies the CARF
words into a bounded buffer, applies only a complete in-range plan, and maps
the effective payload. Missing UPGS, unsupported versions, short selection
arrays, or out-of-range writes keep the preparation incomplete.
`apply_car_track_runtime_record` composes that mapping with the selected PCAR,
ACAR, NCAR, or GCAR tag, the caller's final `0x1E4` size write, and the
`0x45CAE0` transform at `+0x18`. It writes into caller-owned storage so bytes
outside the observed stores remain explicit inputs.

Unless `DAT_004DC1B0` forces the default-placement branch, `0x45CE10` may
supply a precomputed transform. The default branch transforms the parsed X/Z
through the track basis and translation globals initialized at `0x45C630`,
queries terrain at the resulting position, adds 2 to the returned height, and
uses an identity orientation. A configured-record flag at `+0xC4` can trigger
`0x45D1C0`. The semantics of these flags, the precomputed transform, and the
subtype initialization helpers are not yet reconstructed.

The ACAR-specific `0x41A2A0` helper reads the configured index from record
`+0x194`; `0x41C990` indexes `DAT_00503D40` and returns that entry modulo 7.
It obtains the row selector from `DAT_004F0B30` through `0x41AFF0` and calls
the manager vtable method at `+4` with that selector and the mapped index.
The vtable target is `0x41BB00`. The manager has eight rows of 0x44 bytes;
each row contains up to 16 entry DWORDs and a count in its final DWORD. The
method adds the manager cursor at `+0x228` to the mapped index, divides by the
selected row's count, and uses the remainder as the entry column. The returned
DWORD is loaded from the manager row. The constructor's `0x41B420` path fills
the rows from the global 8x17 DWORD table at `0x504A78`, when the development
driver check does not suppress that path. Thus the virtual lookup algorithm
and row contents' source are reconstructed; the runtime cursor, row selector,
row counts, and pointed-to entry payload remain inputs. `plan_acar_virtual_lookup`
models the row/column selection while retaining the returned DWORD raw.

The helper then calls
`WorldGameOptions()` (`0x45C2C0`) and `WorldGetTrackNumber()` (`0x45C280`);
the wrappers pass their respective "cannot call before WorldBegin" message
and `DAT_00539414` to `0x45C270`, whose analyzed body is a bare `RET`; they
then return their global state. If the virtual
selected entry's word at `+0x2C`, compared as a signed DWORD, is greater than
`0x42C80000`, it overwrites record `+0x08`. If the second is greater than
`0x3F800000`, it multiplies record floats at `+0x150` and `+0x154` by the
selected entry's word at `+0x30`. The portable `plan_acar_track_adjustment`
models the threshold gates and writes while leaving those pointed-to words as
explicit observations.

When the configured-record byte at `+0xC4` is nonzero, `0x45D1C0` constructs a
`BALL`/0x38 record using `ball.mod`, allocates 0x40 bytes, and calls
`0x45C320` after the common `0x463590` constructor. Its fields are `3000.0` at
`+0x08`; `5000.0` at `+0x0C`, `+0x10`, and `+0x14`; zero at `+0x18`; terrain
height queried at `(0,0)` plus `5000.0` at `+0x1C`; zero at `+0x20`; `18.0` at
`+0x24`; `5000.0` at `+0x28`; `0.6` at `+0x2C`; `5.0` at `+0x30`; and the
car index at `+0x34`. `ConfiguredBallEffectRecord` asserts this layout, and
`plan_configured_ball_effect` takes the terrain result as an observation.

`plan_car_track_line` models the configured-car index gate (including the
increment before empty-model and subtype validation), record tags, 0x1E4 size,
allocation and constructor choice, and the default or supplied precomputed
transform. The default pose uses the track basis for transformed X/Z, terrain
height plus 2 for Y, and identity orientation. `plan_car_spline_pose` also
reconstructs the successful-result math in `0x45CE10`: cubic Hermite
interpolation of the selected segment's four X/Z samples, linear blending and
normalization of its direction, a four-unit lateral offset chosen by placement
ordinal parity, terrain height plus 0.61, and the resulting orientation basis.
`0x41E070` initializes the geometry source from `track.ild` and falls back to
`dlatlong.ili`. `select_track_spline_segment` models `0x41E380`'s circular-list
walk: beginning at the sentinel, it advances while the next node's key at
`+0x24` is less than the target, then returns the current node and
`(target - current_key) / current_duration` using the `+0x20` duration. The
fraction is not clamped. `0x45CE10` derives the target as its geometry object's
track key minus `(placement_ordinal * 15 + 28)`. The loaded linked-list values,
cubic samples, endpoint directions, and terrain result remain observations; a
failed geometry lookup follows the default track-basis route.

In `0x45CAE0`, the `0x1E4`-byte runtime record starts at `local_224`, while the
placement helper's `local_20c` output aliases record offset `+0x18`. The
12-float matrix/position result therefore occupies `+0x18..+0x47` in that
record before the selected subtype constructor receives it. The default route
writes the same transform slots. `apply_car_track_placement` writes those 12
little-endian float words into an existing full-size record and preserves all
other bytes, completing the placement-to-record handoff.

`compute_track_geometry_key` models the value calculation observed in
`0x41E6C0`: when the enable byte at `+0x30` is clear it returns zero;
otherwise it computes `(z - reference_z) * z_weight + (x - reference_x) *
x_weight` from fields `+0x18`, `+0x1C`, `+0x3C`, `+0x40`, `+0x4C`, and `+0x50`.
`0x41E130` stores that helper's result at geometry-object offset `+0x54`.
That value is distinct from the placement base key: `0x41E070` writes the
last ILIN row's endpoint sum to `+0x5C`, and the `0x45CE10` stack layout maps
its `local_14` directly to geometry-object `+0x5C`. Thus
`plan_car_spline_target_key` takes this placement base and models
`float(placement_ordinal) * 15 + 28` subtraction before `0x41E380`. Host float
rounding still does not claim bit-exact x87 behavior.
The caller `0x45CAE0` supplies that ordinal as the global car index minus the
global GCAR counter; for a GCAR, it increments the counter first. The
`compute_car_placement_ordinal` helper reproduces the resulting 32-bit
subtraction while leaving the global counter value as an observation.
`plan_car_spline_track_placement` now composes that target calculation with the
circular segment walk and pose construction. For the selected row and its
next row it supplies positions as the Hermite endpoints, directions as the
Hermite tangents and blended direction endpoints, and the observed terrain
height to the pose model. This removes the need for callers to manually
translate the normalized ILIN nodes into `CarSplinePoseObservation`; terrain
sampling and the actual loaded rows remain inputs.
`CarTrackLineObservation::spline_placement` feeds this composition into
`plan_car_track_line`; that caller computes the ordinal from the configured
car index, pre-call GCAR count, and current subtype, then stores both the
placement plan and resulting transform. If the composed segment lookup has no
pose, the existing supplied-pose/transform fallback routes remain available.
The model also reports the ACAR helper, GCAR counter/timer, and configured
BALL effect as explicit side-effect flags. CARF payload parsing and the
observed `0x45E4B0` field stores are modeled; subtype side-effect
implementations remain outside the plan.

### Runtime switch evidence and current use

In shipped `race.bin` (x86, SHA-256
`f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`), global
`0x4DC1B0` is zero-initialized. Its command-line parser at `0x401280` sets it
when a token beginning with `-` or `/` has the four-byte prefix `grid`; the
car-line constructor at `0x45CAE0` reads it to force the track-basis/default
placement. Without that switch, the constructor attempts the spline helper and
uses default placement only when the helper fails. This is static shipped-code
evidence; it does not prove which branch a specific original-game session
takes at runtime.

The reconstruction's Win32 runtime now builds each visible `obj car` pose
through `plan_car_track_line`, using the player slot as PCAR and the remaining
visible slots as NCAR. It builds the spline nodes from normalized `track.ild`,
uses the final node key plus duration as the geometry endpoint key, evaluates
the default head at node zero, and queries terrain at the preliminary planned
X/Z before recomputing the final transform. `-grid` and `/grid` select the
default-placement route; the initial scene pose retains the resulting planned
heading and height instead of replacing them with the older nearest-ideal-line
heading. This live adapter currently assumes an identity track basis, zero
GCAR counter, and `obj car` row order matching the runtime entrant order. It
does not yet derive reversed-track basis, validate row-to-participant subtype
mapping against an original capture, apply transforms to fully constructed
CARF records, or compare measured transforms with a controlled original run.

## Checkpoint line and record

`0x45D950` scans the form `obj checkpoint %s %f,%f %f,%f`; a conversion count
other than five calls the `Bad checkpoint record` error helper. It then queries
terrain height through `0x45F600` at each `(x,z)` endpoint. That helper calls
`0x4672B0` with the queried X/Z and Y initialized to 1000; it returns the
observed output Y when the query succeeds and zero when it fails.

The constructed record is 0x20 bytes: `CHKP` (`0x43484B50`), size `0x20`, then
two endpoint triples in X, queried-height, Z order. The parsed `%s` token is
required for the conversion count but is not used in the constructed record or
the hardcoded visual asset name `checkpt1.mod`.

The nonzero state of `DAT_0053B194` selects the visual route: allocate a 0x40
object, initialize it through `0x463590` with the CHKP record and
`checkpt1.mod`, then append it through `0x45C320`. When the global is zero,
`0x45D950` clears the preceding DWORD and sends the CHKP header and payload to
`0x425720` for record serialization. The same DWORD is the selector read by
the `0x426000` physics-track service factory, so
`CheckpointTrackLineObservation` carries the raw
`track_state_selector_0x53b194` instead of a route-only boolean. Zero/nonzero
controls this parser's branch, while values 0/1/3/5 select service classes in
the factory. The shipped image initializes the field to zero, but runtime
writes remain an observation. `plan_checkpoint_track_line` builds the verified
record and branch plan. `produce_checkpoint_task_record_from_line` now adapts a
raw object line, obtains both heights through `terrain_height_or_zero`, and
serializes the complete 0x20-byte CHKP record on the selector-zero route. It
reports the visual route without emitting physics bytes when the selector is
nonzero. This is an individual record producer; it is not inserted into the
runtime task stream because ordered world-object and entrant record production
is still unresolved. See `physics_task_objects.md` and
`physics_track_selector_source.evidence.json` for the accessor and initial
value trace.

## Wobble line and record

`0x45D5D0` scans `obj wobble %s %d` and requires two conversions. It accepts
`pole` and `flap` case-insensitively, mapping them to raw DWORD values 0 and 1;
other type names take the target fatal-error path. The constructor writes a
`WOBL` (`0x574F424C`) record with size `0x20`. Its remaining DWORDs, in order,
are `10000.0`, `12500.0`, `12500.0`, `12500.0`, the parsed signed integer,
and the type value. The record is passed to `0x463600`, then the resulting
object is appended through `0x45C320`. `WobbleTrackRecord` preserves those raw
offsets and deliberately leaves their semantics unnamed.

`WobbleTrackLineObservation` exposes the scan count and parsed values;
`make_wobble_track_record` rejects the observed malformed-line and unknown-type
routes, and otherwise returns the exact raw record fields.

The `CheckpointTrackRecord` and `WobbleTrackRecord` C++ layouts include the
header DWORDs as members. Compile-time size and offset assertions pin them to
the observed 0x20-byte records.

## Static box line and record

The `static` directive reaches `0x45D6E0` only when `0x40D2F0` returns zero.
That parser scans `obj static %s` followed by nine floats and requires ten total
conversions. The only accepted type observed in the body is `box` at
`0x4DC670`; unknown types call the fatal-error helper. The first three floats
are copied to payload offset `+0x2C`, the next three form the rotation vector,
and the final three are copied to `+0x3C`. Payload DWORD `+0x38` is cleared.

The resulting `STAT` (`0x53544154`) record has total size `0x48`. Its payload
begins with a row-major 3x3 matrix at `+0x08`, followed by those raw fields.
For rotation-vector magnitude at or below `0.0001`, the matrix is identity;
otherwise the routine normalizes the vector and writes the Rodrigues-derived
matrix entries. `0x425720` serializes the contiguous header and payload into
the shared record buffer. `StaticTrackRecordPayload` and
`plan_static_track_line` reproduce the observed layout and branch outcomes.
The target's `DAT_004E9754` division-correction path and x87 precision are
represented with ordinary extended-precision host arithmetic, so the matrix
can differ in low bits on affected hardware.

`StaticTrackRecord` stores the STAT header before its payload; a compile-time
size assertion fixes the complete object layout at 0x48 bytes.

## Obstacle line and record

`0x45D390` scans `obj obstacle %s %s %f,%f:%f %f` and requires six
conversions. The first token maps case-insensitively to `ball` → 0, `cube` → 1,
or `prism` → 2; other values take the fatal-error path. The second token names
the model resource. The four numeric values are retained as X, Z, an opaque
scalar, and scale. The parser obtains terrain height at X/Z through
`0x45F600`, obtains the model through `0x44F390`, queries model bounds through
`0x509B0`, and releases the model through `0x44F420` before creating and
appending the scene object.

`0x509B0` follows the model's vertex-block pointer. When present, it initializes
axis minima to 10000 and maxima to -10000, then reads X/Y/Z at offsets 0, 1,
and 2 of each eight-float vertex record. A missing vertex block yields six
zeroes and calls the fatal helper; a present block with a nonpositive count
retains the sentinels. `compute_obstacle_model_bounds` exposes the stride,
sentinel, and missing-block branches.

Before construction, `0x45D390` computes Y as
`terrain_height - model_min_y + 4`, extents as each maximum minus minimum, and
three raw scale terms as squared extent times `10.749798` times the line's
scale. The constant is the float at `0x4C9040`. Its contiguous 0x38-byte record
is:

| Offset | Value |
|---:|---|
| `+0x00` | `OBST` (`0x4F425354`) |
| `+0x04` | `0x38` total record size |
| `+0x08` | parsed scale |
| `+0x0C` | Y extent squared times factor times scale |
| `+0x10` | Z extent squared times factor times scale |
| `+0x14` | X extent squared times factor times scale |
| `+0x18` | parsed X |
| `+0x1C` | terrain-adjusted Y |
| `+0x20` | parsed Z |
| `+0x24` | third parsed scalar, semantics unknown |
| `+0x28` | model X extent |
| `+0x2C` | model Y extent |
| `+0x30` | model Z extent |
| `+0x34` | obstacle type ID (ball 0, cube 1, prism 2) |

The shared scene constructor `0x463590` initializes the scene-object base,
loads the named model and stores its handle at object `+0x3C`, then calls
`0x463510`/`0x425720` to serialize this record and store its record-buffer
offset at object `+0x34`. Finally `0x45D390` appends the object through
`0x45C320`. `ObstacleTrackRecord` and `plan_obstacle_track_line` now reproduce
the complete record and preserve the model asset name needed by this path.
The C++ struct includes the OBST header as its first two DWORDs and asserts the
complete 0x38-byte size and recovered first and last payload offsets.

Evidence IDs:

- `0x418F20`: `ev_089cb89e90a15d2bb3dd4826f786c930c2f1c41b21bd3624d0ecbade54d9af6b`
- `0x418FA0`: `ev_62e66cb03086464f87c995dc56fb677181dfcc5189645b3e81a4f89cd088acdc`
- `0x418FB0`: `ev_1667ae3b0a614734d7a7d0068f26583ae115eca7a7c6f899c1730623dcb011b6`
- `0x45C630`: `ev_f0b883adbd848f2664e51cb6ba5badacca072ffb56068ff45b06a058e5a0b18b`
- `0x45C630` loader detail: `ev_8fa18b5dad4cc293f2dee3ca6070623cba4b3705c407cf7b573be532dc884469`
- `0x45C630` full track-obtain/dispatch path: `ev_d7b3a68d43de04556c5398cfebdf771f7b964291cc5a9351c65622f8d75aecfe`
- `0x45C860` checkpoint-derived track transform: `ev_a72ffe2df3cdac37015e05cce180e89be36f447b1f0e0f7a95a96b0f598ab6a2`
- `0x45DB00` Euler rotation matrix: `ev_51b739ff3896bc62884d051326688b01cc4ba0ecbb36465f765e63f7d94b889b`
- `0x402DD0` row-major 3x3 matrix multiplication: `ev_f8317f8b9afd1dd0013a262afabfec456d113da324713721400d0d83a98b96d9`
- `0x411000` fatal panic wrapper: `ev_0c79299b0c4964f72cbeb1d10559b2c2b18e5693baba61eb1eeaa3a7fa70af39`
- `0x45CAB0` configured-car loop: `ev_41caa0fe773152070803d33e597d829b4c893e9f4e8a27604671002cb9948d6e`
- `0x45CAE0` car line constructor: `ev_bfeb1e7746f91fefee94fb2fcadd91d99a58fd3c6dd684406b8d8ce64d269bf4`
- `0x45CE10` precomputed placement helper: `ev_edbff837f576c0ebd208cd2ddc23c21fbcef5f5a13c8c9ef0b4a1c742d58d8b8`
- `0x45CE10` selected-segment pose math: `ev_214d85373822c1b1828ec8c1f270940de7d13a83237ed5d2d2a373ccc15ad75c`
- `0x45CE10` stack field mapping for placement base `+0x5C`: `ev_0b87dae46cb5d4573e2679ed5a17ae640d343a68e3473984fde4754bd6abb20d`
- `0x41E070` track geometry iterator setup: `ev_87d6548abc1113324c0c0a02f67d18597d3a6ab95d9869ce01437cc8e0c25e18`
- `0x41E130` projected geometry key storage: `ev_4ee1247f707cc67dbcc13c1f2b51098c38f64d3ba4a04c7c4061fbd41eb8ca70`
- `0x41E6C0` geometry key calculation: `ev_29e4c6cdc81ed53c65712dbaf5574c1ff3c7087805c193a205e1c6cfb952aa12`
- `0x41E380` linked-list segment selection and interpolation fraction: `ev_36fe2310a3f54f2cff42034194ce44ab45042ae5e14cb5fa0883e4826debe799`
- `0x41FD80` direction-vector interpolation/normalization: `ev_c3da0ecfe464fd1853e0c0d6211bb2661ad5d9f133b810c284498bf469c33723`
- `0x45D110` cubic X/Z interpolation: `ev_68470575c4d6a933be5c4a2d26b82f641c085305cb7cd8bca835799547c5ba27`
- `0x45CAE0` precomputed-pose success/fallback gate: `ev_ba45d8741718eabfde745f1cde82e80b5989c36c45ace7626b357e95f6c26655`
- `0x45D1C0` configured BALL effect: `ev_52e688c9b935e3a6578d47014d2218c17bc7252f3950e4e75ae5773f88d0234e`
- `0x45D1C0` configured BALL effect detail: `ev_4e8971ea524e65ac74094e19514fe3e39361d90b36d8ceaf453e933f4884b062`
- `0x41A2A0` ACAR helper writes: `ev_78facd1d5c2dd00084495cc37e2fe38ed7b4777afa11799f66cd9add515fea83`
- `0x41C990` ACAR index remap: `ev_deeda08b33e34e674e30fb6c377be9aac714bf3cba6df4474713ce7939fcb7a0`
- `0x41AFF0` global ACAR provider getter: `ev_68c2311d6a42e29f4a6bd674734a00678122196a1ce64deee780069f08fd91ce`
- `0x41BB00` virtual lookup instructions: `ev_d2f83650b1156b292edfd0f73f694d650e700fa5706d7f3411d56961fc34e02d`, `ev_bf852b3a8e1b0edaf4987b0014000e705f583e8f15c4d697839f968ae7f25c17`, `ev_2c381a743cec41cabd4cb80887c181bb39abb3ba1928b9d1f96d73b404a83e03`, `ev_1a3178534ce46aff65b3838b39abfa4d64ea8f42ddcd03e49ab17d4032c2d680`, `ev_9f19e2338dd9ef477f991d4a3866f5c1522493eacbadfa97af9ec830c8f534da`
- `0x41BA70` manager constructor and row setup call: `ev_940268f25dbfe6d58194ad184c779c6836a1968c7e620b8e8778e27d52f88692`
- `0x41B420` fills manager rows and counts: `ev_0a948c04b274c4ce30f6ec6ae7a0c69cf37483d23791f5d20bb098651ee03cc2`
- `0x41BB30` global 8x17 table lookup: `ev_d31ffef84449c28bf42f172b070abebfacd43f21c4138a795ccec8c4c25b03fd`
- `0x41E070` track geometry initialization: `ev_87d6548abc1113324c0c0a02f67d18597d3a6ab95d9869ce01437cc8e0c25e18`
- `0x41E030` geometry resource replacement: `ev_2945c95dcfa71db0f115f836faf0d0db9dadc631fc2a2d96307233eda2d91d79`
- `0x41CE80` ILIN resource validation/copy: `ev_5e76ea450fa47323a22d0cf8a70a1b183298b2bcbcb491adcbce6f192ab34e0a`
- `0x41CF60` ILIN orientation and circular-link normalization: `ev_453b2d64613bbcd4e5114823d84fdefe0ebf8956ab9db1e76f2f3fbe80aa4c6a`
- `0x41D120` geometry resource loader handoff: `ev_1170afbeca3ee9b34148e5822d286c01ec63840f62985ec395bff7af4c048dd2`
- `0x41D290` initial oriented-segment selection: `ev_e1eeabc811079974a378291a0bbf67d2665a31614b66918b5f34887a6a4c2e6b`
- `0x41D3A0` iterative point-to-track projection: `ev_3e17e6add1bd8f0fd41edec54abd53dd9565088c9387eb2491f78ef8e4a56878`
- `0x41FD50` segment position interpolation: `ev_49637204ddc9459d9b26931e487749418bf6a231b4e684da72abf006c03a58fa`
- `0x41FD80` normalized direction interpolation: `ev_a7614fe0d7afadfa63ad41c27b88efdf0833e85bfdd384e6a8f6befd18ee84a5`
- `0x41D1D0` initial 10000-key projection setup: `ev_8441ab428fd16d01524af2154977c49882dae3079e002e5105948c5f989b0269`
- `0x41BC30` traverses the eight configured rows: `ev_5caccd34c15f6e064656e6c84acbfb6146be5a7a56e1a27a06921cc9fa4e1eae`
- `0x45C2C0` `WorldGameOptions`: `ev_bd04df62f6b95fd6c9ca94441bc443ba349282d6610f673b6faa4e216e49b68a`
- `0x45C280` `WorldGetTrackNumber`: `ev_4bfa7c6158c887dd0a4333b5a66b2cbbe870f36a87ef551cde4eb434d91328ad`
- `0x45C270` world API check helper: `ev_474d802a38c3162465a6966ca781e66576a50a6c61665771718d99c448bafaa5`
- `0x40E670` GCAR timer getter: `ev_280c0c61e0121222526ec61692c83e1c42cd487a89631b0fb1d6fb70cce40464`
- `0x45EC20` CARF resource path and basename copy: `ev_17d1cae37797b0625f8cd9168909a49ea943b14a649befbff5dea0a0fb2466d5`; focused evidence is in `metadata/car_file_loader.evidence.json`
- Focused confirmation that `0x45CAE0` calls the CARF loader and field mapper, then supplies the resulting record to its scene-construction path: `ev_0c21c172a16c58e01c47ed385c88c6018c7190ca4bd9f9ecdbedda281414dedd`
- `0x45EC20` exact `0x228` CARF copy, model basename write, and call to `0x45E4B0`: `ev_fb3327a7e08f9a6d971381fe97d5a07675ead14fa67db814cca419b6d6950aa8`
- `0x45E4B0` observed setup/CARF-to-record field stores, including `0x1E0` size followed by `0x45CAE0`'s final `0x1E4` size write: `ev_68656e61c5d41c844cb217bb7a21fe87a07e39090d7602d933940825e718e6aa`
- `0x45E2F0` CARF version/upgrade loader: `ev_05c66de5eacb28dd86480abcad9ff524848abae7fecb91b9056a3bfdc06fb006`
- `0x45E470` size-check helper body: `ev_af962d5e30792f74916a80bdc00afc19aeb957310c53c7b0dc7b9343c1b72358`
- `0x45ED10` UPGS loader: `ev_bd53fdb90138dd00a7d2a870d4f301fdc59196586f8b37dd18da1cbe48aaed23`
- `0x45E480` UPGS application loop: `ev_882e49398a89796f5d7820c15549c050bf0c0f92e1ca46804d58dd780406efac`
- resource suffix bytes `.cf` at `0x4D3E9C`: `ev_74cd6e0b73fa1d7848377757e5c90ec44b782396c81467577a4c0bb8c5b7b0f8`
- resource suffix bytes `.car` at `0x4D3E94`: `ev_0417b4b9e64b10598a9824843f2d15403c7314f45c472069e32ac02d78e13fd1`
- resource suffix bytes `.ugs` at `0x4DC874`: `ev_d62c3e17002030b83c5b2aab4264f0342c22d4fb6b146911015c1bc176426b67`
- `0x45E4B0` CARF-to-record field mapping: `ev_520f4b2d9aec343dddb2f2127431bfe0c6d4c25a99ab20ffa06eec1a05a08317`
- `0x45D270`: `ev_7e2fd437460b56cf40e509f370a9ae91cc2c47ac5d476f84011adb084b95bba5`
- `0x45D270` loader dispatch detail: `ev_2b8962cca5300344b4b9a8829cc28c76fcbeadf560672b269d731a598791a34c`
- loader token bytes at `0x4DC440`: `ev_06b8f406878932b3363ffb60b535560199cda46472838d461950e1a92617b2de`
- `0x45D950`: `ev_e2e01cae065351933fb41a05647f45a3e2218661a494f4e4095f4960d1a26924`
- `0x45F600`: `ev_90a2d09df6eda39d947f0ebe2f9b29773f053bb18dce9718cc3aae63c5cf684c`
- `0x45D5D0`: `ev_e78207a5b8b032fbb89899559578a8f0f4d215211637d1b72603c7dd9f96533d`
- wobble type bytes at `0x4DC5F4`: `ev_deb62d620454fa325cc866a464db1352e26019df6dbc7a1ba0e21927879f7c83`
- `0x45D6E0` pseudocode: `ev_eb5935e6a551cf23fa91352ace999cfe2c0a158aafcbb828b7cabe32ff24959f`
- `0x45D6E0` instructions: `ev_4acbfe6d2397810fe55e429197e8f54269e4440fad4631decd123f188e3a5652`
- type bytes at `0x4DC670`: `ev_84a4d3533ad2a1fc2e0b4335f2b2697ec305facd434eaf3e71ae38b9473e5df5`
- `0x425720` serializer: `ev_2497a64360cb381b32c5cd6f8d0ae361ab2027ced1020eeef98983481661856c`
- `0x45D390` pseudocode: `ev_f3ad083b0b1076cc4b0526c7022c91012f521cec2041d64448a2b723a45a1952`
- `0x45D390` instructions: `ev_4535af6557da7d146c47621c2ab33ce2821bb0b76fc2fcaf30528867c63e3d65`
- obstacle type bytes at `0x4DC58C`: `ev_648393227f5c1f54607a19bb86a49bd295fd52be8571eeb061b2141da8d0ce5a`
- `0x509B0` model bounds: `ev_23fcb3d129c1a2820c2973d55a72acce77f823557536b75498c836ab8c1b5a62`
- `0x44F390` model acquisition: `ev_5ed97be0191c1dcfef475f1971142485aa2e8d90540b4439ada831454ab7d432`
- scale constant at `0x4C9040`: `ev_0becb5b67642dfd9616fb3f2ee934cd58f6910a92493990ac9b6f3fd6fd88912`
- `0x463590` common scene constructor: `ev_80aaabb90539e4f1f5e0bdddfb05203f89b72e9f89e138cef47538fd536d2efc`
- `0x463510` record/offset handoff: `ev_cbf8e50a4cb756cfafc3f82a9f25d8c2705face9a1de320f7d86ac16cf5c2d35`

## Remaining track object work

The BATS payload parser and top-level line classifier are modeled, as are
checkpoint, wobble, static-box, and obstacle records. Car subtype and
placement planning plus the CARF envelope parser and observed CARF-to-runtime
field/placement composition are modeled. Call-site selection bytes, bytes not
written by the observed mapper, subtype-constructor effects, and external
resource-manager behavior remain inputs. Map and AI behavior outside the
observed dispatch loop, some checkpoint consumer behavior, and full track
traversal also remain incomplete.

The shared terrain surface query is now modeled in `terrain_surface`, including coordinate quantization, partition-tree selection, plane validation, height solving, and feature extraction. The loader path is now reconstructed too: `0x4630B0` requests `track.bpp`; `0x467060` loads type `TPPB` at version 2; its 20-byte header gives a plane count at `+0x00` and root node index at `+0x10`; plane records are 0x38 bytes and partition nodes are 0x1C bytes. `0x467130` fixes child references from serialized indices to pointers, with `-1` as the null sentinel. `parse_terrain_surface_payload` decodes this resource into portable plane and node arrays, `parse_track_terrain_resource` reads it from a `ResourceBundle`, and `make_terrain_surface_query_input` joins it to the existing sampler. Static evidence: `0x4630B0` (`ev_03f026f83423665e58978722697a9778931cb0b559d41521d2497b36a1cc9997`), `0x467060` (`ev_3350db3f16418ef6ac36fe9ad5a586424f4fcb7bcfd3004a8fecfb8006e71cdd`), `0x467130` (`ev_9a312afc972270d8e79bbda9f5c0b4f8b70e3ccddcd0328f6f0d55d90e8817c5`), and query routine `0x4672B0` (`ev_02a12b56b8da3a7c9bbd70acea0ae1696edd55d1a9b5d5edc3fa23baaf6dc868`). The extracted `uptown.trk` sample has 12,092 planes and 43,078 nodes. The loader's panic behavior and exact x87/FDIV arithmetic remain distinct from the bounded parser's safe rejection behavior. See `vehicle_setup.md` for query equations and floating-point limitations.

Caller tracing closes more of the integration gap. `0x45F600` builds the
height-only query `(x, 1000, z)`, calls `0x4672B0`, and returns the sampled Y or
zero on failure. Its caller references include car placement, default spline
placement, the configured BALL effect, obstacle and checkpoint construction,
plus `0x4282E0`; that routine uses the height result while transforming camera
coordinates and is called by `0x429C00`. Mode-0 camera update `0x421120` and
wheel update `0x442530` call `0x45F760` for a full point/normal/feature sample.
`sample_terrain_resource` runs that query directly against a parsed payload,
and `terrain_height_or_zero` matches the `0x45F600` fallback. Query positions
computed by each larger caller remain supplied values. Evidence: `0x45F600`
(`ev_1470d7bd5ca25a8ade0f20f36528a0c868988010753bc5bf61bb110797b8f7ba`),
`0x4282E0` (`ev_aef95d127c69508de483fcb6eb6eb894b4e9aaae2712a29c16f9d850f23f0e90`),
and camera mode `0x421120`
(`ev_3314c631f34b460398f9e0176a2161e3670cc8c9ee6fcbed396c68f19426b8dc`).
