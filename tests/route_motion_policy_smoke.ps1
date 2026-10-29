param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_route_policy_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null

function Invoke-RoutePolicyRun([string]$Name, [string]$ExtraArguments) {
    $trace = Join-Path $root "$Name.csv"
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = "--data-dir `"$DataDir`" --track bemidji --car viper --software --smoke-frames 8 --record-trace `"$trace`" $ExtraArguments"
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
    $defaultRows = Invoke-RoutePolicyRun 'default' ''
    if ($defaultRows.Count -lt 2) { throw 'Default runtime trace has too few rows' }
    if ($defaultRows | Where-Object { $_.diagnostics_only -ne '1' -or [int]$_.route_motion_step_count -ne 0 }) {
        throw 'Default runtime must keep the route-motion adapter disabled'
    }

    $prototypeRows = Invoke-RoutePolicyRun 'prototype' '--route-motion-prototype'
    if ($prototypeRows.Count -lt 2) { throw 'Prototype runtime trace has too few rows' }
    if ($prototypeRows | Where-Object { $_.diagnostics_only -ne '0' }) {
        throw 'Explicit route-motion prototype must be distinguishable from diagnostics-only mode'
    }
    $steps = @($prototypeRows | ForEach-Object { [int]$_.route_motion_step_count } | Measure-Object -Sum).Sum
    if ($steps -le 0) { throw 'Explicit prototype flag did not enable route motion' }

    Write-Output 'Default startup disables ideal-line motion; the explicit prototype flag enables the adapter. No route progress or race outcome is treated as gameplay evidence.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
