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
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --config-dir `"$configDir`" --software"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    $setup = Wait-ProcessWindow $process 'ViperRaceSetupFrontendWindow' 30
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]67, [IntPtr]::Zero)) {
        throw 'Could not submit C to open the career slot screen'
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($process.HasExited) { throw 'Career slot screen closed the setup process unexpectedly' }
    if (-not [ViperFrontendWin32]::PostMessage($setup, 0x0100, [IntPtr]27, [IntPtr]::Zero)) {
        throw 'Could not submit Escape to return from career slots'
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($process.HasExited) { throw 'Escape from career slots closed the app instead of returning to setup' }
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
    Write-Output 'Setup opened career slots, returned to setup, loaded the selected race, and exited cleanly.'
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
