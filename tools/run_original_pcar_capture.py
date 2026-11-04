#!/usr/bin/env python3
"""Run one bounded PCAR source capture against a separately staged ISO copy.

The host launches the original race.bin suspended, installs the video-memory
startup hook and the two source/control agents, then resumes it. The staged
copy must have sound disabled. Original binaries and captures stay external to
this source repository.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path


EXPECTED_ISO_RACE_SHA256 = "f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56"
HOST_TIMEOUT_SECONDS = 135
CANARY_SEMAPHORE_NAME = "MGI Viper Racing 1998 Canary"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sound_settings(path: Path) -> dict[str, str]:
    section = ""
    settings: dict[str, str] = {}
    for raw_line in path.read_text(encoding="ascii").splitlines():
        line = raw_line.strip()
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].upper()
        elif section == "SOUND" and line and not line.startswith(";"):
            fields = line.split(None, 1)
            if len(fields) == 2 and fields[0] in {
                "enabled", "effects_volume", "music_volume", "cd_audio"
            }:
                settings[fields[0]] = fields[1]
    return settings


def hold_canary_semaphore(name: str = CANARY_SEMAPHORE_NAME) -> tuple[object, int, bool]:
    """Supply the named-object precondition expected by engine startup."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create = kernel32.CreateSemaphoreW
    create.argtypes = (ctypes.c_void_p, ctypes.c_long, ctypes.c_long, ctypes.c_wchar_p)
    create.restype = ctypes.c_void_p
    close = kernel32.CloseHandle
    close.argtypes = (ctypes.c_void_p,)
    close.restype = ctypes.c_int
    handle = create(None, 0, 1, name)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    preexisting = ctypes.get_last_error() == 183  # ERROR_ALREADY_EXISTS
    return kernel32, int(handle), preexisting


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", required=True, type=Path,
                        help="isolated GameRoot containing Data/race.bin")
    parser.add_argument("--frida-python-path", required=True, type=Path,
                        help="directory containing the installed frida Python package")
    parser.add_argument("--capture-dir", required=True, type=Path,
                        help="external directory for private JSONL output")
    parser.add_argument("--expected-sha256", default=EXPECTED_ISO_RACE_SHA256)
    args = parser.parse_args()

    game_root = args.game_root.resolve(strict=True)
    race_bin = (game_root / "Data" / "race.bin").resolve(strict=True)
    expected_cwd = game_root
    capture_dir = args.capture_dir.resolve()
    if not race_bin.is_file() or race_bin.parent != (game_root / "Data").resolve():
        raise SystemExit("race.bin must be a file directly inside the staged Data directory")

    actual_hash = sha256(race_bin)
    if actual_hash.lower() != args.expected_sha256.lower():
        raise SystemExit(f"race.bin SHA-256 mismatch: {actual_hash}")

    required_sound = {
        "enabled": "no",
        "effects_volume": "0.000000",
        "music_volume": "0.000000",
        "cd_audio": "no",
    }
    checked_sound: dict[str, dict[str, str]] = {}
    for relative in (Path("Data/options.def"), Path("Config/options.cfg")):
        path = game_root / relative
        current = sound_settings(path)
        if any(current.get(key) != value for key, value in required_sound.items()):
            raise SystemExit(f"Sound is not fully disabled in staged {relative}: {current}")
        checked_sound[str(relative)] = current

    frida_path = args.frida_python_path.resolve(strict=True)
    sys.path.insert(0, str(frida_path))
    try:
        import frida  # type: ignore[import-not-found]
    except ImportError as error:
        raise SystemExit(f"Cannot import Frida from {frida_path}: {error}") from error

    repo_tools = Path(__file__).resolve().parent
    agents = {
        "startup_vram": repo_tools / "original_startup_vram_override.js",
        "pcar_source_watch": repo_tools / "pcar_source_field_watch.js",
        "controlled_wheel_capture": repo_tools / "original_pcar_controlled_wheel_capture.js",
    }
    missing_agents = [str(path) for path in agents.values() if not path.is_file()]
    if missing_agents:
        raise SystemExit(f"Missing capture agents: {missing_agents}")

    capture_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_path = capture_dir / f"pcar_source_capture_{timestamp}.jsonl"
    finished = threading.Event()
    detached = threading.Event()
    done_agents: set[str] = set()
    output_lock = threading.Lock()
    device = frida.get_local_device()
    existing_game_processes = [
        process.name for process in device.enumerate_processes()
        if process.name.lower() in {"race.bin", "viper racing.exe"}
    ]
    if existing_game_processes:
        raise SystemExit(
            "An original game process is already running; refusing to attach or launch: "
            + ", ".join(existing_game_processes)
        )
    pid: int | None = None
    session = None
    scripts = []
    canary_kernel32 = None
    canary_handle = None
    canary_preexisting = False

    def record(row: dict[str, object]) -> None:
        with output_lock:
            with output_path.open("a", encoding="utf-8", newline="\n") as output:
                output.write(json.dumps(row, separators=(",", ":"), default=str) + "\n")
                output.flush()

    record({
        "event": "capture_host_start",
        "utc": datetime.now(timezone.utc).isoformat(),
        "race_bin": str(race_bin),
        "race_bin_sha256": actual_hash,
        "working_directory": str(expected_cwd),
        "sound_settings": checked_sound,
        "frida_version": frida.__version__,
        "agents": {name: str(path) for name, path in agents.items()},
        "host_timeout_seconds": HOST_TIMEOUT_SECONDS,
    })

    def on_message(agent_name: str):
        def handler(message: dict[str, object], data: bytes | None) -> None:
            if message.get("type") == "send":
                payload = message.get("payload", {})
                row = payload if isinstance(payload, dict) else {"payload": payload}
                row = {**row, "agent": agent_name, "host_utc": datetime.now(timezone.utc).isoformat()}
                record(row)
                if agent_name == "pcar_source_watch" and row.get("event") == "pcar_source_field_watch_done":
                    done_agents.add(agent_name)
                if agent_name == "controlled_wheel_capture" and row.get("event") == "controlled_pcar_capture_done":
                    done_agents.add(agent_name)
                if done_agents == {"pcar_source_watch", "controlled_wheel_capture"}:
                    finished.set()
            else:
                record({
                    "event": "frida_script_message",
                    "agent": agent_name,
                    "message": message,
                    "data_length": len(data) if data is not None else 0,
                    "host_utc": datetime.now(timezone.utc).isoformat(),
                })
        return handler

    def on_detached(reason: object, crash: object) -> None:
        record({"event": "frida_session_detached", "reason": reason, "crash": crash})
        detached.set()

    try:
        canary_kernel32, canary_handle, canary_preexisting = hold_canary_semaphore()
        record({"event": "expected_canary_semaphore_held",
                "name": CANARY_SEMAPHORE_NAME,
                "preexisting": canary_preexisting,
                "host_utc": datetime.now(timezone.utc).isoformat()})
        pid = device.spawn([str(race_bin), "-1"], cwd=str(expected_cwd))
        session = device.attach(pid)
        session.on("detached", on_detached)
        for name, path in agents.items():
            script = session.create_script(path.read_text(encoding="utf-8"))
            script.on("message", on_message(name))
            script.load()
            scripts.append(script)
        record({"event": "capture_agents_loaded", "pid": pid,
                "host_utc": datetime.now(timezone.utc).isoformat()})
        device.resume(pid)
        print(f"Capture running in muted isolated copy; pid={pid}", flush=True)
        print("Select Quick Race > Bemidji > RACE in the game window; the agents capture two minutes and then this host closes the game.", flush=True)
        deadline = time.monotonic() + HOST_TIMEOUT_SECONDS
        while time.monotonic() < deadline and not finished.is_set() and not detached.is_set():
            time.sleep(0.2)
        outcome = "both_agents_completed" if finished.is_set() else (
            "target_detached" if detached.is_set() else "host_timeout")
        record({"event": "capture_host_end", "outcome": outcome,
                "done_agents": sorted(done_agents),
                "host_utc": datetime.now(timezone.utc).isoformat()})
        print(f"Capture ended: {outcome}; output={output_path}", flush=True)
        return 0 if finished.is_set() else 2
    finally:
        for script in scripts:
            try:
                script.unload()
            except Exception:
                pass
        if session is not None:
            try:
                session.detach()
            except Exception:
                pass
        if pid is not None:
            try:
                device.kill(pid)
                print(f"Stopped capture process pid={pid}", flush=True)
            except Exception:
                pass
        if canary_kernel32 is not None and canary_handle is not None:
            try:
                canary_kernel32.CloseHandle(ctypes.c_void_p(canary_handle))
            except Exception:
                pass


if __name__ == "__main__":
    raise SystemExit(main())
