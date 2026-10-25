# Reconstructed multiplayer session slice

`viper_network_session` provides the reconstructed client's TCP host/join transport. `viper_game --host PORT` and `viper_game --join HOST PORT` connect one peer before entering the selected race. After each local runtime update, each side sends normalized steering, throttle, and brake values; the peer's latest controls drive the first non-player route entrant on the next local update. This owns transport through the runtime argument/control path, while the route integrator remains provisional reconstructed-client behavior.

## Protocol v2

All multibyte values are unsigned big-endian. A client sends a 12-byte hello (`VRN2`, u16 version 2, role byte 0, three reserved bytes); the host validates it and returns the same record with role/status byte 1. Version mismatch or an unexpected role aborts the connection.

Each snapshot is a TCP frame: u32 body length, u8 type (`1` = state snapshot), u32 sequence, u32 simulation tick, then 0–65536 payload bytes. The length counts the body. Complete snapshots are reliable and ordered by TCP. The receiver accepts only strictly newer 32-bit sequence values, using serial-number arithmetic so wraparound works; stale or repeated frames are consumed and reported as `false`. The provisional race-input payload is exactly three IEEE-754 binary32 values (steering, throttle, brake) in big-endian byte order, with ranges `[-1,1]`, `[0,1]`, and `[0,1]`. Other payloads remain caller-defined.

`Session::host(port)` blocks for one client; `Session::join(host, port)` performs connect and handshake. Each side may send and receive snapshots after connect. Transport failures and malformed frames throw `std::runtime_error`.

## Pre-race setup agreement

Before opening the race window, both clients send a `VRC1` configuration as
state sequence 0 and compare the peer's record with their own. The bounded
record contains track and car resource stems, lap count, opponent-strength
profile, and entrant count. Both clients abort before entering gameplay if
the payload is malformed or any field differs. Resource stems are restricted
to 1–32 ASCII letters, digits, `_`, or `-`; laps must be 1–99; the profile must
be one of the eight parsed AIDR groups; and entrant count must be 2–64. The
first per-update control exchange begins at sequence 1. This agrees on the
race fields currently owned by the runtime; it does not prove identical asset
hashes, participant identities, controls, physics state, or deterministic
simulation.

## Regression coverage and limits

`network_session_regression` opens a localhost host and client, checks handshake, bidirectional snapshot transfer, tick/payload preservation, stale sequence rejection, and the normalized race-input and race-configuration codec round-trip/bounds checks. It uses a fixed local port (39173), so parallel copies of the regression should not run simultaneously. When original assets are installed, `game_network_race_smoke` first verifies that Viper/Bemidji and Dundas/Viper clients reject the mismatch, then launches two matching Viper/Bemidji clients and verifies both complete a short route-runtime smoke. The runtime path is one peer only and blocks once per rendered frame for input exchange. There is still no authoritative physics simulation, remote full-state replication, interpolation, lag handling, reconnection, multiple peers, or legacy-client compatibility. The paired smoke verifies setup agreement, process connection, and bounded route-runtime completion only; it does not verify sustained interactive play or matching physics.
