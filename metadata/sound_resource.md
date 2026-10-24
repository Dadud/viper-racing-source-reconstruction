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
does not yet identify resource decoding, buffer format, playback callbacks,
or the runtime mixing graph. No decoded audio or audio playback has been
verified in the reconstruction.
