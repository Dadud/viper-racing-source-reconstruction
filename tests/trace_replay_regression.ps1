param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_trace_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null
$capture = Join-Path $root 'capture.csv'
$replay = Join-Path $root 'replay.csv'
$repeat = Join-Path $root 'repeat.csv'
$raceTrace = Join-Path $root 'race.csv'
$raceOutput = Join-Path $root 'race-output.csv'
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

    $raceLines = [System.Collections.Generic.List[string]]::new()
    $raceLines.Add($inputLines[0])
    $template = $inputLines[1].Split(',')
    for ($frame = 0; $frame -lt 1800; ++$frame) {
        $fields = [string[]]$template.Clone()
        $fields[0] = [string]$frame
        $fields[1] = '0'
        $fields[2] = '0.05'
        $fields[3] = '0'
        $fields[4] = '0'
        $fields[5] = '1'
        $fields[6] = '0'
        $raceLines.Add(($fields -join ','))
    }
    [System.IO.File]::WriteAllLines($raceTrace, $raceLines)
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $raceTrace --record-trace $raceOutput
    if ($LASTEXITCODE -ne 0) { throw "Full race trace playback exited with $LASTEXITCODE" }

    $raceOutputLines = [System.IO.File]::ReadAllLines($raceOutput)
    $finishFrame = -1
    $finishElapsed = 0.0
    $wheelQueryStart = 13 + 5 * 7
    $wheelQueriesObserved = [bool[]]::new(4)
    for ($index = 1; $index -lt $raceOutputLines.Count; ++$index) {
        $fields = $raceOutputLines[$index].Split(',')
        if ($fields.Count -lt ($wheelQueryStart + 5 * 4)) {
            throw 'Race trace is missing per-wheel contact diagnostics'
        }
        for ($wheel = 0; $wheel -lt 4; ++$wheel) {
            if ($fields[$wheelQueryStart + 5 * $wheel] -eq '1') {
                $wheelQueriesObserved[$wheel] = $true
            }
        }
        if ($fields[12] -eq '1' -and $finishFrame -lt 0) {
            $finishFrame = $index - 1
            $finishElapsed = [single]::Parse($fields[1], [cultureinfo]::InvariantCulture)
            if ($fields[11] -ne '1') { throw 'Player finish lap count differs from configured one-lap race' }
        }
        if ($finishFrame -ge 0 -and $index -gt ($finishFrame + 1) -and
            [single]::Parse($fields[1], [cultureinfo]::InvariantCulture) -ne $finishElapsed) {
            throw 'Player race clock changed after finish'
        }
    }
    if ($finishFrame -lt 0) { throw 'Player did not finish a complete Viper/Bemidji lap' }
    if ($finishElapsed -le 0) { throw 'Player finish time is not positive' }
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        if (-not $wheelQueriesObserved[$wheel]) {
            throw "Wheel $wheel never produced a successful live terrain query"
        }
    }
    $lastRaceFields = $raceOutputLines[-1].Split(',')
    for ($car = 0; $car -lt 7; ++$car) {
        $completedColumn = 16 + 5 * $car
        $finishedColumn = 17 + 5 * $car
        if ($lastRaceFields[$completedColumn] -lt 1 -or $lastRaceFields[$finishedColumn] -ne '1') {
            $segmentColumn = 13 + 5 * $car
            $speedColumn = 15 + 5 * $car
            throw "Opponent $car did not reach its finish state (segment=$($lastRaceFields[$segmentColumn]), speed=$($lastRaceFields[$speedColumn]), laps=$($lastRaceFields[$completedColumn]), finished=$($lastRaceFields[$finishedColumn]))"
        }
    }
    Write-Output "Viper/Bemidji route race finished in ${finishElapsed}s at frame $finishFrame; all seven provisional opponents finished."
}
finally {
    Remove-Item -LiteralPath $capture, $replay, $repeat, $raceTrace, $raceOutput -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Force -ErrorAction SilentlyContinue
}
