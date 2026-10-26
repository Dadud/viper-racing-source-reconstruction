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
try {
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --smoke-frames 12 --record-trace $capture
    if ($LASTEXITCODE -ne 0) { throw "Trace capture exited with $LASTEXITCODE" }

    $inputLines = [System.IO.File]::ReadAllLines($capture)
    for ($frame = 1; $frame -lt $inputLines.Count; ++$frame) {
        $fields = $inputLines[$frame].Split(',')
        # Keep gameplay actions neutral: this regression checks trace framing,
        # deterministic playback, and fixed-step diagnostics, not route driving.
        $fields[3] = '0'
        $fields[4] = '0'
        $fields[5] = '0'
        $fields[6] = '0'
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
    $bodyTimerColumn = [Array]::IndexOf($tickHeader, 'body_timer_0xe44')
    $bodyWheelPairScaleColumns = @(
        [Array]::IndexOf($tickHeader, 'body_wheel_pair_scale_0xdd4'),
        [Array]::IndexOf($tickHeader, 'body_wheel_pair_scale_0xdd8'))
    $bodyKinematicsReadyColumn = [Array]::IndexOf(
        $tickHeader, 'body_kinematics_input_ready')
    $bodyKinematicsNonFiniteColumn = [Array]::IndexOf(
        $tickHeader, 'body_kinematics_non_finite')
    $bodyKinematicsMissingCountColumn = [Array]::IndexOf(
        $tickHeader, 'body_kinematics_missing_float_count')
    $bodyIntegratorTimerReadyColumn = [Array]::IndexOf(
        $tickHeader, 'body_integrator_timer_ramp_source_ready')
    $bodyIntegratorTimerFiniteColumn = [Array]::IndexOf(
        $tickHeader, 'body_integrator_timer_known_fields_finite')
    $bodyIntegratorTimerMissingCountColumn = [Array]::IndexOf(
        $tickHeader, 'body_integrator_timer_missing_count')
    $bodyIntegratorRampColumn = [Array]::IndexOf(
        $tickHeader, 'body_integrator_ramp_0x50c')
    $bodyVehicleOrdinalColumn = [Array]::IndexOf(
        $tickHeader, 'body_vehicle_ordinal_0x510')
    $bodyIntegratorTimerMissingOffsets = [int[]]::new(3)
    for ($lane = 0; $lane -lt 3; ++$lane) {
        $bodyIntegratorTimerMissingOffsets[$lane] = [Array]::IndexOf(
            $tickHeader, "body_integrator_timer_missing_offset_$lane")
    }
    $bodyKinematicsMissingOffsets = [int[]]::new(44)
    for ($lane = 0; $lane -lt 44; ++$lane) {
        $bodyKinematicsMissingOffsets[$lane] = [Array]::IndexOf(
            $tickHeader, "body_kinematics_missing_offset_$lane")
    }
    $bodyBasisColumns = [int[]]::new(9)
    for ($basis = 0; $basis -lt 9; ++$basis) {
        $bodyBasisColumns[$basis] = [Array]::IndexOf(
            $tickHeader, "body_basis_$basis")
    }
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
    $wheelPreupdateColumns = [int[]]::new(4)
    $wheelProducerOwnedColumns = [int[]]::new(4)
    $wheelPriorDepthColumns = [int[]]::new(4)
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        $wheelActiveColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_active")
        $wheelDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_projected_depth")
        $wheelPreupdateColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_observed_value_0x158")
        $wheelProducerOwnedColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_0x158_native_producer_owned")
        $wheelPriorDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_previous_contact_depth_0xe8")
    }
    if ($tickIndexColumn -lt 0 -or $tickStepColumn -lt 0 -or
        $tickStepCountColumn -lt 0 -or $tickTimeColumn -lt 0 -or
        $tickSourceColumn -lt 0 -or $tickInputColumn -lt 0 -or
        $bodyTimerColumn -lt 0 -or
        @($bodyWheelPairScaleColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        $bodyKinematicsReadyColumn -lt 0 -or
        $bodyKinematicsNonFiniteColumn -lt 0 -or
        $bodyKinematicsMissingCountColumn -lt 0 -or
        @($bodyKinematicsMissingOffsets | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        $bodyIntegratorTimerReadyColumn -lt 0 -or
        $bodyIntegratorTimerFiniteColumn -lt 0 -or
        $bodyIntegratorTimerMissingCountColumn -lt 0 -or
        $bodyIntegratorRampColumn -lt 0 -or $bodyVehicleOrdinalColumn -lt 0 -or
        @($bodyIntegratorTimerMissingOffsets | Where-Object { $_ -lt 0 }).Count -gt 0) {
        throw 'Fixed-step trace header is missing required input/timing fields'
    }
    if (@($bodyBasisColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($bodyPositionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($bodyMotionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelActiveColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelPreupdateColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelProducerOwnedColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelPriorDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0) {
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
        $reportedMissingOffsets = @($bodyKinematicsMissingOffsets | Where-Object {
            -not [string]::IsNullOrWhiteSpace($fields[$_])
        } | ForEach-Object { $fields[$_] })
        if ($fields[$bodyKinematicsReadyColumn] -ne '1' -or
            $fields[$bodyKinematicsNonFiniteColumn] -ne '0' -or
            $fields[$bodyKinematicsMissingCountColumn] -ne '0' -or
            $reportedMissingOffsets.Count -ne 0) {
            throw "Body kinematics diagnostics differ at line $($index + 1): ready=$($fields[$bodyKinematicsReadyColumn]), nonfinite=$($fields[$bodyKinematicsNonFiniteColumn]), missingCount=$($fields[$bodyKinematicsMissingCountColumn]), offsets=$($reportedMissingOffsets -join ';')"
        }
        if ($fields[$bodyIntegratorTimerReadyColumn] -notin @('0', '1') -or
            $fields[$bodyIntegratorTimerFiniteColumn] -notin @('0', '1') -or
            $fields[$bodyIntegratorTimerMissingCountColumn] -notmatch '^[0-3]$') {
            throw "Body integrator timer diagnostics are malformed at line $($index + 1)"
        }
        if ($index -eq 1) {
            # The timer/history ring is still disconnected, but the bounded
            # 0x431150 paired differential producer may now own +0x158 when
            # all source fields and contact bytes are initialized.
            if ([string]::IsNullOrWhiteSpace($fields[$bodyIntegratorRampColumn]) -or
                [Math]::Abs([single]::Parse(
                    $fields[$bodyIntegratorRampColumn], [cultureinfo]::InvariantCulture) -
                    1.0) -gt 0.00001 -or
                $fields[$bodyVehicleOrdinalColumn] -ne '7') {
                throw 'First tick does not expose the initialized PCAR +0x50C/+0x510 constructor state'
            }
            for ($wheel = 0; $wheel -lt 4; ++$wheel) {
                if ($fields[$wheelProducerOwnedColumns[$wheel]] -notin @('0', '1') -or
                    $fields[$wheelPriorDepthColumns[$wheel]] -ne '0') {
                    throw "Wheel $wheel first-tick +0x158 ownership/value or +0xE8 constructor state differs"
                }
            }
        }
        foreach ($column in @($bodyIntegratorRampColumn, $bodyVehicleOrdinalColumn)) {
            if ([string]::IsNullOrWhiteSpace($fields[$column])) { continue }
            $value = [single]::Parse($fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($value) -or [single]::IsInfinity($value)) {
                throw "Body integrator source snapshot is non-finite at line $($index + 1)"
            }
        }
        $timerMissingOffsets = @($bodyIntegratorTimerMissingOffsets | Where-Object {
            -not [string]::IsNullOrWhiteSpace($fields[$_])
        } | ForEach-Object { $fields[$_] })
        if ($timerMissingOffsets.Count -ne
            [int]$fields[$bodyIntegratorTimerMissingCountColumn]) {
            throw "Body integrator timer missing-field count differs at line $($index + 1)"
        }
        foreach ($column in $bodyBasisColumns + $bodyPositionColumns) {
            $value = [single]::Parse($fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($value) -or [single]::IsInfinity($value)) {
                throw "Body position snapshot is non-finite at line $($index + 1)"
            }
        }
        $bodyTimer = [single]::Parse(
            $fields[$bodyTimerColumn], [cultureinfo]::InvariantCulture)
        if ($tickIndex -eq 0 -and [Math]::Abs($bodyTimer - (-100.0)) -gt 0.00001) {
            throw 'PCAR body trace does not show the native constructor +0xE44 sentinel'
        }
        if ([Math]::Abs($bodyTimer - (-100.0)) -gt 0.00001) {
            throw 'PCAR +0xE44 changed without its runtime timer/ordinal producer being connected'
        }
        foreach ($column in $bodyWheelPairScaleColumns) {
            if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                throw 'PCAR constructor wheel-pair scale is missing from the byte-backed body trace'
            }
            $scale = [single]::Parse(
                $fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($scale) -or [single]::IsInfinity($scale)) {
                throw "PCAR constructor wheel-pair scale is non-finite at line $($index + 1)"
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
            if ($fields[$wheelProducerOwnedColumns[$wheel]] -notin @('0', '1')) {
                throw "Wheel $wheel +0x158 producer-ownership flag is invalid"
            }
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
            foreach ($column in @(
                $wheelPreupdateColumns[$wheel], $wheelPriorDepthColumns[$wheel])) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) { continue }
                $historyValue = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($historyValue) -or
                    [single]::IsInfinity($historyValue)) {
                    throw "Wheel $wheel history snapshot is non-finite at line $($index + 1)"
                }
            }
        }
        ++$expectedTick
    }
    if ($wheelDepthObserved -contains $false) {
        throw 'Fixed-step trace did not capture projected depth for all four wheels'
    }
    $header = $replayedLines[0].Split(',')
    $routeDistanceColumn = [Array]::IndexOf(
        $header, 'player_route_distance_travelled')
    $livePositionColumn = [Array]::IndexOf($header, 'live_position')
    $liveFieldSizeColumn = [Array]::IndexOf($header, 'live_field_size')
    $opponentRouteDistanceColumns = @()
    foreach ($name in $header) {
        if ($name -match '^car\d+_route_distance_travelled$') {
            $opponentRouteDistanceColumns += [Array]::IndexOf($header, $name)
        }
    }
    if ($routeDistanceColumn -lt 0 -or $livePositionColumn -lt 0 -or
        $liveFieldSizeColumn -lt 0 -or $opponentRouteDistanceColumns.Count -ne 7) {
        throw 'Trace is missing live race-position route-state fields'
    }
    $aiValidColumns = @()
    for ($column = 0; $column -lt $header.Count; ++$column) {
        if ($header[$column] -match '^car\d+_ai_valid$') {
            $aiValidColumns += $column
        }
    }
    if ($aiValidColumns.Count -ne 7) {
        throw "Expected seven per-opponent AI action columns, found $($aiValidColumns.Count)"
    }
    $aiActionColumns = @()
    foreach ($validColumn in $aiValidColumns) {
        $carPrefix = $header[$validColumn] -replace '_ai_valid$', ''
        $columns = @(
            $validColumn,
            [Array]::IndexOf($header, "${carPrefix}_ai_throttle"),
            [Array]::IndexOf($header, "${carPrefix}_ai_brake"),
            [Array]::IndexOf($header, "${carPrefix}_ai_target_speed"),
            [Array]::IndexOf($header, "${carPrefix}_ai_leader"),
            [Array]::IndexOf($header, "${carPrefix}_ai_gap"))
        if (@($columns | Where-Object { $_ -lt 0 }).Count -gt 0) {
            throw "Trace is missing action fields for $carPrefix"
        }
        $aiActionColumns += ,$columns
    }
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
        $playerRouteDistance = [single]::Parse(
            $fields[$routeDistanceColumn], [cultureinfo]::InvariantCulture)
        $livePosition = [int]::Parse(
            $fields[$livePositionColumn], [cultureinfo]::InvariantCulture)
        $liveFieldSize = [int]::Parse(
            $fields[$liveFieldSizeColumn], [cultureinfo]::InvariantCulture)
        if ([single]::IsNaN($playerRouteDistance) -or
            [single]::IsInfinity($playerRouteDistance) -or
            $liveFieldSize -ne 8 -or $livePosition -lt 1 -or
            $livePosition -gt $liveFieldSize) {
            throw "Live route position diagnostics are malformed at frame $($index - 1)"
        }
        foreach ($column in $opponentRouteDistanceColumns) {
            $opponentRouteDistance = [single]::Parse(
                $fields[$column], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($opponentRouteDistance) -or
                [single]::IsInfinity($opponentRouteDistance)) {
                throw "Opponent route distance is non-finite at frame $($index - 1)"
            }
        }
        foreach ($columns in $aiActionColumns) {
            if ($fields[$columns[0]] -notin @('0', '1')) {
                throw "AI action validity is malformed at frame $($index - 1)"
            }
            if ($fields[$columns[0]] -eq '1') {
                foreach ($actionColumn in @($columns[1], $columns[2], $columns[3], $columns[5])) {
                    $actionValue = [single]::Parse(
                        $fields[$actionColumn], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($actionValue) -or [single]::IsInfinity($actionValue)) {
                        throw "AI action contains a non-finite value at frame $($index - 1)"
                    }
                }
                $throttleValue = [single]::Parse(
                    $fields[$columns[1]], [cultureinfo]::InvariantCulture)
                $brakeValue = [single]::Parse(
                    $fields[$columns[2]], [cultureinfo]::InvariantCulture)
                if ($throttleValue -lt 0.0 -or $throttleValue -gt 1.0 -or
                    $brakeValue -lt 0.0 -or $brakeValue -gt 1.0 -or
                    ($throttleValue -gt 0.0 -and $brakeValue -gt 0.0)) {
                    throw "AI throttle/brake decision is outside the policy range at frame $($index - 1)"
                }
            }
        }
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

    Write-Output "Fixed-step trace capture/playback reproduced $($replayedLines.Count - 1) frames exactly across two runs; no route progress, lap completion, opponent finish, or gameplay fidelity is asserted."
}
finally {
    Remove-Item -LiteralPath $capture, $replay, $repeat, $replayTicks,
        $repeatTicks -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Force -ErrorAction SilentlyContinue
}
