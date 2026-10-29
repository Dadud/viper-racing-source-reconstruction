param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$trace = Join-Path ([System.IO.Path]::GetTempPath()) "viper_keyboard_$([guid]::NewGuid().ToString('N')).csv"
$configDir = Join-Path ([System.IO.Path]::GetTempPath()) "viper_keyboard_config_$([guid]::NewGuid().ToString('N'))"

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
    [void](New-Item -ItemType Directory -Path $configDir)
    $setupPreference = Join-Path $configDir 'viper-racing-setup.cfg'
    [System.IO.File]::WriteAllText($setupPreference,
        "VIPER-RACING-SETUP 4`ntrack=bemidji`ncar=viper`nlaps=2`nopponent_strength=0`nsteer_left=Key_A`nsteer_right=Key_D`nthrottle=Key_W`nbraking=Key_S`n")
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    # Omit explicit track/car flags so startup loads the frontend selection
    # and its Config-owned keyboard overrides before entering the race.
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$configDir`" --software --diagnostics-only --record-trace `"$trace`""
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

    # The temporary v4 preference maps throttle to W and braking to S. Posted key events exercise
    # the VK-to-DIK adapter and the live evaluator; the trace records consumed
    # runtime input values, not the posted messages themselves.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]87, [IntPtr]0)) {
        throw 'Could not post throttle key-down'
    }
    Start-Sleep -Milliseconds 600
    # A lost focus must release held gameplay controls even if Windows never
    # delivers the corresponding key-up to this window.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0008, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Could not post focus loss'
    }
    Start-Sleep -Milliseconds 250
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]87, [IntPtr]0)) {
        throw 'Could not post throttle key-up'
    }
    Start-Sleep -Milliseconds 300
    # Exercise the remapped brake input; do not infer vehicle motion from the
    # route prototype.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]83, [IntPtr]0)) {
        throw 'Could not post brake key-down'
    }
    Start-Sleep -Milliseconds 700
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]83, [IntPtr]0)) {
        throw 'Could not post brake key-up'
    }
    Start-Sleep -Milliseconds 150
    # These keys come from the shipped options.def. Check that their recovered
    # callback outputs are consumed, without asserting route-following motion.
    Send-RaceKey $window 69 'e-brake'       # Key_E
    Send-RaceKey $window 82 'reverse'       # Key_R
    # Hold Reverse long enough for the input trace to observe the action.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]82, [IntPtr]0)) {
        throw 'Could not post reverse key-down'
    }
    Start-Sleep -Milliseconds 800
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]82, [IntPtr]0)) {
        throw 'Could not post reverse key-up'
    }
    Start-Sleep -Milliseconds 150
    Send-RaceKey $window 67 'clutch'        # Key_C
    Send-RaceKey $window 32 'airlift'       # Key_SPACE
    # Pause while throttle is held. Entering pause must clear the held input
    # and freeze simulation time and fixed-step dispatch until P resumes.
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]0x26, [IntPtr]0)) {
        throw 'Could not post throttle key-down for the pause check'
    }
    Start-Sleep -Milliseconds 400
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]80, [IntPtr]0)) {
        throw 'Could not pause the race'
    }
    Start-Sleep -Milliseconds 650
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]0x26, [IntPtr]0)) {
        throw 'Could not release throttle while paused'
    }
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]80, [IntPtr]0)) {
        throw 'Could not release the pause key'
    }
    Start-Sleep -Milliseconds 120
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0100, [IntPtr]80, [IntPtr]0)) {
        throw 'Could not resume the race'
    }
    Start-Sleep -Milliseconds 450
    if (-not [ViperKeyboardSmokeWin32]::PostMessage($window, 0x0101, [IntPtr]80, [IntPtr]0)) {
        throw 'Could not release the resume key'
    }
    Start-Sleep -Milliseconds 120
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
    $diagnosticsOnlyColumn = [Array]::IndexOf($header, 'diagnostics_only')
    $routeMotionStepColumn = [Array]::IndexOf($header, 'route_motion_step_count')
    if ($diagnosticsOnlyColumn -lt 0 -or $routeMotionStepColumn -lt 0) {
        throw 'Trace omitted diagnostics-only route-motion markers'
    }
    foreach ($line in $rows | Select-Object -Skip 1) {
        $fields = $line.Split(',')
        if ($fields[$diagnosticsOnlyColumn] -ne '1' -or
            [uint32]::Parse($fields[$routeMotionStepColumn],
                [cultureinfo]::InvariantCulture) -ne 0) {
            throw 'Keyboard smoke executed the provisional route-motion adapter'
        }
    }
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
    if ($pressedIndex -lt 0) { throw 'The live trace never recorded throttle while remapped W was held' }
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
    $parsedRows = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $rows | Select-Object -Skip 1) {
        [void]$parsedRows.Add($line.Split(','))
    }
    $brakeRows = @($parsedRows | Where-Object { $_[6] -eq '1' })
    if ($brakeRows.Count -eq 0) { throw 'Remapped S never reached the runtime as braking input' }
    foreach ($action in $actionColumns.GetEnumerator()) {
        $observed = $false
        foreach ($row in $rows | Select-Object -Skip 1) {
            $value = [single]::Parse($row.Split(',')[$action.Value], [cultureinfo]::InvariantCulture)
            if ($value -gt 0.0) { $observed = $true; break }
        }
        if (-not $observed) { throw "The shipped keyboard binding never reached the traced $($action.Key) output" }
    }

    $pausedColumn = [Array]::IndexOf($header, 'paused')
    $elapsedColumn = [Array]::IndexOf($header, 'elapsed_seconds')
    $stepColumn = [Array]::IndexOf($header, 'physics_step_count')
    $callbackColumn = [Array]::IndexOf($header, 'pcar_callback_step_count')
    $dispatchColumn = [Array]::IndexOf($header, 'pcar_task_projection_dispatch_steps')
    if (@($pausedColumn, $elapsedColumn, $stepColumn, $callbackColumn, $dispatchColumn) -contains -1) {
        throw 'Runtime trace omitted fields needed to verify pause/resume'
    }
    $traceRows = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $rows | Select-Object -Skip 1) {
        [void]$traceRows.Add($line.Split(','))
    }
    $pausedRows = @($traceRows | Where-Object { $_[$pausedColumn] -eq '1' })
    if ($pausedRows.Count -lt 3) { throw 'Trace did not capture a sustained paused interval' }
    $frozenColumns = @($elapsedColumn, $stepColumn, $callbackColumn, $dispatchColumn)
    foreach ($pausedRow in $pausedRows) {
        foreach ($column in $frozenColumns) {
            if ($pausedRow[$column] -ne $pausedRows[0][$column]) {
                throw "Race state column '$($header[$column])' changed while paused"
            }
        }
        if ($pausedRow[5] -eq '1') { throw 'Throttle remained active after pause cleared held controls' }
    }
    $lastPausedIndex = -1
    for ($index = 0; $index -lt $traceRows.Count; ++$index) {
        if ($traceRows[$index][$pausedColumn] -eq '1') { $lastPausedIndex = $index }
    }
    $resumedSteps = $false
    for ($index = $lastPausedIndex + 1; $index -lt $traceRows.Count; ++$index) {
        if ($traceRows[$index][$pausedColumn] -eq '0' -and
            [uint32]::Parse($traceRows[$index][$stepColumn]) -gt
                [uint32]::Parse($pausedRows[0][$stepColumn])) {
            $resumedSteps = $true
            break
        }
    }
    if (-not $resumedSteps) { throw 'Fixed-step simulation did not resume after P was pressed again' }

    Write-Output 'Configured control signals, focus-loss release, and P pause/resume passed in diagnostics-only mode; ideal-line route motion was disabled. This verifies input plumbing only, not physics or driving.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $trace -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $configDir) {
        Remove-Item -LiteralPath $configDir -Recurse -Force
    }
}
