#!/usr/bin/env python3
"""Run one bounded PCAR source capture via the isolated official launcher.

The launcher-created race.bin child is suspended long enough to install
observers. The staged copy keeps sound initialized with effects/music at zero
and CD audio disabled. Original binaries and captures stay external.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path


EXPECTED_ISO_RACE_SHA256 = "f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56"
EXPECTED_LAUNCHER_SHA256 = "d75b38228d4b68b2c5ef39252b95e43a9de16976005c26fab4491d05ce3d32d4"
HOST_TIMEOUT_SECONDS = 180
DEFAULT_STARTUP_WINDOW_SECONDS = 15
MAX_STARTUP_WINDOW_SECONDS = 300
CANARY_SEMAPHORE_NAME = "MGI Viper Racing 1998 Canary"


def capture_agent_names(startup_only: bool, include_source_watch: bool = False,
                        include_controlled_capture: bool = False,
                        include_graphics_trace: bool = False,
                        include_thread_sample: bool = False) -> tuple[str, ...]:
    names = ["startup_vram", "exit_status"]
    if not startup_only or include_source_watch:
        names.append("pcar_source_watch")
    if not startup_only or include_controlled_capture:
        names.append("controlled_wheel_capture")
    if include_graphics_trace:
        names.append("graphics_startup_trace")
    if include_thread_sample:
        names.append("startup_thread_sampler")
    return tuple(names)


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


def mounted_volume_label(root: str) -> str:
    """Read a mounted Windows volume label without changing mount state."""
    volume_root = root.rstrip("\\/") + "\\"
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_info = kernel32.GetVolumeInformationW
    get_info.argtypes = (
        wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD),
        ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, wintypes.DWORD,
    )
    get_info.restype = wintypes.BOOL
    label = ctypes.create_unicode_buffer(261)
    if not get_info(volume_root, label, len(label), None, None, None, None, 0):
        raise ctypes.WinError(ctypes.get_last_error())
    return label.value


def validate_disc_volume_label(drive: str, label: str) -> str:
    """Require the disc label expected by the official ISO launcher path."""
    if label.casefold() != "viper":
        raise SystemExit(
            f"Expected the original VIPER disc mounted at {drive}; found label {label!r}")
    return label


def required_capture_sound_settings(process_silent_audio: bool = False) -> dict[str, str]:
    if process_silent_audio:
        # Keep CD playback disabled. DirectSound buffers are silenced in the
        # target process before the engine resumes, while the game's effects
        # and music configuration remains at its known active values.
        return {
            "enabled": "yes",
            "effects_volume": "1.000000",
            "music_volume": "1.000000",
            "cd_audio": "no",
        }
    return {
        "enabled": "yes",
        "effects_volume": "0.000000",
        "music_volume": "0.000000",
        "cd_audio": "no",
    }


def canary_semaphore_strategy(direct_engine: bool) -> str:
    # The official launcher creates the canary itself and holds it while
    # starting race.bin. Pre-creating it here makes the launcher treat this as
    # a duplicate/copy-protection launch and exit before its preflight.
    return "host_holds_for_direct_engine" if direct_engine else "launcher_creates_for_child"


def resolve_launcher_arguments(arguments: list[str] | None) -> tuple[str, ...]:
    # Match the recovered primary-adapter launch used in the controlled
    # original startup reference. Earlier runner traces showed this switch was
    # not reaching race.bin despite an outdated comment claiming it did.
    return tuple(arguments) if arguments is not None else ("-1",)


def query_process_exit_status(process_id: int) -> dict[str, object]:
    """Read the target process exit code after a Frida detach notification."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    open_process = kernel32.OpenProcess
    open_process.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    open_process.restype = wintypes.HANDLE
    handle = open_process(0x1000, False, process_id)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        return {"exit_status_query_error": ctypes.get_last_error()}
    try:
        value = wintypes.DWORD()
        get_exit_code = kernel32.GetExitCodeProcess
        get_exit_code.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
        get_exit_code.restype = wintypes.BOOL
        if not get_exit_code(handle, ctypes.byref(value)):
            return {"exit_status_query_error": ctypes.get_last_error()}
        return {"process_exit_code": value.value,
                "process_exit_code_hex": f"0x{value.value:08X}",
                "process_still_active": value.value == 259}
    finally:
        kernel32.CloseHandle(handle)


def active_capture_target_names(processes: list[object],
                                status_query=query_process_exit_status) -> list[str]:
    """Ignore stale Frida process entries after verifying OS exit status.

    A detached session can leave a terminated target in Frida's enumeration
    briefly. Unknown status remains a refusal so the runner never attaches to
    a target whose liveness could not be checked.
    """
    names = []
    for process in processes:
        name = str(getattr(process, "name", ""))
        if name.lower() not in {"race.bin", "viper racing.exe"}:
            continue
        process_id = int(getattr(process, "pid"))
        status = status_query(process_id)
        if status.get("process_still_active") is not False:
            names.append(name)
    return names


def cleanup_capture_instrumentation(sessions: list[object | None],
                                   scripts: list[object]) -> None:
    """Detach sessions before unloading scripts to avoid stale-session hangs."""
    for session in sessions:
        if session is None:
            continue
        try:
            session.detach()
        except Exception:
            pass
    for script in scripts:
        try:
            script.unload()
        except Exception:
            pass


def capture_wait_seconds(startup_only: bool, startup_window_seconds: int) -> int:
    if not startup_only:
        return HOST_TIMEOUT_SECONDS
    if not 1 <= startup_window_seconds <= MAX_STARTUP_WINDOW_SECONDS:
        raise ValueError(
            f"startup window must be between 1 and {MAX_STARTUP_WINDOW_SECONDS} seconds")
    return startup_window_seconds


def resume_thread_until_running(thread_id: int, maximum_calls: int = 16) -> dict[str, object]:
    """Resume the exact suspended child thread created by the launcher hook."""
    if thread_id <= 0 or maximum_calls <= 0:
        raise ValueError("thread_id and maximum_calls must be positive")
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    open_thread = kernel32.OpenThread
    open_thread.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    open_thread.restype = wintypes.HANDLE
    handle = open_thread(0x0002, False, thread_id)  # THREAD_SUSPEND_RESUME
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        resume = kernel32.ResumeThread
        resume.argtypes = (wintypes.HANDLE,)
        resume.restype = wintypes.DWORD
        previous = 0xFFFFFFFF
        for calls in range(1, maximum_calls + 1):
            previous = int(resume(handle))
            if previous == 0xFFFFFFFF:
                raise ctypes.WinError(ctypes.get_last_error())
            if previous <= 1:
                return {"thread_id": thread_id, "resume_calls": calls,
                        "previous_suspend_count": previous,
                        "already_running": previous == 0}
        raise RuntimeError(
            f"thread {thread_id} remains suspended after {maximum_calls} resumes "
            f"(last previous count {previous})")
    finally:
        kernel32.CloseHandle(handle)


def terminate_owned_process(process_id: int, exit_code: int = 0xC000013A) -> bool:
    """Terminate only a PID created by this runner and wait for its exit."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    open_process = kernel32.OpenProcess
    open_process.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    open_process.restype = wintypes.HANDLE
    handle = open_process(0x0001 | 0x001000 | 0x100000, False, process_id)
    if not handle:
        # ERROR_INVALID_PARAMETER / ERROR_NOT_FOUND means the process exited
        # between the host's final observation and cleanup.
        return ctypes.get_last_error() in (87, 1168)
    try:
        terminate = kernel32.TerminateProcess
        terminate.argtypes = (wintypes.HANDLE, wintypes.UINT)
        terminate.restype = wintypes.BOOL
        if not terminate(handle, exit_code):
            observed = query_process_exit_status(process_id)
            return observed.get("process_exit_code") != 259
        wait = kernel32.WaitForSingleObject
        wait.argtypes = (wintypes.HANDLE, wintypes.DWORD)
        wait.restype = wintypes.DWORD
        return wait(wintypes.HANDLE(handle), 5000) == 0
    finally:
        kernel32.CloseHandle(handle)


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


def create_launcher_suspended(launcher: Path, cwd: Path,
                              arguments: tuple[str, ...]) -> tuple[object, int, int, int, int]:
    """Start the staged official launcher suspended and return owned handles."""
    class StartupInfo(ctypes.Structure):
        _fields_ = [
            ("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
            ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
            ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
            ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
            ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
            ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
            ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
            ("lpReserved2", ctypes.POINTER(wintypes.BYTE)),
            ("hStdInput", wintypes.HANDLE), ("hStdOutput", wintypes.HANDLE),
            ("hStdError", wintypes.HANDLE),
        ]

    class ProcessInformation(ctypes.Structure):
        _fields_ = [
            ("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
            ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create = kernel32.CreateProcessW
    create.argtypes = (
        wintypes.LPCWSTR, wintypes.LPWSTR, ctypes.c_void_p, ctypes.c_void_p,
        wintypes.BOOL, wintypes.DWORD, ctypes.c_void_p, wintypes.LPCWSTR,
        ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInformation),
    )
    create.restype = wintypes.BOOL
    info = StartupInfo()
    info.cb = ctypes.sizeof(StartupInfo)
    process = ProcessInformation()
    command = ctypes.create_unicode_buffer(
        subprocess.list2cmdline([str(launcher), *arguments]))
    if not create(str(launcher), command, None, None, False, 0x00000004,
                  None, str(cwd), ctypes.byref(info), ctypes.byref(process)):
        raise ctypes.WinError(ctypes.get_last_error())
    close = kernel32.CloseHandle
    close.argtypes = (wintypes.HANDLE,)
    close.restype = wintypes.BOOL
    get_exit_code = kernel32.GetExitCodeProcess
    get_exit_code.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    get_exit_code.restype = wintypes.BOOL
    return (kernel32, int(process.hProcess), int(process.hThread),
            int(process.dwProcessId), int(process.dwThreadId))


def select_launcher_paths(game_root: Path,
                          launcher_path: Path | None = None) -> tuple[Path, Path]:
    """Select the staged official launcher and its historically verified cwd."""
    root = game_root.resolve(strict=True)
    launcher = (launcher_path or (root / "Viper Racing.exe")).resolve(strict=True)
    launcher_cwd = (root / "Data").resolve(strict=True)
    return launcher, launcher_cwd


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", required=True, type=Path,
                        help="isolated GameRoot containing Data/race.bin")
    parser.add_argument("--launcher-path", type=Path,
                        help="verified official launcher; defaults to the staged GameRoot copy")
    parser.add_argument("--launcher-arg", action="append", default=None,
                        help="argument forwarded by the official launcher (default: -1; repeat for multiple arguments)")
    parser.add_argument("--frida-python-path", required=True, type=Path,
                        help="directory containing the installed frida Python package")
    parser.add_argument("--capture-dir", required=True, type=Path,
                        help="external directory for private JSONL output")
    parser.add_argument("--disc-drive", default="F:",
                        help="drive holding the hash-verified VIPER ISO (default: F:)")
    parser.add_argument("--expected-sha256", default=EXPECTED_ISO_RACE_SHA256)
    parser.add_argument("--direct-engine", action="store_true",
                        help="spawn race.bin directly instead of the official launcher")
    parser.add_argument("--process-silent-audio", action="store_true",
                        help="allow active effects/music settings only with the in-process DirectSound volume clamp")
    parser.add_argument("--startup-only", action="store_true",
                        help="load only startup video-memory and process-exit observers; omit PCAR probes")
    parser.add_argument("--startup-window-seconds", type=int,
                        default=DEFAULT_STARTUP_WINDOW_SECONDS,
                        help="bounded --startup-only observation window (1..300 seconds; default 15)")
    parser.add_argument("--include-pcar-source-watch", action="store_true",
                        help="with --startup-only, add the exact PCAR constructor write watcher")
    parser.add_argument("--include-controlled-wheel-capture", action="store_true",
                        help="with --startup-only, add the controlled PCAR/wheel observer")
    parser.add_argument("--graphics-startup-trace", action="store_true",
                        help="trace recovered DirectDraw startup functions and native termination/exception events")
    parser.add_argument("--sample-main-thread", action="store_true",
                        help="sample the launcher-created engine thread PC/SP at 10 Hz for 10 seconds")
    args = parser.parse_args()
    launcher_arguments = resolve_launcher_arguments(args.launcher_arg)
    try:
        wait_seconds = capture_wait_seconds(
            args.startup_only, args.startup_window_seconds)
    except ValueError as error:
        raise SystemExit(str(error)) from error

    game_root = args.game_root.resolve(strict=True)
    race_bin = (game_root / "Data" / "race.bin").resolve(strict=True)
    # The successful suspended-child reference capture launched the verified
    # official launcher with GameRoot\Data as its working directory. Preserve
    # that process setup while keeping all staged settings and data isolated.
    launcher, launcher_cwd = select_launcher_paths(game_root, args.launcher_path)
    expected_cwd = race_bin.parent
    capture_dir = args.capture_dir.resolve()
    if not race_bin.is_file() or race_bin.parent != (game_root / "Data").resolve():
        raise SystemExit("race.bin must be a file directly inside the staged Data directory")

    actual_hash = sha256(race_bin)
    if actual_hash.lower() != args.expected_sha256.lower():
        raise SystemExit(f"race.bin SHA-256 mismatch: {actual_hash}")
    launcher_hash = sha256(launcher)
    if launcher_hash.lower() != EXPECTED_LAUNCHER_SHA256:
        raise SystemExit(f"Launcher SHA-256 mismatch: {launcher_hash}")
    disc_label = validate_disc_volume_label(
        args.disc_drive, mounted_volume_label(args.disc_drive))

    required_sound = required_capture_sound_settings(args.process_silent_audio)
    checked_sound: dict[str, dict[str, str]] = {}
    for relative in (Path("Data/options.def"), Path("Config/options.cfg")):
        path = game_root / relative
        current = sound_settings(path)
        if any(current.get(key) != value for key, value in required_sound.items()):
            raise SystemExit(f"Staged sound settings do not match the muted capture profile in {relative}: {current}")
        checked_sound[str(relative)] = current

    frida_path = args.frida_python_path.resolve(strict=True)
    sys.path.insert(0, str(frida_path))
    try:
        import frida  # type: ignore[import-not-found]
    except ImportError as error:
        raise SystemExit(f"Cannot import Frida from {frida_path}: {error}") from error

    repo_tools = Path(__file__).resolve().parent
    launcher_bridge = repo_tools / "original_launcher_child_bridge.js"
    agent_paths = {
        "startup_vram": repo_tools / "original_startup_vram_override.js",
        "exit_status": repo_tools / "original_exit_status_probe.js",
        "pcar_source_watch": repo_tools / "pcar_source_field_watch.js",
        "controlled_wheel_capture": repo_tools / "original_pcar_controlled_wheel_capture.js",
        "graphics_startup_trace": repo_tools / "original_graphics_startup_trace.js",
        "startup_thread_sampler": repo_tools / "original_startup_thread_sample.js",
    }
    agents = {name: agent_paths[name]
              for name in capture_agent_names(
                  args.startup_only, args.include_pcar_source_watch,
                  args.include_controlled_wheel_capture,
                  args.graphics_startup_trace,
                  args.sample_main_thread)}
    if args.process_silent_audio:
        agents["directsound_silencer"] = (
            repo_tools / "original_directsound_process_silencer.js")
    missing_agents = [str(path) for path in [launcher_bridge, *agents.values()]
                      if not path.is_file()]
    if missing_agents:
        raise SystemExit(f"Missing capture agents: {missing_agents}")

    capture_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_path = capture_dir / f"pcar_source_capture_{timestamp}.jsonl"
    finished = threading.Event()
    child_ready = threading.Event()
    launcher_detached = threading.Event()
    child_detached = threading.Event()
    audio_silencer_ready = threading.Event()
    done_agents: set[str] = set()
    child_info: dict[str, int] = {}
    shutdown_requested_processes: set[int] = set()
    output_lock = threading.Lock()
    device = frida.get_local_device()
    existing_game_processes = active_capture_target_names(
        device.enumerate_processes())
    if existing_game_processes:
        raise SystemExit(
            "An original game process is already running; refusing to attach or launch: "
            + ", ".join(existing_game_processes)
        )
    launcher_pid: int | None = None
    child_pid: int | None = None
    launcher_handles: tuple[object, int, int] | None = None
    launcher_session = None
    child_session = None
    launcher_scripts = []
    child_scripts = []
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
        "launcher": str(launcher),
        "launcher_sha256": launcher_hash,
        "launcher_working_directory": str(launcher_cwd),
        "disc_drive": args.disc_drive,
        "disc_volume_label": disc_label,
        "working_directory": str(expected_cwd),
        "sound_settings": checked_sound,
        "process_silent_audio": args.process_silent_audio,
        "startup_only": args.startup_only,
        "frida_version": frida.__version__,
        "launcher_bridge": str(launcher_bridge),
        "launch_mode": "direct_engine" if args.direct_engine else "official_launcher",
        "launcher_arguments": list(launcher_arguments),
        "agents": {name: str(path) for name, path in agents.items()},
        "host_timeout_seconds": wait_seconds,
    })

    def on_message(agent_name: str):
        def handler(message: dict[str, object], data: bytes | None) -> None:
            if message.get("type") == "send":
                payload = message.get("payload", {})
                row = payload if isinstance(payload, dict) else {"payload": payload}
                row = {**row, "agent": agent_name, "host_utc": datetime.now(timezone.utc).isoformat()}
                record(row)
                if agent_name == "launcher_bridge" and row.get("event") == "race_child_ready_for_attach":
                    child_info["pid"] = int(row["pid"])
                    child_info["tid"] = int(row["tid"])
                    child_ready.set()
                if agent_name == "pcar_source_watch" and row.get("event") == "pcar_source_field_watch_done":
                    done_agents.add(agent_name)
                if agent_name == "controlled_wheel_capture" and row.get("event") == "controlled_pcar_capture_done":
                    done_agents.add(agent_name)
                if agent_name == "directsound_silencer" and row.get("event") == "directsound_process_silencer_ready":
                    audio_silencer_ready.set()
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

    def on_detached(kind: str):
        def handler(reason: object, crash: object) -> None:
            row: dict[str, object] = {"event": "frida_session_detached",
                "session": kind, "reason": reason, "crash": crash}
            if kind == "launcher":
                if launcher_handles is not None:
                    kernel32, process_handle, _ = launcher_handles
                    exit_code = wintypes.DWORD()
                    if kernel32.GetExitCodeProcess(
                            wintypes.HANDLE(process_handle), ctypes.byref(exit_code)):
                        row["process_exit_code"] = exit_code.value
                launcher_detached.set()
            else:
                if child_pid is not None:
                    row.update(query_process_exit_status(child_pid))
                    row["termination_requested_by_host"] = (
                        child_pid in shutdown_requested_processes)
                child_detached.set()
            record(row)
        return handler

    try:
        if args.direct_engine:
            canary_kernel32, canary_handle, canary_preexisting = hold_canary_semaphore()
            record({"event": "expected_canary_semaphore_held",
                    "name": CANARY_SEMAPHORE_NAME,
                    "preexisting": canary_preexisting,
                    "strategy": canary_semaphore_strategy(True),
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            child_pid = device.spawn([str(race_bin), "-1"], cwd=str(expected_cwd))
            child_session = device.attach(child_pid)
            child_session.on("detached", on_detached("race.bin"))
        else:
            record({"event": "launcher_canary_delegated",
                    "name": CANARY_SEMAPHORE_NAME,
                    "strategy": canary_semaphore_strategy(False),
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            (launcher_kernel32, launcher_process_handle, launcher_thread_handle,
             launcher_pid, launcher_tid) = create_launcher_suspended(
                 launcher, launcher_cwd, launcher_arguments)
            launcher_handles = (launcher_kernel32, launcher_process_handle, launcher_thread_handle)
            launcher_session = device.attach(launcher_pid)
            launcher_session.on("detached", on_detached("launcher"))
            bridge_source = launcher_bridge.read_text(encoding="utf-8").replace(
                "__EXPECTED_CWD_JSON__", json.dumps(str(expected_cwd)))
            bridge = launcher_session.create_script(bridge_source)
            bridge.on("message", on_message("launcher_bridge"))
            bridge.load()
            launcher_scripts.append(bridge)
            record({"event": "launcher_bridge_loaded", "pid": launcher_pid,
                    "tid": launcher_tid, "cwd": str(launcher_cwd),
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            resume_thread = launcher_kernel32.ResumeThread
            resume_thread.argtypes = (wintypes.HANDLE,)
            resume_thread.restype = wintypes.DWORD
            previous_suspend_count = resume_thread(wintypes.HANDLE(launcher_thread_handle))
            if previous_suspend_count == 0xffffffff:
                raise ctypes.WinError(ctypes.get_last_error())
            record({"event": "launcher_resumed", "pid": launcher_pid,
                    "previous_suspend_count": previous_suspend_count,
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            launch_deadline = time.monotonic() + HOST_TIMEOUT_SECONDS
            while (time.monotonic() < launch_deadline and not child_ready.is_set()
                   and not launcher_detached.is_set()):
                time.sleep(0.1)
            if not child_ready.is_set():
                outcome = "launcher_detached_before_child" if launcher_detached.is_set() else "child_attach_timeout"
                record({"event": "capture_host_end", "outcome": outcome,
                        "done_agents": [], "host_utc": datetime.now(timezone.utc).isoformat()})
                print(f"Capture ended: {outcome}; output={output_path}", flush=True)
                return 2

            child_pid = child_info["pid"]
            child_session = device.attach(child_pid)
            child_session.on("detached", on_detached("race.bin"))
        for name, path in agents.items():
            source = path.read_text(encoding="utf-8")
            if name == "startup_thread_sampler":
                source = source.replace("__TARGET_TID__", str(child_info.get("tid", 0)))
            script = child_session.create_script(source)
            script.on("message", on_message(name))
            script.load()
            child_scripts.append(script)
        if args.process_silent_audio and not audio_silencer_ready.is_set():
            raise RuntimeError(
                "DirectSound process silencer did not report ready before engine resume")
        record({"event": "capture_agents_loaded", "pid": child_pid,
                "process_silent_audio": args.process_silent_audio,
                "host_utc": datetime.now(timezone.utc).isoformat()})
        if args.direct_engine:
            device.resume(child_pid)
            record({"event": "direct_engine_resumed", "pid": child_pid,
                    "cwd": str(expected_cwd),
                    "host_utc": datetime.now(timezone.utc).isoformat()})
        else:
            resume_record = resume_thread_until_running(child_info["tid"])
            record({"event": "launcher_child_resumed", "pid": child_pid,
                    **resume_record,
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            print(f"Resumed launcher-created engine thread {child_info['tid']} after observer installation; prior suspend count={resume_record['previous_suspend_count']}.", flush=True)
        print(f"Capture running via {'direct engine' if args.direct_engine else 'isolated launcher'}; race.bin pid={child_pid}", flush=True)
        if args.startup_only:
            print(f"Startup-only observation: no synthetic input; observers={','.join(agents)}; host records the process exit status and stops the target after {wait_seconds} seconds if it remains alive.", flush=True)
        elif args.process_silent_audio:
            print("Select Quick Race > Bemidji > RACE; DirectSound buffers are clamped to minimum volume in race.bin and CD audio is disabled. Other output APIs are not suppressed.", flush=True)
        else:
            print("Select Quick Race > Bemidji > RACE; effects/music volumes are zero, CD audio is disabled, and the host closes launched processes at completion or timeout.", flush=True)
        deadline = time.monotonic() + wait_seconds
        while (time.monotonic() < deadline and not finished.is_set()
               and not child_detached.is_set()):
            time.sleep(0.2)
        outcome = "both_agents_completed" if finished.is_set() else (
            "target_detached" if child_detached.is_set() else
            "startup_window_elapsed" if args.startup_only else "host_timeout")
        record({"event": "capture_host_end", "outcome": outcome,
                "done_agents": sorted(done_agents),
                "child_active_at_capture_end": not child_detached.is_set(),
                "host_utc": datetime.now(timezone.utc).isoformat()})
        print(f"Capture ended: {outcome}; output={output_path}", flush=True)
        return 0 if finished.is_set() else 2
    finally:
        for process_id in (child_pid, launcher_pid):
            if process_id is None:
                continue
            status = query_process_exit_status(process_id)
            still_active = status.get("process_still_active") is True
            if still_active:
                shutdown_requested_processes.add(process_id)
            stopped = (terminate_owned_process(process_id) if still_active else True)
            record({"event": "capture_process_cleanup", "pid": process_id,
                    "was_active": still_active, "termination_requested": still_active,
                    "termination_succeeded": stopped, **status,
                    "host_utc": datetime.now(timezone.utc).isoformat()})
            if still_active:
                print(f"Stopped capture process pid={process_id}; terminated={stopped}", flush=True)
        cleanup_capture_instrumentation(
            [child_session, launcher_session], child_scripts + launcher_scripts)
        if launcher_handles is not None:
            kernel32, process_handle, thread_handle = launcher_handles
            for handle in (thread_handle, process_handle):
                try:
                    kernel32.CloseHandle(wintypes.HANDLE(handle))
                except Exception:
                    pass
        if canary_kernel32 is not None and canary_handle is not None:
            try:
                canary_kernel32.CloseHandle(ctypes.c_void_p(canary_handle))
            except Exception:
                pass


if __name__ == "__main__":
    raise SystemExit(main())
