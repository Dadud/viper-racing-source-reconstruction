param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_trace_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null
$capture = Join-Path $root 'capture.csv'
$replay = Join-Path $root 'replay.csv'
$repeat = Join-Path $root 'repeat.csv'
$replayTicks = Join-Path $root 'replay-ticks.csv'
$repeatTicks = Join-Path $root 'repeat-ticks.csv'
$raceTrace = Join-Path $root 'race.csv'
$raceOutput = Join-Path $root 'race-output.csv'
$raceTicks = Join-Path $root 'race-ticks.csv'
try {
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --smoke-frames 12 --record-trace $capture
    if ($LASTEXITCODE -ne 0) { throw "Trace capture exited with $LASTEXITCODE" }

    $inputLines = [System.IO.File]::ReadAllLines($capture)
    for ($frame = 1; $frame -lt $inputLines.Count; ++$frame) {
        $fields = $inputLines[$frame].Split(',')
        # The recovered type-1 steering path ramps at a frame-rate-dependent
        # rate. Hold long enough to cross the route adapter's active threshold.
        $fields[3] = if ($frame -ge 4 -and $frame -le 10) { '1' } else { '0' }
        $fields[4] = '0'
        $fields[5] = if ($frame -ge 2 -and $frame -le 7) { '1' } else { '0' }
        $fields[6] = if ($frame -ge 9 -and $frame -le 10) { '1' } else { '0' }
        $inputLines[$frame] = $fields -join ','
    }
    [System.IO.File]::WriteAllLines($capture, $inputLines)

    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $capture --record-trace $replay `
        --record-tick-trace $replayTicks
    if ($LASTEXITCODE -ne 0) { throw "Trace replay exited with $LASTEXITCODE" }
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $capture --record-trace $repeat `
        --record-tick-trace $repeatTicks
    if ($LASTEXITCODE -ne 0) { throw "Repeated trace replay exited with $LASTEXITCODE" }

    $replayedLines = [System.IO.File]::ReadAllLines($replay)
    $repeatedLines = [System.IO.File]::ReadAllLines($repeat)
    if ($inputLines.Count -lt 2) { throw 'Capture trace has no frame rows' }
    if ($inputLines.Count -ne $replayedLines.Count -or
        $replayedLines.Count -ne $repeatedLines.Count) {
        throw 'Trace row counts differ across input and replay runs'
    }
    $replayedTickLines = [System.IO.File]::ReadAllLines($replayTicks)
    $repeatedTickLines = [System.IO.File]::ReadAllLines($repeatTicks)
    if ($replayedTickLines.Count -lt 2 -or
        $replayedTickLines.Count -ne $repeatedTickLines.Count -or
        $replayedTickLines[0] -cne $repeatedTickLines[0]) {
        throw 'Fixed-step trace is empty or has inconsistent replay row counts'
    }
    $tickHeader = $replayedTickLines[0].Split(',')
    $tickIndexColumn = [Array]::IndexOf($tickHeader, 'tick_index')
    $tickStepColumn = [Array]::IndexOf($tickHeader, 'scheduled_step_index')
    $tickStepCountColumn = [Array]::IndexOf($tickHeader, 'scheduled_step_count')
    $tickTimeColumn = [Array]::IndexOf($tickHeader, 'simulation_time')
    $tickSourceColumn = [Array]::IndexOf($tickHeader, 'input_provenance')
    $tickInputColumn = [Array]::IndexOf($tickHeader, 'keyboard_snapshot_hex')
    $bodyPositionColumns = @(
        [Array]::IndexOf($tickHeader, 'body_pos_x'),
        [Array]::IndexOf($tickHeader, 'body_pos_y'),
        [Array]::IndexOf($tickHeader, 'body_pos_z'))
    $bodyMotionColumns = @(
        [Array]::IndexOf($tickHeader, 'body_vel_x'),
        [Array]::IndexOf($tickHeader, 'body_vel_y'),
        [Array]::IndexOf($tickHeader, 'body_vel_z'),
        [Array]::IndexOf($tickHeader, 'body_ang_x'),
        [Array]::IndexOf($tickHeader, 'body_ang_y'),
        [Array]::IndexOf($tickHeader, 'body_ang_z'),
        [Array]::IndexOf($tickHeader, 'body_force_x'),
        [Array]::IndexOf($tickHeader, 'body_force_y'),
        [Array]::IndexOf($tickHeader, 'body_force_z'),
        [Array]::IndexOf($tickHeader, 'body_moment_x'),
        [Array]::IndexOf($tickHeader, 'body_moment_y'),
        [Array]::IndexOf($tickHeader, 'body_moment_z'))
    $wheelActiveColumns = [int[]]::new(4)
    $wheelDepthColumns = [int[]]::new(4)
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        $wheelActiveColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_active")
        $wheelDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_projected_depth")
    }
    if ($tickIndexColumn -lt 0 -or $tickStepColumn -lt 0 -or
        $tickStepCountColumn -lt 0 -or $tickTimeColumn -lt 0 -or
        $tickSourceColumn -lt 0 -or $tickInputColumn -lt 0) {
        throw 'Fixed-step trace header is missing required input/timing fields'
    }
    if (@($bodyPositionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($bodyMotionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelActiveColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0) {
        throw 'Fixed-step trace header is missing body or wheel state fields'
    }
    $expectedTick = 0
    $previousOuterUpdate = -1L
    $expectedStepIndex = 0
    $wheelDepthObserved = [bool[]]::new(4)
    for ($index = 1; $index -lt $replayedTickLines.Count; ++$index) {
        if ($replayedTickLines[$index] -cne $repeatedTickLines[$index]) {
            throw "Repeated fixed-step trace differs at CSV line $($index + 1)"
        }
        $fields = $replayedTickLines[$index].Split(',')
        if ($fields.Count -ne $tickHeader.Count) {
            throw "Fixed-step trace field count is invalid at line $($index + 1)"
        }
        $outerUpdate = [long]::Parse($fields[0], [cultureinfo]::InvariantCulture)
        $stepIndex = [int]::Parse($fields[$tickStepColumn], [cultureinfo]::InvariantCulture)
        $stepCount = [int]::Parse($fields[$tickStepCountColumn], [cultureinfo]::InvariantCulture)
        $tickIndex = [int]::Parse($fields[$tickIndexColumn], [cultureinfo]::InvariantCulture)
        $simTime = [single]::Parse($fields[$tickTimeColumn], [cultureinfo]::InvariantCulture)
        if ($outerUpdate -ne $previousOuterUpdate) {
            if ($stepIndex -ne 0) { throw 'First tick in an outer update did not start at step zero' }
            $expectedStepIndex = 0
            $previousOuterUpdate = $outerUpdate
        } else {
            ++$expectedStepIndex
            if ($stepIndex -ne $expectedStepIndex) { throw 'Fixed-step rows are out of scheduler order' }
        }
        if ($stepCount -lt 1 -or $stepIndex -ge $stepCount -or
            $tickIndex -ne $expectedTick -or
            [Math]::Abs($simTime - ($tickIndex * 0.016)) -gt 0.00001 -or
            $fields[$tickSourceColumn] -ne '1' -or
            $fields[$tickInputColumn].Length -ne 512) {
            throw "Fixed-step input/timing snapshot is invalid at line $($index + 1)"
        }
        foreach ($column in $bodyPositionColumns) {
            $value = [single]::Parse($fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($value) -or [single]::IsInfinity($value)) {
                throw "Body position snapshot is non-finite at line $($index + 1)"
            }
        }
        foreach ($column in $bodyMotionColumns) {
            if ([string]::IsNullOrWhiteSpace($fields[$column])) { continue }
            $value = [single]::Parse($fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($value) -or [single]::IsInfinity($value)) {
                throw "Body motion/force snapshot is non-finite at line $($index + 1)"
            }
        }
        for ($wheel = 0; $wheel -lt 4; ++$wheel) {
            $active = $fields[$wheelActiveColumns[$wheel]]
            if (-not [string]::IsNullOrWhiteSpace($active) -and $active -notin @('0', '1')) {
                throw "Wheel $wheel contact-active value is invalid at line $($index + 1)"
            }
            $depth = $fields[$wheelDepthColumns[$wheel]]
            if (-not [string]::IsNullOrWhiteSpace($depth)) {
                $depthValue = [single]::Parse($depth, [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($depthValue) -or [single]::IsInfinity($depthValue)) {
                    throw "Wheel $wheel projected depth is non-finite at line $($index + 1)"
                }
                $wheelDepthObserved[$wheel] = $true
            }
        }
        ++$expectedTick
    }
    if ($wheelDepthObserved -contains $false) {
        throw 'Fixed-step trace did not capture projected depth for all four wheels'
    }
    $observedThrottle = $false
    $observedSteering = $false
    $header = $replayedLines[0].Split(',')
    $pcarPrefixStart = [Array]::IndexOf($header, 'pcar_prefix_0xde0')
    $pcarBodyStart = [Array]::IndexOf($header, 'pcar_body_0xde0')
    $pcarMultiplierIndex = [Array]::IndexOf($header, 'pcar_0x40d270_multiplier')
    $physicsStepsIndex = [Array]::IndexOf($header, 'physics_steps_scheduled')
    $physicsTickIndex = [Array]::IndexOf($header, 'physics_step_count')
    $pcarCallbackTickIndex = [Array]::IndexOf($header, 'pcar_callback_step_count')
    $pcarTaskIndex = [Array]::IndexOf($header, 'pcar_callback_task_stream_index')
    $pcarTaskDispatchIndex = [Array]::IndexOf(
        $header, 'pcar_task_projection_dispatch_steps')
    $pcarTimestampIndex = [Array]::IndexOf($header, 'pcar_body_0xecc')
    $pcarTimestampBackendIndex = [Array]::IndexOf(
        $header, 'pcar_timestamp_backend_callback_requested')
    $pcarTimestampGateIndex = [Array]::IndexOf($header, 'control_0x509360')
    $physicsTimeIndex = [Array]::IndexOf($header, 'physics_simulation_time')
    $sharedFloatSourceIndex = [Array]::IndexOf($header, 'control_0x509344')
    $sharedByteSourceIndex = [Array]::IndexOf($header, 'control_0x509420')
    $sharedFloatCacheIndex = [Array]::IndexOf($header, 'pcar_body_0x4f0')
    $sharedByteCacheIndex = [Array]::IndexOf($header, 'pcar_body_0x4ec')
    if ($pcarPrefixStart -lt 0 -or $pcarBodyStart -lt 0 -or
        $pcarMultiplierIndex -lt 0 -or $physicsStepsIndex -lt 0 -or
        $physicsTickIndex -lt 0 -or $pcarCallbackTickIndex -lt 0 -or
        $pcarTaskIndex -lt 0 -or $pcarTaskDispatchIndex -lt 0 -or
        $pcarTimestampIndex -lt 0 -or $pcarTimestampBackendIndex -lt 0 -or
        $pcarTimestampGateIndex -lt 0 -or
        $physicsTimeIndex -lt 0 -or $sharedFloatSourceIndex -lt 0 -or
        $sharedByteSourceIndex -lt 0 -or $sharedFloatCacheIndex -lt 0 -or
        $sharedByteCacheIndex -lt 0) {
        throw 'Trace is missing PCAR or fixed-step scheduler fields'
    }
    $observedPcarBodyState = $false
    $observedConfiguredPcarMultiplier = $false
    $observedPhysicsStep = $false
    $observedPcarTimestamp = $false
    $previousPhysicsTick = -1
    for ($index = 1; $index -lt $replayedLines.Count; ++$index) {
        if ($replayedLines[$index] -cne $repeatedLines[$index]) {
            throw "Repeated trace playback differs at CSV line $($index + 1)"
        }
        $fields = $replayedLines[$index].Split(',')
        $sharedFloatSource = [single]::Parse(
            $fields[$sharedFloatSourceIndex], [cultureinfo]::InvariantCulture)
        $sharedFloatCache = [single]::Parse(
            $fields[$sharedFloatCacheIndex], [cultureinfo]::InvariantCulture)
        $sharedByteSource = [int]::Parse(
            $fields[$sharedByteSourceIndex], [cultureinfo]::InvariantCulture)
        $sharedByteCache = [int]::Parse(
            $fields[$sharedByteCacheIndex], [cultureinfo]::InvariantCulture)
        if ([Math]::Abs($sharedFloatSource - $sharedFloatCache) -gt 0.000001 -or
            $sharedByteSource -ne $sharedByteCache) {
            throw "PCAR shared-global cache stores differ from their getter values at frame $($index - 1)"
        }
        $pcarMultiplier = [single]::Parse(
            $fields[$pcarMultiplierIndex], [cultureinfo]::InvariantCulture)
        if ([Math]::Abs($pcarMultiplier - 1.0) -gt 0.000001) {
            throw "PCAR multiplier did not use the shipped GAME throttle_boost value at frame $($index - 1)"
        }
        $observedConfiguredPcarMultiplier = $true
        if ($fields[5] -eq '1' -and [single]::Parse($fields[9], [cultureinfo]::InvariantCulture) -gt 0) {
            $observedThrottle = $true
        }
        if ($fields[3] -eq '1' -and [Math]::Abs([single]::Parse(
                $fields[10], [cultureinfo]::InvariantCulture)) -gt 0) {
            $observedSteering = $true
        }
        $scheduledSteps = [int]::Parse($fields[$physicsStepsIndex],
            [cultureinfo]::InvariantCulture)
        $physicsTick = [int]::Parse($fields[$physicsTickIndex],
            [cultureinfo]::InvariantCulture)
        $pcarCallbackTick = [int]::Parse($fields[$pcarCallbackTickIndex],
            [cultureinfo]::InvariantCulture)
        $pcarTaskStreamIndex = [int]::Parse($fields[$pcarTaskIndex],
            [cultureinfo]::InvariantCulture)
        $pcarTaskDispatchSteps = [int]::Parse($fields[$pcarTaskDispatchIndex],
            [cultureinfo]::InvariantCulture)
        $pcarTimestampBackendRequested = [int]::Parse(
            $fields[$pcarTimestampBackendIndex], [cultureinfo]::InvariantCulture)
        $pcarTimestampGate = [int]::Parse(
            $fields[$pcarTimestampGateIndex], [cultureinfo]::InvariantCulture)
        $physicsTime = [single]::Parse($fields[$physicsTimeIndex],
            [cultureinfo]::InvariantCulture)
        if ($scheduledSteps -lt 0 -or $scheduledSteps -gt 2 -or
            $physicsTick -lt $previousPhysicsTick -or
            $pcarCallbackTick -ne $physicsTick -or
            $pcarTaskStreamIndex -ne 7 -or
            $pcarTaskDispatchSteps -ne $pcarCallbackTick -or
            [Math]::Abs($physicsTime - ($physicsTick * 0.016)) -gt 0.0001) {
            throw "Fixed-step clock, PCAR task binding, or callback cadence is inconsistent at frame $($index - 1)"
        }
        if ($physicsTick -gt 0) { $observedPhysicsStep = $true }
        if ($scheduledSteps -gt 0) {
            if ($pcarTimestampBackendRequested -ne 0 -and
                $pcarTimestampBackendRequested -ne 1) {
                throw "PCAR timestamp latch violated its initialized gate at frame $($index - 1)"
            }
            if ([string]::IsNullOrEmpty($fields[$pcarTimestampIndex])) {
                if ($pcarTimestampBackendRequested -ne 0) {
                    throw "PCAR requested the timestamp backend without an initialized +0xECC field at frame $($index - 1)"
                }
            } else {
                $pcarTimestamp = [single]::Parse(
                    $fields[$pcarTimestampIndex], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($pcarTimestamp) -or
                    [single]::IsInfinity($pcarTimestamp) -or
                    ($pcarTimestampBackendRequested -eq 1 -and
                        $pcarTimestampGate -eq 0) -or
                    ($pcarTimestampGate -eq 0 -and $pcarTimestamp -ne 0.0)) {
                    throw "PCAR timestamp latch violated its initialized gate at frame $($index - 1)"
                }
            }
            $observedPcarTimestamp = $true
        }
        $previousPhysicsTick = $physicsTick
        for ($field = 0; $field -lt 4; ++$field) {
            if ([string]::IsNullOrEmpty($fields[$pcarBodyStart + $field])) { continue }
            if ($fields[$pcarPrefixStart + $field] -cne
                    $fields[$pcarBodyStart + $field]) {
                throw "PCAR callback scalar $field differs from its persistent body bytes at frame $($index - 1)"
            }
            $observedPcarBodyState = $true
        }
    }
    if (-not $observedPcarBodyState) {
        throw 'No trace frame recorded the materialized PCAR body scalar fields'
    }
    if (-not $observedConfiguredPcarMultiplier) {
        throw 'No trace frame recorded the configured PCAR callback multiplier'
    }
    if (-not $observedPhysicsStep) {
        throw 'Trace playback did not advance the recovered fixed-step clock'
    }
    if (-not $observedPcarTimestamp) {
        throw 'Trace playback did not record the PCAR timestamp boundary on a callback step'
    }
    if (-not $observedThrottle) { throw 'Playback did not produce positive speed under throttle' }
    if (-not $observedSteering) { throw 'Playback did not produce steering response' }
    Write-Output "Trace playback reproduced $($replayedLines.Count - 1) frames exactly across two runs."

    $raceLines = [System.Collections.Generic.List[string]]::new()
    $raceLines.Add($inputLines[0])
    $template = $inputLines[1].Split(',')
    # The live runtime follows the recovered 16 ms task cadence and two-step
    # catch-up cap. A 32 ms schedule exercises two native ticks per row while
    # providing enough simulated time for the full lap and opponent finishes.
    for ($frame = 0; $frame -lt 3000; ++$frame) {
        $fields = [string[]]$template.Clone()
        $fields[0] = [string]$frame
        $fields[1] = '0'
        $fields[2] = '0.032'
        $fields[3] = '0'
        $fields[4] = '0'
        $fields[5] = '1'
        $fields[6] = '0'
        $raceLines.Add(($fields -join ','))
    }
    [System.IO.File]::WriteAllLines($raceTrace, $raceLines)
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --play-trace $raceTrace --record-trace $raceOutput `
        --record-tick-trace $raceTicks
    if ($LASTEXITCODE -ne 0) { throw "Full race trace playback exited with $LASTEXITCODE" }

    $raceOutputLines = [System.IO.File]::ReadAllLines($raceOutput)
    $finishFrame = -1
    $finishElapsed = 0.0
    $raceHeader = $raceOutputLines[0].Split(',')
    $wheelQueryStart = 13 + 5 * 7
    $wheelQueriesObserved = [bool[]]::new(4)
    $wheelOriginStart = $wheelQueryStart + 5 * 4
    $wheelSurfaceStart = $wheelOriginStart + 3 * 4
    $wheelSurfaceSamplesObserved = [bool[]]::new(4)
    if ($raceHeader[$wheelOriginStart] -ne 'wheel0_query_x' -or
        $raceHeader[$wheelOriginStart + 1] -ne 'wheel0_query_y' -or
        $raceHeader[$wheelOriginStart + 2] -ne 'wheel0_query_z' -or
        $raceHeader[$wheelSurfaceStart] -ne 'wheel0_surface_x') {
        throw 'Race trace is missing the per-wheel world-space terrain query origins'
    }
    for ($index = 1; $index -lt $raceOutputLines.Count; ++$index) {
        $fields = $raceOutputLines[$index].Split(',')
        if ($fields.Count -lt ($wheelSurfaceStart + 6 * 4)) {
            throw 'Race trace is missing per-wheel contact diagnostics'
        }
        for ($wheel = 0; $wheel -lt 4; ++$wheel) {
            if ($fields[$wheelQueryStart + 5 * $wheel] -eq '1') {
                $wheelQueriesObserved[$wheel] = $true
                $originStart = $wheelOriginStart + 3 * $wheel
                $origin = [single[]]::new(3)
                for ($component = 0; $component -lt 3; ++$component) {
                    $origin[$component] = [single]::Parse(
                        $fields[$originStart + $component], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($origin[$component]) -or
                        [single]::IsInfinity($origin[$component])) {
                        throw "Wheel $wheel emitted a non-finite terrain query origin"
                    }
                }
                $sampleStart = $wheelSurfaceStart + 6 * $wheel
                $sample = [single[]]::new(6)
                for ($component = 0; $component -lt 6; ++$component) {
                    $sample[$component] = [single]::Parse(
                        $fields[$sampleStart + $component], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($sample[$component]) -or
                        [single]::IsInfinity($sample[$component])) {
                        throw "Wheel $wheel emitted a non-finite terrain contact sample"
                    }
                }
                $normalLength = [Math]::Sqrt(
                    $sample[3] * $sample[3] + $sample[4] * $sample[4] +
                    $sample[5] * $sample[5])
                if ($normalLength -le 0.1) {
                    throw "Wheel $wheel emitted a degenerate terrain normal"
                }
                $wheelSurfaceSamplesObserved[$wheel] = $true
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
    $raceTickLines = [System.IO.File]::ReadAllLines($raceTicks)
    if ($raceTickLines.Count -lt 2) { throw 'Full race produced no fixed-step trace rows' }
    $raceTickHeader = $raceTickLines[0].Split(',')
    $raceTickIndexColumn = [Array]::IndexOf($raceTickHeader, 'tick_index')
    $raceTickTimeColumn = [Array]::IndexOf($raceTickHeader, 'simulation_time')
    $raceTickThrottleColumn = [Array]::IndexOf($raceTickHeader, 'throttle')
    if ($raceTickIndexColumn -lt 0 -or $raceTickTimeColumn -lt 0 -or
        $raceTickThrottleColumn -lt 0) {
        throw 'Full race fixed-step trace is missing required fields'
    }
    $raceTickThrottleObserved = $false
    for ($index = 1; $index -lt $raceTickLines.Count; ++$index) {
        $fields = $raceTickLines[$index].Split(',')
        $tickIndex = [int]::Parse($fields[$raceTickIndexColumn], [cultureinfo]::InvariantCulture)
        $simTime = [single]::Parse($fields[$raceTickTimeColumn], [cultureinfo]::InvariantCulture)
        if ($tickIndex -ne ($index - 1) -or
            [Math]::Abs($simTime - ($tickIndex * 0.016)) -gt 0.00001) {
            throw "Full race fixed-step trace has a tick gap or timer mismatch at row $index"
        }
        if ([single]::Parse($fields[$raceTickThrottleColumn], [cultureinfo]::InvariantCulture) -gt 0) {
            $raceTickThrottleObserved = $true
        }
    }
    if (-not $raceTickThrottleObserved) {
        throw 'Full race fixed-step trace never captured throttle input'
    }
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        if (-not $wheelQueriesObserved[$wheel] -or -not $wheelSurfaceSamplesObserved[$wheel]) {
            throw "Wheel $wheel never produced a valid live terrain contact sample"
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
    Remove-Item -LiteralPath $capture, $replay, $repeat, $replayTicks,
        $repeatTicks, $raceTrace, $raceOutput, $raceTicks -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Force -ErrorAction SilentlyContinue
}
