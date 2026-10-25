param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$hostProcess = $null
$joinProcess = $null
$common = "--data-dir `"$DataDir`" --track bemidji --car viper --software --smoke-frames 8"

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
    $joinProcess = Start-RaceProcess "--data-dir `"$DataDir`" --track dundas --car viper --software --smoke-frames 8 --join 127.0.0.1 $mismatchPort"
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

    $hostProcess = Start-RaceProcess "$common --host $port"
    Start-Sleep -Milliseconds 300
    $joinProcess = Start-RaceProcess "$common --join 127.0.0.1 $port"

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
        throw "Multiplayer race smoke timed out (host pid $($hostProcess.Id), join pid $($joinProcess.Id))"
    }
    if ($hostProcess.ExitCode -ne 0 -or $joinProcess.ExitCode -ne 0) {
        throw "Multiplayer race clients exited with host=$($hostProcess.ExitCode), join=$($joinProcess.ExitCode)"
    }
    Write-Output "Two reconstructed Bemidji/Viper clients connected and completed the bounded route-runtime smoke on TCP port $port."
}
finally {
    Stop-IfRunning $joinProcess
    Stop-IfRunning $hostProcess
    if ($null -ne $joinProcess) { $joinProcess.Dispose() }
    if ($null -ne $hostProcess) { $hostProcess.Dispose() }
}
