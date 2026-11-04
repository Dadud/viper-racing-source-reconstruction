# Sound resource investigation

The installed `GameData/trk2.sfx` is separate from the repository's `RST0`
`ResourceBundle` format: the existing bundle reader rejects it. The shipped
file is 2,911,920 bytes, SHA-256
`0CE938B34F5233E2F24B3B2966E37E0AFB0B0E40DECCE5471C442410E7BE5083`.
Its first 16 bytes are `30 53 45 52 30 58 46 53 01 00 00 00 00 00 00 00`
(`0SER0XFS`, version word 1, zero word). At offset `0x10` it contains the
bytes `21 49 47 4D` (`!IGM`), followed by `0x002C5E86` at `0x14`.

The DWORDs at `0x18..0x27` can be divided into values resembling a PCM format
descriptor: `1`, `1`, `22050`, `44100`, `2`, and `16`. The following word at
`0x28` is `0x1364`, which does not fit the standard PCM `WAVEFORMATEX` header
interpretation as `cbSize`. The bytes from `0x2C` onward have not been proven
to be raw samples or any known codec. These are byte-level observations, not
an audio-format identification.

The game sound-manager path is documented in
[`vehicle_setup.md`](vehicle_setup.md): `race.bin` requests `ESFX` resources
and selects engine sample names from the selected car's setup, with an
`engine.txt` fallback. The executable imports `DirectSoundCreate`, but this
does not identify the separate `trk2.sfx` decoder or full native mixing graph.

## Car bundle `SFX0` entries

Car bundles use a different `SFX0` resource type (`0x53465830`) for the PCM
engine samples selected by the `ESFX` setup resource. Their 20-byte prefix is
observed as a DWORD sample-data byte count followed by PCM tag, channels, sample
rate, average byte rate, block alignment, and bits per sample. The declared
sample data immediately follows the prefix. All remaining bytes are retained
as opaque tail data; the five shipped car bundles have 4,098 such bytes per
`SFX0` resource. The reader in `include/viper/sound_resource.hpp` checks the
format and bounds but does not assign meaning to this tail. Its regression
reads all shipped car bundles using external assets.

The Win32 runtime now requires the selected car's `%se.ens` `ESFX` setup and
loads its idle `%si.sfx` resource plus exactly the declared band count. Each
band tries `%s%d.sfx` first and `engine%d.sfx` second, matching the recovered
native resource-name path. Every DirectSound band buffer retains its matching
seven-float setup record, but
none is started and all volumes remain at the DirectSound minimum. The previous
route-speed/throttle band selector and route-speed pitch scaling were removed:
these are not the native ESFX inputs. `0x46C7A0` reads vehicle values through
virtual offsets `+0x48` and `+0x44`, then applies per-band data through
`0x46CA10`. The PCAR getters map `+0x48` to body `+0xD70` and `+0x44` to
`+0xDE0` or `+0xC34` depending on global `DAT_00508A40`. The runtime now
models the `+0xD70` producer: after its four-wheel contact/history pass it reads
the configured curve object's getter value at parent `+0xC0C` and copies the
same bits into body `+0xD70`, with both values included in the fixed-step trace.
For the ordinary elapsed-scheduler path, the shared `DAT_00508A40` gate is
false, so the `+0x44` getter selects `+0xC34`. That field is curve-object
`+0x38`, initialized to zero by `0x440100`; fixed-step traces now record this
selected getter value as a diagnostic input. The alternate true-gate `+0xDE0`
path and downstream ESFX-to-DirectSound mixing remain unverified, so audio
playback remains muted.
`--smoke-audio --smoke-frames 30` initializes DirectSound and exercises resource
and PCM-buffer creation only. Current runs load Viper's four resources (idle plus
three bands), and Exotic, Plane, Sedan, and Sports' three resources each (idle
plus two bands); these checks do not start playback or establish audible
behavior. Previous looped playback and its buffer-loss recovery were removed
with the unsupported route-speed/throttle heuristic. Speaker output capture and
comparison remain open. The separate top-level `trk2.sfx` file's `MGI!` payload
remains unparsed.

### Native vehicle values used by ESFX playback

The PCAR vtable installed by constructor `0x438430` is rooted at `0x4C8188`.
Its `+0x44` and `+0x48` slots point to the two getters called by `0x46C7A0`.
The `+0x44` getter `0x427E20` returns body `+0xDE0` when global byte
`DAT_00508A40` is nonzero, otherwise body `+0xC34`. The `+0x48` entry at
`0x427E40` loads body `+0xD70` and returns that x87 value. `0x46C7A0` passes
the latter to the band update and uses the former both in its `0.1` idle-sample
gate and in the gain expression. Evidence: `0x438430`
`ev_510bfe4e40f83845cad49b0e753b4ec819125f7cf1ff2334d871c3d0b3013e30`,
vtable slot file mappings `ev_5dd55ac1aab781225385fe1e65639110bfeba619438e753f6f563a66b10343ae`
and `ev_027f5b0720eb26551da6442468e1d185088a418e0f2631bac565d55dd8bf2935`,
getter `0x427E20` `ev_2c2e757c202f1282c4607fe775bb820e6862895d2f9263f976fe5076df91cee3`,
gate `0x426E50` `ev_e01201a9050e5f41ffc99d59d85028d41c3bd333c20642039e17ec0a72b2cb5f`,
and getter entry/return instructions `ev_92caab18e249e08201e8e6ad415a8fbe55ba1b403b497d6120812b48b4a23b1b`
and `ev_7e58fe89138b7ccfbe31d211d5ddef8588b07678e47bcac0f70110246c52fb07`.
The sound-manager behavior is in `0x46C7A0`
`ev_7cc415200b8badae62ec3aa0395159882ba6263f9ff2238c771e3a5489c2a9dc`;
its ESFX interpolation/update path is `0x46CA10`
`ev_97d1dbf313edcc14af9f77ccfe7f45c328151d27e3d5eab2a78f66ac4dc0e993`.

These observations establish the native source fields and gate, but not their
semantic meanings or units. At `0x4313F2..0x4313FC`, `0x431150` calls virtual
slot `+0` through the curve subobject at parent `+0xBFC` and stores the x87
return at `+0xD70`. The configured object has vtable `0x4C8780`; slot `+0`
targets `0x43F970`, which returns curve-object `+0x10` (parent `+0xC0C`).
Constructor `0x4401D0` initializes that field to `1000.0`. The fixed-step
runtime now models this concrete getter and store, and the route-free trace
regression requires the observed getter and stored sample to match and remain
finite. The `+0x44` callback's current false-gate source is also mapped to
curve child `+0x38` (parent `+0xC34`), initialized to zero; the trace requires
that value for this scheduler mode. The alternate true-gate `+0xDE0` branch,
meaning/units of either getter, and ESFX mixing remain unverified. Audio remains
muted; the runtime must not feed route speed or keyboard throttle directly
into ESFX tuning on this evidence alone.
