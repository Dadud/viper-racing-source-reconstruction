# Reconstructed multiplayer session slice

`viper_network_session` is a standalone TCP host/join transport for reconstructed clients. It is intentionally independent of legacy-client interoperability and currently is not wired into the game's race UI or simulation loop.

## Protocol v1

All multibyte values are unsigned big-endian. A client sends a 12-byte hello (`VRN1`, u16 version, role byte 0, three reserved bytes); the host validates it and returns the same record with role/status byte 1. Version mismatch or an unexpected role aborts the connection.

Each snapshot is a TCP frame: u32 body length, u8 type (`1` = state snapshot), u32 sequence, u32 simulation tick, then 0–65536 opaque payload bytes. The length counts the body. Complete snapshots are reliable and ordered by TCP. The receiver accepts only strictly newer 32-bit sequence values, using serial-number arithmetic so wraparound works; stale or repeated frames are consumed and reported as `false`. Payload schema, authority policy, interpolation, and simulation integration belong to the caller and are not defined yet.

`Session::host(port)` blocks for one client; `Session::join(host, port)` performs connect and handshake. Each side may send and receive snapshots after connect. Transport failures and malformed frames throw `std::runtime_error`.

## Regression coverage and limits

`network_session_regression` opens a localhost host and client, checks handshake, bidirectional snapshot transfer, tick/payload preservation, and stale sequence rejection. It uses a fixed local port (39173), so parallel copies of the regression should not run simultaneously. This verifies the session transport only; no race lobby, vehicle input/state codec, reconnection, multiple clients, or game runtime integration is implemented.
