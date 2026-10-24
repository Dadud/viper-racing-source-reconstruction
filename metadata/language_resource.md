# LANGRES0 language resource

The extracted `GameData/english.lng` file has SHA-256
`7d2e1fabd12588ed4d83eb5a1c21af49b4c670fde14c47152b18197808f83b42`.
Its on-disk header begins with the eight bytes `0SERGNAL`; the language name
starts at file offset `0x14`. The entry count is at `0x54`, followed at `0x58`
by that many eight-byte `(key offset, value offset)` pairs. Both offsets are
relative to the in-memory language object at file offset `0x14`, matching the
pointer rebasing loop in `0x4187A0`.

`parse_language_resource` checks the magic, count/table bounds, relative-offset
arithmetic, string bounds, and NUL termination. It leaves the bytes unchanged
and preserves table order. Inspecting the extracted English file yielded 822
valid entries with 822 unique keys, including:

| Key | English value |
| --- | --- |
| `Dashboard:TestTrack:Try` | `TRY` |
| `Dashboard:TestTrack:Time` | `TIME` |
| `Dashboard:TestTrack:DistanceAbbreviated` | `DIST` |

The CLI exposes this parser through `--inspect-language PATH [KEY]`. The
original `.lng` asset is not included in this source repository.

Evidence: `0x418620` enumerates `*.lng` and registers each parsed language
name (`ev_0db207d58ca09bf2f79ef85d0f031118b0fba43b35a8e35ee1daaa6543ad8e99`);
`0x418720` obtains a `LANG` resource and calls the rebasing helper when needed
(`ev_d4ecb5a29f46456c8f89732e78c805dd569e7842142c897cbfa034ecd3f93078`);
`0x417CB0` shows the resource payload pointer adjustment and table-member reads
(`ev_5e5a3a07556c51f28c9a1904559c37d5d9651c16a4c2dd9293af1ed04f1cad7b`);
and `0x4187A0` adds the object base to each of the two entry offsets
(`ev_2c73f32acaddb00a6c66b9133ddff8754340f279c5e175facae571ed0f4b3d6a`).
