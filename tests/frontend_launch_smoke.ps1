param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public delegate bool ViperEnumWindowsProc(IntPtr hwnd, IntPtr lParam);
public static class ViperFrontendWin32 {
    [DllImport("user32.dll")] public static extern bool EnumWindows(ViperEnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Find-ProcessWindow([int]$ProcessId, [string]$ClassName) {
    $found = [System.Collections.Generic.List[IntPtr]]::new()
    $callback = [ViperEnumWindowsProc] {
        param([IntPtr]$Window, [IntPtr]$Unused)
        [uint32]$owner = 0
        [void][ViperFrontendWin32]::GetWindowThreadProcessId($Window, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $name = [System.Text.StringBuilder]::new(128)
            [void][ViperFrontendWin32]::GetClassName($Window, $name, $name.Capacity)
            if ($name.ToString() -eq $ClassName) { [void]$found.Add($Window) }
        }
        return $true
    }
    [void][ViperFrontendWin32]::EnumWindows($callback, [IntPtr]::Zero)
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
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for window class $ClassName"
}

$process = $null
$configDir = Join-Path $env:TEMP ("viper-frontend-config-" + [Guid]::NewGuid().ToString('N'))
try {
    [void](New-Item -ItemType Directory -Path $configDir)
    $careerDir = Join-Path $configDir 'career'
    [void](New-Item -ItemType Directory -Path $careerDir)
    $setupPreference = Join-Path $configDir 'viper-racing-setup.cfg'
    [System.IO.File]::WriteAllText($setupPreference,
        "VIPER-RACING-SETUP 3`ntrack=bemidji`ncar=viper`nlaps=3`nopponent_strength=0`n")
    $careerPath = Join-Path $careerDir 'career0.dat'
    $payload = [byte[]]::new(0x674)
    $name = [System.Text.Encoding]::ASCII.GetBytes('QA Delete Test')
    [Array]::Copy($name, 0, $payload, 0, $name.Length)
    [uint32]$checksum = 0
    foreach ($value in $payload) { $checksum = [uint32]($checksum + $value) }
    $careerImage = [System.Collections.Generic.List[byte]]::new()
    $careerImage.AddRange([BitConverter]::GetBytes([uint32]0x43494E46))
    $careerImage.AddRange([BitConverter]::GetBytes([uint32]2))
    $careerImage.AddRange([BitConverter]::GetBytes([uint32]0x674))
    $careerImage.AddRange([BitConverter]::GetBytes($checksum))
    foreach ($value in $payload) { $careerImage.Add([byte]($value -bxor 0xA4)) }
    [System.IO.File]::WriteAllBytes($careerPath, $careerImage.ToArray())
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$configDir`" --software --diagnostics-only"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]67, [IntPtr]::Zero)) {
        throw 'Could not submit C to open the career slot screen'
    }
    Start-Sleep -Milliseconds 250
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]68, [IntPtr]::Zero)) {
        throw 'Could not request deletion of the selected career slot'
    }
    Start-Sleep -Milliseconds 150
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]78, [IntPtr]::Zero)) {
        throw 'Could not cancel career slot deletion'
    }
    Start-Sleep -Milliseconds 150
    if (-not (Test-Path -LiteralPath $careerPath)) {
        throw 'Cancelling career slot deletion removed the selected file'
    }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]68, [IntPtr]::Zero)) {
        throw 'Could not reopen career slot deletion confirmation'
    }
    Start-Sleep -Milliseconds 150
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]89, [IntPtr]::Zero)) {
        throw 'Could not confirm career slot deletion'
    }
    Start-Sleep -Milliseconds 150
    if (Test-Path -LiteralPath $careerPath) {
        throw 'Confirming career slot deletion left the selected file in place'
    }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]78, [IntPtr]::Zero)) {
        throw 'Could not create a career slot in the selected empty slot'
    }
    Start-Sleep -Milliseconds 250
    if (-not (Test-Path -LiteralPath $careerPath)) {
        throw 'Career slot creation did not write the selected native slot'
    }
    $createdBytes = [System.IO.File]::ReadAllBytes($careerPath)
    if ($createdBytes.Length -ne 0x684) {
        throw "Created career slot has an unexpected size: $($createdBytes.Length)"
    }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]78, [IntPtr]::Zero)) {
        throw 'Could not check that creating a career refuses an occupied slot'
    }
    Start-Sleep -Milliseconds 150
    $validBytesAfterCreateAttempt = [System.IO.File]::ReadAllBytes($careerPath)
    if ([Convert]::ToBase64String($createdBytes) -ne
        [Convert]::ToBase64String($validBytesAfterCreateAttempt)) {
        throw 'Creating a career overwrote a valid existing slot'
    }
    $corruptBytes = [byte[]](0x01, 0x02, 0x03, 0x04)
    [System.IO.File]::WriteAllBytes($careerPath, $corruptBytes)
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]78, [IntPtr]::Zero)) {
        throw 'Could not check that creating a career preserves an invalid slot'
    }
    Start-Sleep -Milliseconds 150
    $corruptBytesAfterCreateAttempt = [System.IO.File]::ReadAllBytes($careerPath)
    if ([Convert]::ToBase64String($corruptBytes) -ne
        [Convert]::ToBase64String($corruptBytesAfterCreateAttempt)) {
        throw 'Creating a career overwrote an invalid existing slot'
    }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]13, [IntPtr]::Zero)) {
        throw 'Could not validate the new career slot'
    }
    Start-Sleep -Milliseconds 200
    $process.Refresh()
    if ($process.HasExited) { throw 'Career slot screen closed the setup process unexpectedly' }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]27, [IntPtr]::Zero)) {
        throw 'Could not submit Escape to return from career slots'
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($process.HasExited) { throw 'Escape from career slots closed the app instead of returning to setup' }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]221, [IntPtr]::Zero)) {
        throw 'Could not select the next opponent-strength profile'
    }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]13, [IntPtr]::Zero)) {
        throw 'Could not submit Enter to the setup frontend'
    }
    $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    Start-Sleep -Milliseconds 750
    if (-not [ViperFrontendWin32]::PostMessage($race, 0x0100, [IntPtr]27, [IntPtr]::Zero)) {
        throw 'Could not close the launched race window'
    }
    if (-not $process.WaitForExit(15000)) { throw 'Race window did not shut down after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "viper_game exited with code $($process.ExitCode)" }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $savedPreference = [System.IO.File]::ReadAllText($setupPreference)
    if ($savedPreference -notmatch '(?m)^opponent_strength=1$') {
        throw "Setup did not persist the selected Intermediate profile: $savedPreference"
    }
    if ($stdout -notmatch 'selected AIDR profile 1') {
        throw "Selected profile did not reach AI setup. stdout=$stdout stderr=$stderr"
    }

    $randomConfigDir = Join-Path $configDir 'random-track'
    [void](New-Item -ItemType Directory -Path $randomConfigDir)
    $randomPreference = Join-Path $randomConfigDir 'viper-racing-setup.cfg'
    [System.IO.File]::WriteAllText($randomPreference,
        "VIPER-RACING-SETUP 3`ntrack=random`ncar=viper`nlaps=1`nopponent_strength=0`n")
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$randomConfigDir`" --software --diagnostics-only"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]13, [IntPtr]::Zero)) {
        throw 'Could not accept the saved Random track selection'
    }
    $race = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    $randomTitle = [System.Text.StringBuilder]::new(256)
    for ($attempt = 0; $attempt -lt 20 -and $randomTitle.ToString() -notmatch 'Lap 1/1'; ++$attempt) {
        [void]$randomTitle.Clear()
        [void][ViperFrontendWin32]::GetWindowText($race, $randomTitle, $randomTitle.Capacity)
        if ($randomTitle.ToString() -notmatch 'Lap 1/1') { Start-Sleep -Milliseconds 50 }
    }
    $randomTrackMatch = [regex]::Match($randomTitle.ToString(),
        'Viper Racing.*? ([A-Za-z]+)\s*&\s*viper.*?Lap 1/1')
    $installedTracks = @('hastings', 'heaven', 'kenyon', 'limbo', 'nfield', 'uptown', 'dundas', 'bemidji')
    if ($randomTitle.ToString() -notmatch 'Lap 1/1' -or
        -not $randomTrackMatch.Success -or
        $installedTracks -notcontains $randomTrackMatch.Groups[1].Value.ToLowerInvariant()) {
        throw "Random selection did not start a race on an installed track: $($randomTitle.ToString())"
    }
    if (-not $process.WaitForExit(0)) {
        [void][ViperFrontendWin32]::PostMessage($race, 0x0100, [IntPtr]27, [IntPtr]::Zero)
        if (-not $process.WaitForExit(15000)) { throw 'Random-track race did not exit after Escape' }
    }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Random-track race exited with code $($process.ExitCode)" }
    $realizedTrack = $randomTrackMatch.Groups[1].Value.ToLowerInvariant()
    $savedRandomPreference = [System.IO.File]::ReadAllText($randomPreference)
    if ($savedRandomPreference -notmatch "(?m)^track=$([regex]::Escape($realizedTrack))$" -or
        $savedRandomPreference -match '(?m)^track=random$') {
        throw "Random selection did not persist the realized concrete track: $savedRandomPreference"
    }

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$randomConfigDir`" --software --diagnostics-only"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    [void][ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]13, [IntPtr]::Zero)
    $restoredRace = Wait-ProcessWindow $process 'ViperRacingRuntimeWindow' 45
    $restoredTitle = [System.Text.StringBuilder]::new(256)
    $restoredPattern = "Lap 1/1"
    for ($attempt = 0; $attempt -lt 20 -and $restoredTitle.ToString() -notmatch $restoredPattern; ++$attempt) {
        [void]$restoredTitle.Clear()
        [void][ViperFrontendWin32]::GetWindowText($restoredRace, $restoredTitle, $restoredTitle.Capacity)
        if ($restoredTitle.ToString() -notmatch $restoredPattern) { Start-Sleep -Milliseconds 50 }
    }
    $restoredTrackMatch = [regex]::Match($restoredTitle.ToString(),
        'Viper Racing.*? ([A-Za-z]+)\s*&\s*viper.*?Lap 1/1')
    if ($restoredTitle.ToString() -notmatch $restoredPattern -or
        -not $restoredTrackMatch.Success -or
        $restoredTrackMatch.Groups[1].Value.ToLowerInvariant() -ne $realizedTrack) {
        throw "Relaunch did not restore the realized Random track: $($restoredTitle.ToString())"
    }
    [void][ViperFrontendWin32]::PostMessage($restoredRace, 0x0100, [IntPtr]27, [IntPtr]::Zero)
    if (-not $process.WaitForExit(15000)) { throw 'Restored Random-track race did not exit after Escape' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Restored Random-track race exited with code $($process.ExitCode)" }
    Write-Output 'Career slot actions, profile persistence, Random track selection, realized-track persistence/relaunch, setup-to-race loading, and clean exit passed.'
} catch {
    throw
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    if (Test-Path -LiteralPath $configDir) {
        Remove-Item -LiteralPath $configDir -Recurse -Force
    }
}
