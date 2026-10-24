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

The Win32 runtime loads the selected car's idle and numbered samples into
DirectSound PCM buffers and loops them. It uses provisional speed/throttle
selection and pitch scaling; it does not yet consume the native `ESFX` band
parameters. `--smoke-audio --smoke-frames 30` initializes the DirectSound
device and exercises buffer creation/play calls. The smoke run does not capture
speaker output or compare timing and pitch against the original. The separate
top-level `trk2.sfx` file's `MGI!` payload remains unparsed.
