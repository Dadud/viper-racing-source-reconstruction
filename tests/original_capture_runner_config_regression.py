from __future__ import annotations

import runpy
import tempfile
from pathlib import Path


runner = Path(__file__).resolve().parents[1] / "tools" / "run_original_pcar_capture.py"
runner_functions = runpy.run_path(str(runner))
select_launcher_paths = runner_functions["select_launcher_paths"]
validate_disc_volume_label = runner_functions["validate_disc_volume_label"]
required_capture_sound_settings = runner_functions["required_capture_sound_settings"]
canary_semaphore_strategy = runner_functions["canary_semaphore_strategy"]

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

print("capture launcher path, audio profiles, and canary ownership passed")
