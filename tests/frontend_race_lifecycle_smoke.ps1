param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_frontend_race_$([guid]::NewGuid().ToString('N'))"
$capture = Join-Path $root 'capture.csv'
$raceTrace = Join-Path $root 'two-lap-full-throttle.csv'
$config = Join-Path $root 'Config'

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public delegate bool ViperRaceEnumWindowsProc(IntPtr hwnd, IntPtr lParam);
public static class ViperRaceFrontendWin32 {
    [DllImport("user32.dll")] public static extern bool EnumWindows(ViperRaceEnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Find-ProcessWindow([int]$ProcessId, [string]$ClassName) {
    $found = [System.Collections.Generic.List[IntPtr]]::new()
    $callback = [ViperRaceEnumWindowsProc] {
        param([IntPtr]$Window, [IntPtr]$Unused)
        [uint32]$owner = 0
        [void][ViperRaceFrontendWin32]::GetWindowThreadProcessId($Window, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $name = [System.Text.StringBuilder]::new(128)
            [void][ViperRaceFrontendWin32]::GetClassName($Window, $name, $name.Capacity)
            if ($name.ToString() -eq $ClassName) { [void]$found.Add($Window) }
        }
        return $true
    }
    [void][ViperRaceFrontendWin32]::EnumWindows($callback, [IntPtr]::Zero)
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

function Wait-ResultsReturn([Diagnostics.Process]$Process, [int]$TimeoutSeconds) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $Process.Refresh()
        if ($Process.HasExited) { throw "viper_game exited before returning to setup (code $($Process.ExitCode))" }
        $setup = Find-ProcessWindow $Process.Id 'ViperRaceSetupFrontendWindow'
        if ($setup -ne [IntPtr]::Zero) { return $setup }
        $race = Find-ProcessWindow $Process.Id 'ViperRacingRuntimeWindow'
        if ($race -ne [IntPtr]::Zero) {
            [void][ViperRaceFrontendWin32]::PostMessage($race, 0x0100, [IntPtr]13, [IntPtr]::Zero)
        }
        Start-Sleep -Milliseconds 50
    }
    throw 'Timed out waiting for the full race trace to reach results and return to setup'
}

function Press-Key([IntPtr]$Window, [int]$Key) {
    if (-not [ViperRaceFrontendWin32]::PostMessage($Window, 0x0100, [IntPtr]$Key, [IntPtr]::Zero)) {
        throw "Could not post key $Key to the requested Viper Racing window"
    }
}

$process = $null
New-Item -ItemType Directory -Path $root | Out-Null
try {
    # Capture the app's own trace schema, then build the same full-throttle
    # one-lap Viper/Bemidji input used by the established race regression.
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --smoke-frames 12 --record-trace $capture
    if ($LASTEXITCODE -ne 0) { throw "Trace schema capture exited with $LASTEXITCODE" }
    $captureLines = [System.IO.File]::ReadAllLines($capture)
    if ($captureLines.Count -lt 2) { throw 'Trace schema capture has no data rows' }
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add($captureLines[0])
    $template = $captureLines[1].Split(',')
    # Dundas' ideal line is longer than Bemidji's, so budget enough frames for
    # all entrants to finish the same two-lap scripted race on either track.
    for ($frame = 0; $frame -lt 28800; ++$frame) {
        $fields = [string[]]$template.Clone()
        $fields[0] = [string]$frame
        $fields[1] = '0'
        $fields[2] = '0.05'
        $fields[3] = '0'
        $fields[4] = '0'
        $fields[5] = '1'
        $fields[6] = '0'
        $lines.Add(($fields -join ','))
    }
    [System.IO.File]::WriteAllLines($raceTrace, $lines)

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$config`" --software --play-trace `"$raceTrace`""
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)

    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    # The shipped options.def begins at car_no 0 (Exotic), and Win32 bundle
    # enumeration orders the five installed choices with Viper at index 4.
    1..4 | ForEach-Object { Press-Key $setup 39 }
    Press-Key $setup 33
    Press-Key $setup 13
    $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    $title = [System.Text.StringBuilder]::new(256)
    for ($attempt = 0; $attempt -lt 20 -and $title.Length -eq 0; ++$attempt) {
        [void][ViperRaceFrontendWin32]::GetWindowText($race, $title, $title.Capacity)
        if ($title.Length -eq 0) { Start-Sleep -Milliseconds 50 }
    }
    if ($title.ToString() -notmatch 'bemidji.*viper' -or
        $title.ToString() -notmatch 'Lap 1/2') {
        throw "Setup selection did not start the expected two-lap Viper/Bemidji race: $($title.ToString())"
    }

    $setup = Wait-ResultsReturn $process 60
    Write-Output 'Viper/Bemidji returned to setup.'
    Press-Key $setup 37
    Press-Key $setup 13
    $secondRace = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 30
    $secondTitle = [System.Text.StringBuilder]::new(256)
    for ($attempt = 0; $attempt -lt 20 -and $secondTitle.Length -eq 0; ++$attempt) {
        [void][ViperRaceFrontendWin32]::GetWindowText($secondRace, $secondTitle, $secondTitle.Capacity)
        if ($secondTitle.Length -eq 0) { Start-Sleep -Milliseconds 50 }
    }
    if ($secondTitle.ToString() -notmatch 'bemidji.*sports') {
        throw "Returning to setup and changing to Sports did not produce the expected race window: $($secondTitle.ToString())"
    }
    $setup = Wait-ResultsReturn $process 60
    Write-Output 'Sports/Bemidji returned to setup.'
    # Bemidji is the final installed track in tracks.tab order; Down wraps to Dundas.
    Press-Key $setup 40
    Press-Key $setup 13
    $thirdRace = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 30
    $thirdTitle = [System.Text.StringBuilder]::new(256)
    for ($attempt = 0; $attempt -lt 20 -and $thirdTitle.Length -eq 0; ++$attempt) {
        [void][ViperRaceFrontendWin32]::GetWindowText($thirdRace, $thirdTitle, $thirdTitle.Capacity)
        if ($thirdTitle.Length -eq 0) { Start-Sleep -Milliseconds 50 }
    }
    if ($thirdTitle.ToString() -notmatch 'dundas.*sports' -or
        $thirdTitle.ToString() -notmatch 'Lap 1/2') {
        throw "Frontend did not start the expected two-lap Dundas/Sports race: $($thirdTitle.ToString())"
    }
    $setup = Wait-ResultsReturn $process 60
    Write-Output 'Sports/Dundas returned to setup.'
    Press-Key $setup 27
    if (-not $process.WaitForExit(15000)) { throw 'Viper Racing did not exit after Escape from the returned setup window' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "viper_game exited with code $($process.ExitCode)" }

    $preferencePath = Join-Path $config 'viper-racing-setup.cfg'
    if (-not (Test-Path -LiteralPath $preferencePath)) { throw 'Setup selection preference was not written' }
    $preference = [System.IO.File]::ReadAllText($preferencePath)
    if ($preference -notmatch '(?m)^track=dundas$' -or
        $preference -notmatch '(?m)^car=sports$' -or
        $preference -notmatch '(?m)^laps=2$') {
        throw "Saved selection did not contain Dundas/Sports/two laps: $preference"
    }

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$config`" --software --play-trace `"$raceTrace`""
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    Press-Key $setup 13
    $restoredRace = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 30
    $restoredTitle = [System.Text.StringBuilder]::new(256)
    for ($attempt = 0; $attempt -lt 20 -and $restoredTitle.Length -eq 0; ++$attempt) {
        [void][ViperRaceFrontendWin32]::GetWindowText($restoredRace, $restoredTitle, $restoredTitle.Capacity)
        if ($restoredTitle.Length -eq 0) { Start-Sleep -Milliseconds 50 }
    }
    if ($restoredTitle.ToString() -notmatch 'dundas.*sports' -or
        $restoredTitle.ToString() -notmatch 'Lap 1/2') {
        throw "Relaunched setup did not restore Dundas/Sports with two laps: $($restoredTitle.ToString())"
    }
    Press-Key $restoredRace 27
    if (-not $process.WaitForExit(15000)) { throw 'Viper Racing did not exit after the persisted-selection check' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "relaunch exited with code $($process.ExitCode)" }
    Write-Output 'Viper/Bemidji, Sports/Bemidji, and Sports/Dundas trace races reached results; track, car, and lap count persisted across a clean relaunch.'

    # Exercise setup-to-results lifecycle on every other shipped track. Seed
    # each isolated Config with the selected track so this does not depend on
    # undocumented tracks.tab ordering or selector wrap counts.
    foreach ($track in @('hastings', 'heaven', 'kenyon', 'limbo', 'nfield', 'uptown')) {
        $trackConfig = Join-Path $root "Config-$track"
        New-Item -ItemType Directory -Path $trackConfig | Out-Null
        $selection = "VIPER-RACING-SETUP 2`ntrack=$track`ncar=sports`nlaps=2`n"
        $preferencePath = Join-Path $trackConfig 'viper-racing-setup.cfg'
        [System.IO.File]::WriteAllText($preferencePath, $selection,
            [System.Text.Encoding]::ASCII)

        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $GameExe
        $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$trackConfig`" --software --play-trace `"$raceTrace`""
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
        Press-Key $setup 13
        $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 30
        $trackTitle = [System.Text.StringBuilder]::new(256)
        for ($attempt = 0; $attempt -lt 20 -and $trackTitle.Length -eq 0; ++$attempt) {
            [void][ViperRaceFrontendWin32]::GetWindowText($race, $trackTitle, $trackTitle.Capacity)
            if ($trackTitle.Length -eq 0) { Start-Sleep -Milliseconds 50 }
        }
        if ($trackTitle.ToString() -notmatch "$([regex]::Escape($track)).*sports" -or
            $trackTitle.ToString() -notmatch 'Lap 1/2') {
            throw "Setup did not start the selected two-lap Sports/$track race: $($trackTitle.ToString())"
        }
        $setup = Wait-ResultsReturn $process 60
        Press-Key $setup 27
        if (-not $process.WaitForExit(15000)) {
            throw "Viper Racing did not exit after the Sports/$track results lifecycle"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "Sports/$track lifecycle exited with code $($process.ExitCode)" }
        $preference = [System.IO.File]::ReadAllText($preferencePath)
        if ($preference -notmatch "(?m)^track=$track$" -or
            $preference -notmatch '(?m)^car=sports$' -or
            $preference -notmatch '(?m)^laps=2$') {
            throw "Sports/$track setup selection was not retained: $preference"
        }
        Write-Output "Sports/$track trace race reached results and returned to setup."
    }
    Write-Output 'Trace-driven setup/result lifecycle reached all eight shipped tracks; original-game parity and physical simulation remain unverified.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
