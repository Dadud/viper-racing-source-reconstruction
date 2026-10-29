param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_route_policy_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null

function Invoke-DiagnosticsRun([string]$Name) {
    $trace = Join-Path $root "$Name.csv"
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --track bemidji --car viper --software --diagnostics-only -grid --smoke-frames 8 --record-trace `"$trace`""
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw "Could not start $Name runtime check" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) {
        throw "$Name runtime exited $($process.ExitCode): $stdout`n$stderr"
    }
    if (-not (Test-Path -LiteralPath $trace)) { throw "$Name runtime wrote no trace" }
    return @(Import-Csv -LiteralPath $trace)
}

try {
    $rows = Invoke-DiagnosticsRun 'diagnostics_only'
    if ($rows.Count -lt 2) { throw 'Diagnostics runtime trace has too few rows' }
    if ($rows | Where-Object { $_.diagnostics_only -ne '1' -or
            [int]$_.route_motion_step_count -ne 0 -or
            $_.route_state_initialized -ne '0' }) {
        throw 'Runtime must keep route progression uninitialized and motion disabled'
    }

    Write-Output 'Runtime trace kept route motion disabled. This startup-policy check did not test physics or driving.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
