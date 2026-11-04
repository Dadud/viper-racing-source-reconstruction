from __future__ import annotations

import runpy
import os
import subprocess
import sys
import tempfile
from pathlib import Path


runner = Path(__file__).resolve().parents[1] / "tools" / "run_original_pcar_capture.py"
runner_functions = runpy.run_path(str(runner))
select_launcher_paths = runner_functions["select_launcher_paths"]
validate_disc_volume_label = runner_functions["validate_disc_volume_label"]
required_capture_sound_settings = runner_functions["required_capture_sound_settings"]
canary_semaphore_strategy = runner_functions["canary_semaphore_strategy"]
capture_agent_names = runner_functions["capture_agent_names"]
query_process_exit_status = runner_functions["query_process_exit_status"]
terminate_owned_process = runner_functions["terminate_owned_process"]
resume_thread_until_running = runner_functions["resume_thread_until_running"]
resolve_launcher_arguments = runner_functions["resolve_launcher_arguments"]

with tempfile.TemporaryDirectory(prefix="viper_capture_paths_") as temporary:
    root = Path(temporary) / "GameRoot"
    data = root / "Data"
    data.mkdir(parents=True)
    staged_launcher = root / "Viper Racing.exe"
    staged_launcher.write_bytes(b"verified-launcher-fixture")

    launcher, working_directory = select_launcher_paths(root)
    assert launcher == staged_launcher.resolve()
    assert working_directory == data.resolve()

    explicit_launcher = Path(temporary) / "alternate" / "Viper Racing.exe"
    explicit_launcher.parent.mkdir()
    explicit_launcher.write_bytes(b"explicit-launcher-fixture")
    selected, explicit_working_directory = select_launcher_paths(root, explicit_launcher)
    assert selected == explicit_launcher.resolve()
    assert explicit_working_directory == data.resolve()

assert validate_disc_volume_label("F:", "VIPER") == "VIPER"
try:
    validate_disc_volume_label("F:", "")
except SystemExit as error:
    assert "Expected the original VIPER disc" in str(error)
else:
    raise AssertionError("capture runner accepted an unmounted or wrong-label disc")

assert required_capture_sound_settings() == {
    "enabled": "yes",
    "effects_volume": "0.000000",
    "music_volume": "0.000000",
    "cd_audio": "no",
}
assert required_capture_sound_settings(process_silent_audio=True) == {
    "enabled": "yes",
    "effects_volume": "1.000000",
    "music_volume": "1.000000",
    "cd_audio": "no",
}
assert canary_semaphore_strategy(False) == "launcher_creates_for_child"
assert canary_semaphore_strategy(True) == "host_holds_for_direct_engine"
assert resolve_launcher_arguments(None) == ("-1",)
assert resolve_launcher_arguments(["-safe"]) == ("-safe",)
assert capture_agent_names(True) == ("startup_vram", "exit_status")
assert capture_agent_names(True, include_source_watch=True) == (
    "startup_vram", "exit_status", "pcar_source_watch")
assert capture_agent_names(True, include_controlled_capture=True) == (
    "startup_vram", "exit_status", "controlled_wheel_capture")
assert capture_agent_names(True, include_source_watch=True,
                           include_controlled_capture=True) == (
    "startup_vram", "exit_status", "pcar_source_watch",
    "controlled_wheel_capture")
assert capture_agent_names(True, include_graphics_trace=True) == (
    "startup_vram", "exit_status", "graphics_startup_trace")
assert capture_agent_names(True, include_thread_sample=True) == (
    "startup_vram", "exit_status", "startup_thread_sampler")
assert capture_agent_names(True, include_graphics_trace=True,
                           include_thread_sample=True) == (
    "startup_vram", "exit_status", "graphics_startup_trace",
    "startup_thread_sampler")
assert capture_agent_names(False) == (
    "startup_vram", "exit_status", "pcar_source_watch",
    "controlled_wheel_capture")
if os.name == "nt":
    import ctypes

    current_tid = ctypes.WinDLL("kernel32").GetCurrentThreadId()
    resume_result = resume_thread_until_running(current_tid)
    assert resume_result["already_running"] is True
    assert resume_result["previous_suspend_count"] == 0
    own_status = query_process_exit_status(os.getpid())
    assert own_status["process_still_active"] is True
    assert own_status["process_exit_code"] == 259
    assert terminate_owned_process(0xFFFFFFFF) is True
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    try:
        assert terminate_owned_process(child.pid) is True
        child.wait(timeout=5)
        assert child.returncode is not None
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()

print("capture path, audio, semaphore ownership, and startup-only agent selection passed")
