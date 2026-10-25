# Track object task-record materialization

`materialize_track_object_task_records` walks the already parsed BATS
`track.obt` table in source order, beginning at row 1 as `0x45C630` does. It
returns one owned result per visited line and exact little-endian record bytes
for supported physics-task object classes. These bytes can be passed, in their
produced order, to `serialize_physics_task_records` or merged with records from
the configured-car producer. The function deliberately does not decide that
merged order, assign stream cursors, or append configured entrants.

`plan_ordered_track_task_records` now joins those results with the separately
recovered `TrackPhysicsTaskManifest`. Every output row carries its manifest
index, cursor, tag, and either the source-row bytes or the configured-car
identity. It accepts separately produced car bytes keyed by configured-car
index, which can be produced by `apply_car_track_runtime_record` from CARF,
setup, model-name, and placement inputs. It checks their tag/size headers and
the matching deferred source row before joining them. It reports the first
manifest entry without a complete matching record and is complete only when
the manifest and all record rows are covered. This is an ordering/validation
layer, not a serializer or native task-array loader. The runtime now produces
the player's PCAR and selected AI ACAR record bytes from their setup, CARF,
model name, and placement inputs, then checks their configured indices and tags
against the manifest. It still does not pass those records through this ordered
join or load a full task stream; NCAR/GCAR entries and all materialized
non-car records also remain unresolved at runtime.

The materializer uses the existing line/record models for:

| Class | Producer path | Bytes |
| --- | --- | --- |
| `CHKP` | `produce_checkpoint_task_record_from_line`, including two terrain height-or-zero queries | complete `0x20` record when selector `0x53B194` is zero |
| `WOBL` | native `obj wobble` scan plus `make_wobble_track_record` | complete `0x20` record |
| `STAT` | native `obj static` scan plus `plan_static_track_line` | complete `0x48` record when the caller supplies an enabled `0x40D2F0` gate |
| `OBST` | native `obj obstacle` scan, FNIM model lookup/bounds, terrain height, and `plan_obstacle_track_line` | complete `0x38` record |

The model-bundle span is searched in caller-provided order, using each bundle's
case-insensitive FNIM lookup and the parsed model's eight-float vertex records.
For normal sessions, callers should provide the active track and shared model
bundles in the same effective lookup precedence as the resource manager.
Terrain queries use the supplied track TPPB payload.

`map`, `ai`, comments, and empty lines produce explicit ignored rows. `end`
stops the scan. A car row is reported as `car_record_deferred` for the separate
entrant/car producer; no car bytes are guessed. Malformed rows, unsupported
object types, unavailable/invalid model resources, and incomplete BATS rows
make the result incomplete. The native static-object gate and visual-only
checkpoint route are represented as explicit no-task-record outcomes.

The producer evidence is in `world_track_objects.md`: `0x45D950` emits CHKP
bytes through `0x425720`; `0x45D5D0` constructs WOBL; `0x45D6E0` constructs
STAT; and `0x45D390` derives OBST dimensions from FNIM geometry and serializes
through `0x463510`/`0x425720`. `0x45CAE0` builds the car record using
`0x45EC20`/`0x45E4B0`; the record is passed through `0x463920`/`0x463510` to
the same stream serializer. `0x425720` framing and the later `0x4224D0` reader
are documented in `physics_task_objects.md`.

This is record production, not a complete native-world loader. It does not
construct scene objects, include configured entrant rows, model BALL records
(which are not an `obj` directive in the observed dispatch), run the physics
factory, or establish player/task-array correspondence. STAT matrix arithmetic
and checkpoint/terrain calculations retain the existing host floating-point
approximations; structural completeness does not guarantee x87/FDIV bitwise
identity. The corresponding regression uses synthetic resources to validate
byte layout, source order, route gates, explicit unresolved car entries, and
the join between complete non-car bytes and the independent manifest.
