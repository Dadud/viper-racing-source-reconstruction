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

function Send-RaceKey([IntPtr]$Window, [int]$Key, [string]$Name) {
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($Window, 0x0100, [IntPtr]$Key, [IntPtr]0)) {
        throw "Could not post $Name key-down"
    }
    Start-Sleep -Milliseconds 180
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($Window, 0x0101, [IntPtr]$Key, [IntPtr]0)) {
        throw "Could not post $Name key-up"
    }
    Start-Sleep -Milliseconds 180
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
    # A lost focus must release held gameplay controls even if Windows never
    # delivers the corresponding key-up to this window.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Could not post focus loss'
    }
    Start-Sleep -Milliseconds 250
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]0x26, [IntPtr]0)) {
        throw 'Could not post throttle key-up'
    }
    Start-Sleep -Milliseconds 300
    # These keys come from the shipped options.def. The runtime must expose
    # the reconstructed callback outputs even while their physical/camera/
    # transmission effects remain incomplete.
    Send-RaceKey $window 69 'e-brake'       # Key_E
    Send-RaceKey $window 82 'reverse'       # Key_R
    Send-RaceKey $window 67 'clutch'        # Key_C
    Send-RaceKey $window 32 'airlift'       # Key_SPACE
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]27, [IntPtr]0)) {
        throw 'Could not close the race window'
    }
    if (-not $process.WaitForExit(15000)) { throw 'Race window did not shut down after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "viper_game exited with code $($process.ExitCode)" }
    if (-not (Test-Path -LiteralPath $trace)) { throw 'Runtime did not create the input trace' }

    $rows = [System.IO.File]::ReadAllLines($trace)
    if ($rows.Count -lt 3) { throw 'Input trace did not capture enough race frames' }
    $header = $rows[0].Split(',')
    $nativeThrottleColumn = [Array]::IndexOf($header, 'control_0x5093f4')
    if ($nativeThrottleColumn -lt 0) { throw 'Trace omitted the native throttle control output' }
    $actionColumns = @{
        eBrake = [Array]::IndexOf($header, 'control_0x5093b8')
        reverse = [Array]::IndexOf($header, 'control_0x5093f0')
        clutch = [Array]::IndexOf($header, 'control_0x5093c8')
        airlift = [Array]::IndexOf($header, 'control_0x509360')
    }
    foreach ($entry in $actionColumns.GetEnumerator()) {
        if ($entry.Value -lt 0) { throw "Trace omitted the $($entry.Key) action output" }
    }
    $throttle = @($rows | Select-Object -Skip 1 | ForEach-Object { $_.Split(',')[5] })
    $nativeThrottle = @($rows | Select-Object -Skip 1 | ForEach-Object {
        [single]::Parse($_.Split(',')[$nativeThrottleColumn], [cultureinfo]::InvariantCulture)
    })
    $pressedIndex = -1
    for ($index = 0; $index -lt $throttle.Count; ++$index) {
        if ($throttle[$index] -eq '1') { $pressedIndex = $index; break }
    }
    if ($pressedIndex -lt 0) { throw 'The live trace never recorded throttle while Up was held' }
    if (-not ($nativeThrottle | Where-Object { $_ -gt 0.0 })) {
        throw 'The 0x43B290 control update did not raise its native throttle output'
    }
    $releasedAfterPress = $false
    for ($index = $pressedIndex + 1; $index -lt $throttle.Count; ++$index) {
        if ($throttle[$index] -eq '0') { $releasedAfterPress = $true; break }
    }
    if (-not $releasedAfterPress) { throw 'The live trace never recorded throttle release' }
    $nativeReleased = $false
    for ($index = $pressedIndex + 1; $index -lt $nativeThrottle.Count; ++$index) {
        if ($nativeThrottle[$index] -eq 0.0) { $nativeReleased = $true; break }
    }
    if (-not $nativeReleased) { throw 'The native throttle response did not decay to zero after focus loss' }
    foreach ($action in $actionColumns.GetEnumerator()) {
        $observed = $false
        foreach ($row in $rows | Select-Object -Skip 1) {
            $value = [single]::Parse($row.Split(',')[$action.Value], [cultureinfo]::InvariantCulture)
            if ($value -gt 0.0) { $observed = $true; break }
        }
        if (-not $observed) { throw "The shipped keyboard binding never reached the traced $($action.Key) output" }
    }
    Write-Output 'VK_UP and the shipped e-brake, reverse, clutch, and airlift keys reached their reconstructed 0x43B290 trace outputs; throttle decayed after focus loss.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $trace -Force -ErrorAction SilentlyContinue
}
