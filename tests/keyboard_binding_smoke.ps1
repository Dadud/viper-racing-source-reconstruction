param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$trace = Join-Path ([System.IO.Path]::GetTempPath()) "viper_keyboard_$([guid]::NewGuid().ToString('N')).csv"

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public delegate bool ViperKeyboardEnumWindowsProc(IntPtr hwnd, IntPtr lParam);
public static class ViperKeyboardSmokeWin32 {
    [DllImport("user32.dll")] public static extern bool EnumWindows(ViperKeyboardEnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Find-RaceWindow([int]$ProcessId) {
    $found = [System.Collections.Generic.List[IntPtr]]::new()
    $callback = [ViperKeyboardEnumWindowsProc] {
        param([IntPtr]$Window, [IntPtr]$Unused)
        [uint32]$owner = 0
        [void][ViperKeyboardSmokeWin32]::GetWindowThreadProcessId($Window, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $name = [System.Text.StringBuilder]::new(128)
            [void][ViperKeyboardSmokeWin32]::GetClassName($Window, $name, $name.Capacity)
            if ($name.ToString() -eq 'ViperRacingRuntimeWindow') { [void]$found.Add($Window) }
        }
        return $true
    }
    [void][ViperKeyboardSmokeWin32]::EnumWindows($callback, [IntPtr]::Zero)
    if ($found.Count -gt 0) { return $found[0] }
    return [IntPtr]::Zero
}

$process = $null
try {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --track bemidji --car viper --software --record-trace `"$trace`""
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)

    $window = [IntPtr]::Zero
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 30 -and $window -eq [IntPtr]::Zero) {
        $process.Refresh()
        if ($process.HasExited) { throw "viper_game exited early with code $($process.ExitCode)" }
        $window = Find-RaceWindow $process.Id
        if ($window -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 100 }
    }
    if ($window -eq [IntPtr]::Zero) { throw 'Timed out waiting for the race window' }

    # The shipped binding maps throttle to Key_UP. Posted key events exercise
    # the VK-to-DIK adapter and the live evaluator; the trace records consumed
    # runtime input values, not the posted messages themselves.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]0x26, [IntPtr]0)) {
        throw 'Could not post throttle key-down'
    }
    Start-Sleep -Milliseconds 600
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]0x26, [IntPtr]0)) {
        throw 'Could not post throttle key-up'
    }
    Start-Sleep -Milliseconds 300
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]27, [IntPtr]0)) {
        throw 'Could not close the race window'
    }
    if (-not $process.WaitForExit(15000)) { throw 'Race window did not shut down after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "viper_game exited with code $($process.ExitCode)" }
    if (-not (Test-Path -LiteralPath $trace)) { throw 'Runtime did not create the input trace' }

    $rows = [System.IO.File]::ReadAllLines($trace)
    if ($rows.Count -lt 3) { throw 'Input trace did not capture enough race frames' }
    $throttle = @($rows | Select-Object -Skip 1 | ForEach-Object { $_.Split(',')[5] })
    $pressedIndex = -1
    for ($index = 0; $index -lt $throttle.Count; ++$index) {
        if ($throttle[$index] -eq '1') { $pressedIndex = $index; break }
    }
    if ($pressedIndex -lt 0) { throw 'The live trace never recorded throttle while Up was held' }
    $releasedAfterPress = $false
    for ($index = $pressedIndex + 1; $index -lt $throttle.Count; ++$index) {
        if ($throttle[$index] -eq '0') { $releasedAfterPress = $true; break }
    }
    if (-not $releasedAfterPress) { throw 'The live trace never recorded throttle release' }
    Write-Output 'VK_UP reached the live DIK binding evaluator as throttle and cleared on key-up.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $trace -Force -ErrorAction SilentlyContinue
}
