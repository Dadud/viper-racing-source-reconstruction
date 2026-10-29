param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$hostProcess = $null
$joinProcess = $null
$traceDirectory = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("viper-network-session-smoke-" + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $traceDirectory)
$common = "--data-dir `"$DataDir`" --track bemidji --car viper --software --smoke-frames 8 --diagnostics-only"

function New-RaceProcessStartInfo([string]$Arguments) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = $Arguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    return $startInfo
}

function Start-RaceProcess([string]$Arguments) {
    $process = [Diagnostics.Process]::Start((New-RaceProcessStartInfo $Arguments))
    if ($null -eq $process) { throw 'Could not start a multiplayer smoke client' }
    return $process
}

function Stop-IfRunning([Diagnostics.Process]$Process) {
    if ($null -eq $Process) { return }
    $Process.Refresh()
    if (-not $Process.HasExited) {
        $Process.Kill()
        [void]$Process.WaitForExit(5000)
    }
}

function Assert-NoRouteMotion([string]$TracePath) {
    if (-not (Test-Path -LiteralPath $TracePath)) {
        throw "Network session client did not produce its trace: $TracePath"
    }
    $rows = @(Import-Csv -LiteralPath $TracePath)
    if ($rows.Count -eq 0 -or
        -not ($rows[0].PSObject.Properties.Name -contains 'route_motion_step_count')) {
        throw "Network session trace has no route-motion evidence: $TracePath"
    }
    foreach ($row in $rows) {
        if ([int]$row.route_motion_step_count -ne 0) {
            throw "Diagnostics-only network session advanced route motion in $TracePath"
        }
    }
}

$portListener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$portListener.Start()
$port = ([Net.IPEndPoint]$portListener.LocalEndpoint).Port
$portListener.Stop()

try {
    # A connected pair must reject incompatible setup before either runtime
    # starts consuming race controls.
    $mismatchListener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $mismatchListener.Start()
    $mismatchPort = ([Net.IPEndPoint]$mismatchListener.LocalEndpoint).Port
    $mismatchListener.Stop()
    $hostProcess = Start-RaceProcess "$common --host $mismatchPort"
    Start-Sleep -Milliseconds 300
    $joinProcess = Start-RaceProcess "--data-dir `"$DataDir`" --track dundas --car viper --software --smoke-frames 8 --diagnostics-only --join 127.0.0.1 $mismatchPort"
    $mismatchDeadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $mismatchDeadline) {
        $hostProcess.Refresh()
        $joinProcess.Refresh()
        if ($hostProcess.HasExited -and $joinProcess.HasExited) { break }
        Start-Sleep -Milliseconds 100
    }
    $hostProcess.Refresh()
    $joinProcess.Refresh()
    if (-not $hostProcess.HasExited -or -not $joinProcess.HasExited -or
        $hostProcess.ExitCode -eq 0 -or $joinProcess.ExitCode -eq 0) {
        throw "Mismatched multiplayer setup was not rejected (host=$($hostProcess.ExitCode), join=$($joinProcess.ExitCode))"
    }
    $joinProcess.Dispose()
    $hostProcess.Dispose()
    $joinProcess = $null
    $hostProcess = $null

    $hostTrace = Join-Path $traceDirectory 'host.csv'
    $joinTrace = Join-Path $traceDirectory 'join.csv'
    $hostProcess = Start-RaceProcess "$common --record-trace `"$hostTrace`" --host $port"
    Start-Sleep -Milliseconds 300
    # The default human occupies the final slot (7). Put the peer in slot 6
    # so the setup handshake exercises distinct, non-hardcoded ownership.
    $joinProcess = Start-RaceProcess "$common --record-trace `"$joinTrace`" --network-slot 6 --join 127.0.0.1 $port"

    $deadline = [DateTime]::UtcNow.AddSeconds(50)
    while ([DateTime]::UtcNow -lt $deadline) {
        $hostProcess.Refresh()
        $joinProcess.Refresh()
        if ($hostProcess.HasExited -and $joinProcess.HasExited) { break }
        Start-Sleep -Milliseconds 100
    }

    $hostProcess.Refresh()
    $joinProcess.Refresh()
    if (-not $hostProcess.HasExited -or -not $joinProcess.HasExited) {
        throw "Multiplayer network session smoke timed out (host pid $($hostProcess.Id), join pid $($joinProcess.Id))"
    }
    if ($hostProcess.ExitCode -ne 0 -or $joinProcess.ExitCode -ne 0) {
        throw "Multiplayer race clients exited with host=$($hostProcess.ExitCode), join=$($joinProcess.ExitCode)"
    }
    Assert-NoRouteMotion $hostTrace
    Assert-NoRouteMotion $joinTrace
    Write-Output "Two reconstructed Bemidji/Viper clients completed the bounded diagnostics-only network session smoke with zero route-motion steps on TCP port $port."
}
finally {
    Stop-IfRunning $joinProcess
    Stop-IfRunning $hostProcess
    if ($null -ne $joinProcess) { $joinProcess.Dispose() }
    if ($null -ne $hostProcess) { $hostProcess.Dispose() }
    Remove-Item -LiteralPath $traceDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
