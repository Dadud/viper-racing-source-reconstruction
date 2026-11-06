# Extracted Viper TEX resource inventory

This inventory records the external installation evidence used by the runtime
texture resolver. The files remain outside this source repository. Bundle
entries were enumerated from the RST0 index (16-byte file header, 36-byte
resource records, and 8-byte chunk headers); TEX flags, wrap byte, and mip count
were read from the first 16 bytes of each resource payload. Bundle hashes pin
the exact supplied inputs.

## Viper AI livery bundles

Each present bundle below contains `Viper1.tex` through `Viper7.tex`. Every
listed texture has payload size 174,828 bytes, flags `0`, mip count `9` (256 by
256 base level), and wrap byte `1`.

| Bundle | SHA-256 |
| --- | --- |
| `easy.res` | `00843f9df27774f2131943b87cd45b612a2dfd1f2be1d709904768b688d79418` |
| `medium.res` | `1f88725ea11e0b59d6d2bf016c7fe13657473336bb396d55e957db0c826e6c71` |
| `hard.res` | `84d59c00e9b12ad458e79500a3be1cd56a2f1cba37f94f2d959fe6fdaf21053c` |
| `career1.res` | `17f0028413de75fe3a192c421ba8f459cec7fe7f1d6610c2e1748946ca52239b` |
| `career2.res` | `877ab6e3b8486d1392f515cab9d475cd2601ad6e42bc19be5ea99ba7548fa724` |
| `career3.res` | `84d59c00e9b12ad458e79500a3be1cd56a2f1cba37f94f2d959fe6fdaf21053c` |
| `career4.res` | `f9c0306ea4a8e4ac514fd13f8f9422698e08a2b19c55459cd90f71e0a3a9cf11` |

The extracted Data directory has no `career5.res`; opponent profile index 7
therefore has no bundle to satisfy the current `Viper1.tex`–`Viper7.tex`
lookup and uses flat body colors. The `hard.res` and `career3.res` hashes match,
so both names currently reference identical bundle bytes.

The game-side profile map follows the reconstructed opponent-strength order:
0 Easy, 1 Intermediate, 2 Hard, and 3-7 Career 1-5. This filename mapping and
the candidate livery names agree with the available extracted bundles and the
static selector path documented in [model_resource.md](model_resource.md).

## Shared Viper model materials

The extracted `race.res` SHA-256 is
`b07f40ef8dcba98fb7cef78c87034c9a2b47b9dcd7679aa70777c039be43d7e2`.

| Material entry | Payload bytes | Flags | Mip count / base size | Wrap byte | Runtime sampler / material handling |
| --- | ---: | ---: | --- | ---: | --- |
| `ucar.tex` | 43,756 | `0` | `8` / 128 by 128 | `0` | Clamp (inferred); opaque RGB565 |
| `wheels.tex` | 174,828 | `1` | `9` / 256 by 256 | `2` | Mirror (unconfirmed); RGB565 colorkey and alpha blending |
| `effects.tex` | 43,756 | `2` | `8` / 128 by 128 | `1` | Wrap; ARGB4444 alpha blending |

These names directly match the Viper0 model's `UCAR.tex`, `WHEELS.tex`, and
`EFFECTS.tex` groups. The renderer tries an exact material-name lookup in the
car bundle first, then `race.res`. The pinned [external format
reference](https://github.com/HerbFargus/viper-racing-modding/blob/e8435ddce817eb37502d1dd1080b64ee27f44515/docs/reference/file-formats.md)
associates wrap `1` with tileable surfaces and wrap `0` with unique decals;
wrap `2` appears on one colorkeyed texture. The native parser copies this byte to
texture-object offset `+0x17` at `0x4598E4`, but the inspected creation routine
`0x459ED0` does not consume that field. The renderer maps `0` to clamp, `1` to
wrap, and `2` to mirror; the first two are evidence-informed and the mirror
interpretation is a convention-based hypothesis. Transparent group sorting
remains unresolved.

The Viper car bundle additionally contains `viperd.tex` (flags `2`),
`Viperd1.tex` through `Viperd4.tex` (flags `0`), and `Viperw.tex` (flags `3`).
Those names do not directly match the inspected Viper0 group labels; the
renderer leaves them to the native alias path rather than guessing a binding.
Flags `3` remain unsupported by the texture decoder.

## Evidence limits

The decoded base levels and group bindings are a reconstruction informed by
the native candidate-name flow, supplied resource indexes, and the pinned
external format reference. No original-client runtime capture or independent
pixel comparison established UV order, confirmed all wrap-mode mappings,
transparency ordering,
native material priority, or visual parity. See
[model_resource.md](model_resource.md) for the address-level evidence and
remaining unknowns.
