param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$config = Join-Path ([System.IO.Path]::GetTempPath()) "viper_session_lifecycle_$([guid]::NewGuid().ToString('N'))"
$preferencePath = Join-Path $config 'viper-racing-setup.cfg'

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public delegate bool ViperSessionEnumWindowsProc(IntPtr hwnd, IntPtr lParam);
public static class ViperSessionWin32 {
    [DllImport("user32.dll")] public static extern bool EnumWindows(ViperSessionEnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
}
"@

function Find-ProcessWindow([int]$ProcessId, [string]$ClassName) {
    $found = [System.Collections.Generic.List[IntPtr]]::new()
    $callback = [ViperSessionEnumWindowsProc] {
        param([IntPtr]$Window, [IntPtr]$Unused)
        [uint32]$owner = 0
        [void][ViperSessionWin32]::GetWindowThreadProcessId($Window, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $name = [System.Text.StringBuilder]::new(128)
            [void][ViperSessionWin32]::GetClassName($Window, $name, $name.Capacity)
            if ($name.ToString() -eq $ClassName) { [void]$found.Add($Window) }
        }
        return $true
    }
    [void][ViperSessionWin32]::EnumWindows($callback, [IntPtr]::Zero)
    if ($found.Count -gt 0) { return $found[0] }
    return [IntPtr]::Zero
}

function Wait-ProcessWindow([Diagnostics.Process]$Process, [string]$ClassName, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $Process.Refresh()
        if ($Process.HasExited) { throw "viper_game exited early with code $($Process.ExitCode)" }
        $window = Find-ProcessWindow $Process.Id $ClassName
        if ($window -ne [IntPtr]::Zero) { return $window }
        Start-Sleep -Milliseconds 50
    }
    throw "Timed out waiting for window class $ClassName"
}

function Send-Key([IntPtr]$Window, [int]$Key) {
    if (-not [ViperSessionWin32]::PostMessage($Window, 0x0100, [IntPtr]$Key, [IntPtr]::Zero)) {
        throw "Could not post key $Key"
    }
}

function Start-Session([string]$ConfigurationDirectory) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$ConfigurationDirectory`" --software"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    return [Diagnostics.Process]::Start($startInfo)
}

function Read-RaceTitle([IntPtr]$Window) {
    $title = [System.Text.StringBuilder]::new(256)
    [void][ViperSessionWin32]::GetWindowText($Window, $title, $title.Capacity)
    return $title.ToString()
}

function Wait-RaceTitle([Diagnostics.Process]$Process, [IntPtr]$Window, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $Process.Refresh()
        if ($Process.HasExited) { throw "viper_game exited before publishing the race title (code $($Process.ExitCode))" }
        $title = Read-RaceTitle $Window
        if ($title -match 'bemidji.*viper' -and $title -match 'Lap 1/2') {
            return $title
        }
        Start-Sleep -Milliseconds 50
    }
    return (Read-RaceTitle $Window)
}

$process = $null
try {
    [void](New-Item -ItemType Directory -Path $config)
    # Start at default Bemidji/Exotic setup, then select Viper and two laps
    # through the frontend. This checks session startup and persistence only;
    # it does not drive a lap or claim results behavior.
    [System.IO.File]::WriteAllText($preferencePath,
        "VIPER-RACING-SETUP 4`ntrack=bemidji`ncar=exotic`nlaps=1`nopponent_strength=0`n")
    $process = Start-Session $config
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30

    Send-Key $setup 75 # K: controls page
    Send-Key $setup 40
    Send-Key $setup 40 # throttle row
    Send-Key $setup 13 # capture binding
    Send-Key $setup 87 # W
    Send-Key $setup 27 # return to setup
    Start-Sleep -Milliseconds 150
    1..4 | ForEach-Object { Send-Key $setup 39 }
    Send-Key $setup 33 # PageUp: two laps
    Send-Key $setup 13 # start selected session

    $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    $title = Wait-RaceTitle $process $race 5
    if ($title -notmatch 'bemidji.*viper' -or $title -notmatch 'Lap 1/2') {
        throw "Frontend selection did not start the expected Viper/Bemidji session: $title"
    }
    Send-Key $race 27
    if (-not $process.WaitForExit(15000)) { throw 'Race window did not shut down after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "viper_game exited with code $($process.ExitCode)" }

    $saved = [System.IO.File]::ReadAllText($preferencePath)
    if ($saved -notmatch '(?m)^track=bemidji$' -or
        $saved -notmatch '(?m)^car=viper$' -or
        $saved -notmatch '(?m)^laps=2$' -or
        $saved -notmatch '(?m)^throttle=Key_W$') {
        throw "Frontend selection or control binding did not persist: $saved"
    }

    # Relaunch from the saved selection and verify setup enters the same session.
    $process = Start-Session $config
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    Send-Key $setup 13
    $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    $title = Wait-RaceTitle $process $race 5
    if ($title -notmatch 'bemidji.*viper' -or $title -notmatch 'Lap 1/2') {
        throw "Relaunch did not restore the saved Viper/Bemidji setup: $title"
    }
    Send-Key $race 27
    if (-not $process.WaitForExit(15000)) { throw 'Relaunched race window did not shut down after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Relaunched viper_game exited with code $($process.ExitCode)" }

    Write-Output 'Frontend selection, control remapping, saved setup reload, and race-window startup/shutdown passed. No lap completion or results behavior is asserted.'
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $config -Recurse -Force -ErrorAction SilentlyContinue
}
