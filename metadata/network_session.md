# Reconstructed multiplayer session slice

`viper_network_session` provides the reconstructed client's TCP host/join
transport. `viper_game --host PORT` and `viper_game --join HOST PORT` connect
one peer before entering the selected race. `--network-slot N` chooses that
client's zero-based human grid slot (the configured default remains in effect
when omitted). Each client sends its own complete
route-motion state after its local runtime update. The receiver replaces the
peer-owned route entrant with that snapshot and projects its pose onto the
shared ideal line; it does not re-simulate the peer from locally received
controls. This is replicated state for the route-motion prototype, not native
rigid-body state or original-client protocol compatibility.

## Protocol v3

All multibyte values are unsigned big-endian. A client sends a 12-byte hello
(`VRN3`, u16 version 3, role byte 0, three reserved bytes); the host validates
it and returns the same record with role/status byte 1. Version mismatch or an
unexpected role aborts the connection.

Each snapshot is a TCP frame: u32 body length, u8 type (`1` = state snapshot),
u32 sequence, u32 simulation tick, then 0–65536 payload bytes. The length
counts the body. Complete snapshots are reliable and ordered by TCP. The
receiver accepts only strictly newer 32-bit sequence values, using
serial-number arithmetic so wraparound works; stale or repeated frames are
consumed and reported as `false`.

The race-state payload is exactly 40 bytes: `VRS1`; u32 route segment; u32 lap
start segment; binary32 route fraction, steering, speed; u32 completed laps;
binary32 elapsed seconds and signed route distance; one finished flag; and
three reserved zero bytes. Segment bounds are checked against the receiving
track, lap counts against the agreed race setup, and the decoded state must
remain inside the route adapter's finite value ranges. The receiver also
rejects a simulation tick that moves backwards under 32-bit serial arithmetic,
while allowing repeated ticks when no fixed update occurred.

Before opening the race window, both clients send a `VRC2` configuration as
state sequence 0. The bounded record contains track and car resource stems,
lap count, opponent-strength profile, entrant count, and the local human's
grid slot. Shared race fields must match and the two player slots must be
distinct and in range. Each receiver applies the peer's state to that
negotiated slot and skips local AI advancement for it; this removes the prior
slot-0/slot-1 assumption. Resource stems are restricted to 1–32 ASCII letters,
digits, `_`, or `-`; laps must be 1–99; the profile must
be one of the eight parsed AIDR groups; and entrant count must be 2–64. The
first per-update state exchange begins at sequence 1. This agrees on the
race fields currently owned by the runtime; it does not prove identical asset
hashes, participant identities, controls, or deterministic physical simulation.

`Session::host(port)` blocks for one client; `Session::join(host, port)`
performs connect and handshake. The runtime still exchanges once per rendered
frame synchronously, so a slow peer can stall both clients. There is no pose
interpolation, lag/loss recovery, reconnection, multiple-peer host, lobby, or
original-client compatibility. Because the replicated state is still the
route-motion prototype, this is not accepted gameplay multiplayer.

## Regression coverage and limits

`network_session_regression` checks handshake, bidirectional framing, tick and
payload preservation, stale sequence rejection and wraparound, exact race
state/configuration bytes, malformed state flags, bounded state values,
configuration bounds, and distinct compatible player slots.
`race_progression_regression` verifies that projecting
a replicated route state reproduces its pose without advancing it. With
external game assets installed, `game_network_race_smoke` verifies mismatched
setup rejection and connects two matching Viper/Bemidji clients through the
snapshot exchange and bounded route-runtime lifecycle. It does not verify
sustained interactive play, peer pose convergence under unequal frame rates,
disconnect behavior, or physical parity.
