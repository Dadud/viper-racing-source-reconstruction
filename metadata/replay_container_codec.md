# Native replay container codec

`viper/replay_file.hpp` provides a lossless reader and writer for the observed
version 6 `.rpl` container. It retains the fixed `0xCD4` engine record as
opaque bytes, decodes the seven-word little-endian header, preserves the
`0xE3F`-word table and opaque payload, and rejects unsupported or malformed
files. It does not interpret packet contents, emulate the native replay UI, or
claim that a decoded replay can drive the reconstructed race.

## Evidence and accepted format

The shipped `game_install/GameData/bench.rpl` fixture is 149,408 bytes. Its
header starts at offset `0xCD4` and contains cookie `0x5245504C`, version `6`,
timestamp/index `0`, frame count `224`, base packet size `575`, word count
`3647`, and payload length `131508`. The exact size is:

```
0xCD4 + 0x1C + (3647 * 4) + 131508 = 149408
```

The parser model in `replay_format.cpp` independently specifies the cookie,
version, expected runtime word count, 2 MiB payload cap, frame-count bound,
packet-size comparison, and table-then-payload reads. The serializer model in
`replay_save.cpp` specifies the fixed record, version 6 header, word table,
and opaque tail writes. The codec enforces these observed constraints and
requires exact section lengths; it does not attempt to recover truncated data.

The regression uses the installed `bench.rpl` when external game data is
available and verifies byte-for-byte decode/encode equality. A synthetic
round-trip and malformed-length/version checks also run without external data.

## Remaining replay work

Native load calls a separate fixed-record validator at `0x45C590`, then the
file parser at `0x426790`. UI load/save behavior is represented by planners but
not connected to the game's frontend. The contents of the replay payload and
frame table are still opaque, and packet playback/recording must be recovered
before supporting replay motion or advertising compatible replay operation.

For related observations, see `metadata/frontend_dispatch.md` and the replay
load/save/format models in `include/viper/` and `src/`.
