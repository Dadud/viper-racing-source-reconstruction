param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_trace_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null
$capture = Join-Path $root 'capture.csv'
$replay = Join-Path $root 'replay.csv'
$repeat = Join-Path $root 'repeat.csv'
try {
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --smoke-frames 12 --record-trace $capture
    if ($LASTEXITCODE -ne 0) { throw "Trace capture exited with $LASTEXITCODE" }

    $inputLines = [System.IO.File]::ReadAllLines($capture)
    for ($frame = 1; $frame -lt $inputLines.Count; ++$frame) {
        $fields = $inputLines[$frame].Split(',')
        $fields[3] = if ($frame -eq 4 -or $frame -eq 5) { '1' } else { '0' }
        $fields[4] = '0'
        $fields[5] = if ($frame -ge 2 -and $frame -le 7) { '1' } else { '0' }
        $fields[6] = if ($frame -ge 9 -and $frame -le 10) { '1' } else { '0' }
        $inputLines[$frame] = $fields -join ','
    }
    [System.IO.File]::WriteAllLines($capture, $inputLines)

    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $capture --record-trace $replay
    if ($LASTEXITCODE -ne 0) { throw "Trace replay exited with $LASTEXITCODE" }
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $capture --record-trace $repeat
    if ($LASTEXITCODE -ne 0) { throw "Repeated trace replay exited with $LASTEXITCODE" }

    $replayedLines = [System.IO.File]::ReadAllLines($replay)
    $repeatedLines = [System.IO.File]::ReadAllLines($repeat)
    if ($inputLines.Count -lt 2) { throw 'Capture trace has no frame rows' }
    if ($inputLines.Count -ne $replayedLines.Count -or
        $replayedLines.Count -ne $repeatedLines.Count) {
        throw 'Trace row counts differ across input and replay runs'
    }
    $observedThrottle = $false
    $observedSteering = $false
    for ($index = 1; $index -lt $replayedLines.Count; ++$index) {
        if ($replayedLines[$index] -cne $repeatedLines[$index]) {
            throw "Repeated trace playback differs at CSV line $($index + 1)"
        }
        $fields = $replayedLines[$index].Split(',')
        if ($fields[5] -eq '1' -and [single]::Parse($fields[9], [cultureinfo]::InvariantCulture) -gt 0) {
            $observedThrottle = $true
        }
        if ($fields[3] -eq '1' -and [Math]::Abs([single]::Parse(
                $fields[10], [cultureinfo]::InvariantCulture)) -gt 0) {
            $observedSteering = $true
        }
    }
    if (-not $observedThrottle) { throw 'Playback did not produce positive speed under throttle' }
    if (-not $observedSteering) { throw 'Playback did not produce steering response' }
    Write-Output "Trace playback reproduced $($replayedLines.Count - 1) frames exactly across two runs."
}
finally {
    Remove-Item -LiteralPath $capture, $replay, $repeat -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Force -ErrorAction SilentlyContinue
}
