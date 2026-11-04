param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$root = Join-Path ([System.IO.Path]::GetTempPath()) "viper_trace_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $root | Out-Null
$capture = Join-Path $root 'capture.csv'
$rightCapture = Join-Path $root 'right-capture.csv'
$yawDataDir = Join-Path $root 'YawData'
$replay = Join-Path $root 'replay.csv'
$rightReplay = Join-Path $root 'right-replay.csv'
$yawReplay = Join-Path $root 'yaw-replay.csv'
$repeat = Join-Path $root 'repeat.csv'
$replayTicks = Join-Path $root 'replay-ticks.csv'
$rightReplayTicks = Join-Path $root 'right-replay-ticks.csv'
$yawReplayTicks = Join-Path $root 'yaw-replay-ticks.csv'
$repeatTicks = Join-Path $root 'repeat-ticks.csv'
try {
    # Author a repeatable fixed-step driver input fixture. Route motion is
    # disabled below, so these actions exercise the real binding/PCAR control
    # path without using the ideal line as a movement surrogate.
    $inputLines = [System.Collections.Generic.List[string]]::new()
    [void]$inputLines.Add('frame,elapsed_seconds,delta_seconds,steer_left,steer_right,throttle,brake,phase')
    $frameCount = 320
    for ($frame = 0; $frame -lt $frameCount; ++$frame) {
        $left = 0
        $right = 0
        $throttle = 0
        $brake = 0
        $phase = 'coast'
        if ($frame -lt 96) {
            $throttle = 1
            $phase = 'throttle'
        } elseif ($frame -ge 160 -and $frame -lt 208) {
            $left = 1
            $phase = 'steer-left'
        } elseif ($frame -ge 240 -and $frame -lt 288) {
            $brake = 1
            $phase = 'brake'
        }
        $elapsed = [single]($frame * 0.016)
        $row = '{0},{1},{2},{3},{4},{5},{6},{7}' -f $frame,
            $elapsed.ToString('R', [cultureinfo]::InvariantCulture), '0.016',
            $left, $right, $throttle, $brake, $phase
        [void]$inputLines.Add($row)
    }
    [System.IO.File]::WriteAllLines($capture, $inputLines)

    # Mirror only steering while preserving timing, throttle, and brake.
    $rightInputLines = [System.Collections.Generic.List[string]]::new()
    [void]$rightInputLines.Add($inputLines[0])
    foreach ($line in $inputLines | Select-Object -Skip 1) {
        $fields = $line.Split(',')
        $steerLeft = $fields[3]
        $fields[3] = $fields[4]
        $fields[4] = $steerLeft
        if ($fields[7] -ceq 'steer-left') { $fields[7] = 'steer-right' }
        [void]$rightInputLines.Add(($fields -join ','))
    }
    [System.IO.File]::WriteAllLines($rightCapture, $rightInputLines)

    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --diagnostics-only -grid --play-trace $capture --record-trace $replay `
        --record-tick-trace $replayTicks
    if ($LASTEXITCODE -ne 0) { throw "Trace replay exited with $LASTEXITCODE" }
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --diagnostics-only -grid --play-trace $capture --record-trace $repeat `
        --record-tick-trace $repeatTicks
    if ($LASTEXITCODE -ne 0) { throw "Repeated trace replay exited with $LASTEXITCODE" }
    & $GameExe --data-dir $DataDir --track bemidji --car viper --software `
        --diagnostics-only -grid --play-trace $rightCapture `
        --record-trace $rightReplay --record-tick-trace $rightReplayTicks
    if ($LASTEXITCODE -ne 0) { throw "Mirrored steering replay exited with $LASTEXITCODE" }

    # Exercise the optional native yaw-response selector against an isolated
    # data copy. Never edit the user's supplied installation for this case.
    Copy-Item -LiteralPath $DataDir -Destination $yawDataDir -Recurse
    $yawOptionsPath = Join-Path $yawDataDir 'options.def'
    if (-not (Test-Path -LiteralPath $yawOptionsPath)) {
        throw 'Temporary yaw-control data copy omitted options.def'
    }
    $yawOptions = [System.IO.File]::ReadAllText($yawOptionsPath)
    $yawPattern = '(?m)^yaw_control\s+[-+]?\d+\s*$'
    if (-not [regex]::IsMatch($yawOptions, $yawPattern)) {
        throw 'Temporary options.def has no integer yaw_control setting'
    }
    $yawOptions = [regex]::Replace($yawOptions, $yawPattern, 'yaw_control 1', 1)
    [System.IO.File]::WriteAllText($yawOptionsPath, $yawOptions)
    & $GameExe --data-dir $yawDataDir --track bemidji --car viper --software `
        --diagnostics-only -grid --play-trace $capture `
        --record-trace $yawReplay --record-tick-trace $yawReplayTicks
    if ($LASTEXITCODE -ne 0) { throw "Yaw-control fixed-step replay exited with $LASTEXITCODE" }

    $replayedLines = [System.IO.File]::ReadAllLines($replay)
    $rightReplayedLines = [System.IO.File]::ReadAllLines($rightReplay)
    $yawReplayedLines = [System.IO.File]::ReadAllLines($yawReplay)
    $repeatedLines = [System.IO.File]::ReadAllLines($repeat)
    if ($inputLines.Count -lt 2) { throw 'Capture trace has no frame rows' }
    if ($inputLines.Count -ne $replayedLines.Count -or
        $replayedLines.Count -ne $repeatedLines.Count) {
        throw 'Trace row counts differ across input and replay runs'
    }
    $replayedTickLines = [System.IO.File]::ReadAllLines($replayTicks)
    $rightReplayedTickLines = [System.IO.File]::ReadAllLines($rightReplayTicks)
    $yawReplayedTickLines = [System.IO.File]::ReadAllLines($yawReplayTicks)
    $repeatedTickLines = [System.IO.File]::ReadAllLines($repeatTicks)
    if ($replayedTickLines.Count -lt 2 -or
        $replayedTickLines.Count -ne $repeatedTickLines.Count -or
        $replayedTickLines[0] -cne $repeatedTickLines[0]) {
        throw 'Fixed-step trace is empty or has inconsistent replay row counts'
    }
    if ($rightReplayedLines.Count -ne $replayedLines.Count -or
        $rightReplayedTickLines.Count -ne $replayedTickLines.Count -or
        $rightReplayedTickLines[0] -cne $replayedTickLines[0]) {
        throw 'Mirrored steering trace row counts or fixed-step schemas differ'
    }
    if ($yawReplayedLines.Count -ne $replayedLines.Count -or
        $yawReplayedTickLines.Count -ne $replayedTickLines.Count -or
        $yawReplayedTickLines[0] -cne $replayedTickLines[0]) {
        throw 'Yaw-control trace row counts or fixed-step schemas differ'
    }
    $tickHeader = $replayedTickLines[0].Split(',')
    $tickIndexColumn = [Array]::IndexOf($tickHeader, 'tick_index')
    $tickStepColumn = [Array]::IndexOf($tickHeader, 'scheduled_step_index')
    $tickStepCountColumn = [Array]::IndexOf($tickHeader, 'scheduled_step_count')
    $tickTimeColumn = [Array]::IndexOf($tickHeader, 'simulation_time')
    $tickSourceColumn = [Array]::IndexOf($tickHeader, 'input_provenance')
    $tickInputColumn = [Array]::IndexOf($tickHeader, 'keyboard_snapshot_hex')
    $tickSteeringInputColumn = [Array]::IndexOf($tickHeader, 'steering')
    $tickThrottleInputColumn = [Array]::IndexOf($tickHeader, 'throttle')
    $tickBrakeInputColumn = [Array]::IndexOf($tickHeader, 'brake')
    $pcarCurveGetterColumn = [Array]::IndexOf(
        $tickHeader, 'pcar_curve_getter_0xc0c')
    $pcarBodySampleColumn = [Array]::IndexOf(
        $tickHeader, 'pcar_body_sample_0xd70')
    $pcarSoundGetterGateColumn = [Array]::IndexOf(
        $tickHeader, 'pcar_sound_getter_gate_0x508a40')
    $pcarSoundGetterColumn = [Array]::IndexOf(
        $tickHeader, 'pcar_sound_getter_0x44_selected_value')
    $bodyTimerColumn = [Array]::IndexOf($tickHeader, 'body_timer_0xe44')
    $bodyWheelPairScaleColumns = @(
        [Array]::IndexOf($tickHeader, 'body_wheel_pair_scale_0xdd4'),
        [Array]::IndexOf($tickHeader, 'body_wheel_pair_scale_0xdd8'))
    $bodyConstructorGeometryColumns = @(
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd7c'),
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd80'),
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd84'),
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd88'),
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd8c'),
        [Array]::IndexOf($tickHeader, 'body_constructor_0xd90'))
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
    $bodyPlacementTransformMatchesColumn = [Array]::IndexOf(
        $tickHeader, 'body_transform_matches_runtime_placement')
    $bodyIntegratorRampColumn = [Array]::IndexOf(
        $tickHeader, 'body_integrator_ramp_0x50c')
    $bodyVehicleOrdinalColumn = [Array]::IndexOf(
        $tickHeader, 'body_vehicle_ordinal_0x510')
    $integratorWheelScalarReachedColumn = [Array]::IndexOf(
        $tickHeader, 'integrator_wheel_scalar_callback_reached')
    $integratorWheelScalarSelectorColumn = [Array]::IndexOf(
        $tickHeader, 'integrator_wheel_scalar_selector_nonzero')
    $integratorWheelScalarResponseGateColumn = [Array]::IndexOf(
        $tickHeader, 'integrator_wheel_scalar_response_gate_passed')
    $integratorWheelScalarNegativePairColumn = [Array]::IndexOf(
        $tickHeader, 'integrator_wheel_scalar_negative_pair_gate_passed')
    $integratorWheelScalarTargetColumn = [Array]::IndexOf(
        $tickHeader, 'integrator_wheel_scalar_target_response')
    $integratorModeForceColumns = @(
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_evaluated'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_supported'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_contact_gate'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_forward_gate'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_accumulated'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_x'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_y'),
        [Array]::IndexOf($tickHeader, 'integrator_mode_force_z'))
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
    $wheelSuspensionResetColumns = [int[]]::new(4)
    $wheelSuspensionForceColumns = @()
    $wheelGateColumns = [int[]]::new(4)
    $wheelDepthColumns = [int[]]::new(4)
    $wheelClampedDepthColumns = [int[]]::new(4)
    $wheelPreupdateColumns = [int[]]::new(4)
    $wheelProducerOwnedColumns = [int[]]::new(4)
    $wheelPriorDepthColumns = [int[]]::new(4)
    $wheelAnimationFlagsColumns = [int[]]::new(4)
    $wheelAnimationModeColumns = [int[]]::new(4)
    $wheelAnimationBasisColumns = @()
    $wheelForwardProjectionColumns = [int[]]::new(4)
    $wheelLateralProjectionColumns = [int[]]::new(4)
    $wheelSlipRatioColumns = [int[]]::new(4)
    $wheelTireCurveColumns = @()
    $wheelTireDynamicsColumns = @()
    $wheelTireResetColumns = @()
    $wheelTireAccumulatorColumns = @()
    $wheelTireVectorColumns = @()
    $wheelRateFinalizeColumns = @()
    $wheelTireForceColumns = @()
    $wheelSpinMomentColumns = @()
    $wheelTireFollowupForceColumns = @()
    $wheelScalarColumns = [int[]]::new(4)
    $wheelSpinColumns = @()
    $wheelQuerySucceededColumns = [int[]]::new(4)
    $wheelContactDepthLimitColumns = [int[]]::new(4)
    $wheelCollisionColumns = @()
    $wheelContactGeometryColumns = @()
    $contactGeometryNames = @(
        'contact_origin', 'query_contact_point',
        'query_contact_normal', 'contact_direction',
        'selected_model_vertex', 'position_relative_to_vertex')
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        $wheelActiveColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_active")
        $wheelSuspensionResetColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_suspension_reset_contact")
        $forceColumns = [int[]]::new(3)
        foreach ($axis in 0..2) {
            $axisName = @('x', 'y', 'z')[$axis]
            $forceColumns[$axis] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_suspension_force_$axisName")
        }
        $wheelSuspensionForceColumns += ,$forceColumns
        $wheelGateColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_gate_passed")
        $wheelDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_projected_depth")
        $wheelClampedDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_clamped_contact_depth")
        $wheelPreupdateColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_observed_value_0x158")
        $wheelProducerOwnedColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_0x158_native_producer_owned")
        $wheelPriorDepthColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_previous_contact_depth_0xe8")
        $wheelAnimationFlagsColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_animation_flags_0x120")
        $wheelAnimationModeColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_animation_mode_0x121")
        $basisColumns = [int[]]::new(8)
        $basisNames = @(
            'basis_angle_0xd4', 'basis_sine_0xb0', 'basis_zero_0xb4',
            'basis_cosine_0xb8', 'basis_offset_0xd8',
            'basis_0x110_x', 'basis_0x110_y', 'basis_0x110_z')
        for ($lane = 0; $lane -lt $basisNames.Count; ++$lane) {
            $basisColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($basisNames[$lane])")
        }
        $wheelAnimationBasisColumns += ,$basisColumns
        $wheelForwardProjectionColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_forward_velocity_projection")
        $wheelLateralProjectionColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_lateral_velocity_projection")
        $wheelSlipRatioColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_slip_direction_ratio_0x74")
        $tireCurveNames = @(
            'tire_curve_input', 'tire_forward_curve_value',
            'tire_lateral_curve_value', 'tire_dynamics_division_valid')
        $tireCurveColumns = [int[]]::new($tireCurveNames.Count)
        for ($lane = 0; $lane -lt $tireCurveNames.Count; ++$lane) {
            $tireCurveColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireCurveNames[$lane])")
        }
        $wheelTireCurveColumns += ,$tireCurveColumns
        $tireDynamicsNames = @(
            'tire_filter_applied', 'tire_filter_0x170',
            'tire_filter_0x174', 'tire_instantaneous_0x180',
            'tire_response_gate_0x11c', 'tire_trigonometric_response')
        $tireDynamicsColumns = [int[]]::new($tireDynamicsNames.Count)
        for ($lane = 0; $lane -lt $tireDynamicsNames.Count; ++$lane) {
            $tireDynamicsColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireDynamicsNames[$lane])")
        }
        $wheelTireDynamicsColumns += ,$tireDynamicsColumns
        $wheelTireResetColumns += [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_tire_filter_reset_applied")
        $tireAccumulatorNames = @(
            'tire_accumulator_applied', 'tire_accumulator_0xa4',
            'tire_accumulator_0x10c', 'tire_state_0xec', 'tire_state_0xf0')
        $tireAccumulatorColumns = [int[]]::new($tireAccumulatorNames.Count)
        for ($lane = 0; $lane -lt $tireAccumulatorNames.Count; ++$lane) {
            $tireAccumulatorColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireAccumulatorNames[$lane])")
        }
        $wheelTireAccumulatorColumns += ,$tireAccumulatorColumns
        $tireVectorNames = @(
            'tire_vectors_applied',
            'tire_vector_0xf4_x', 'tire_vector_0xf4_y', 'tire_vector_0xf4_z',
            'tire_vector_0x100_x', 'tire_vector_0x100_y', 'tire_vector_0x100_z')
        $tireVectorColumns = [int[]]::new($tireVectorNames.Count)
        for ($lane = 0; $lane -lt $tireVectorNames.Count; ++$lane) {
            $tireVectorColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireVectorNames[$lane])")
        }
        $wheelTireVectorColumns += ,$tireVectorColumns
        $wheelRateFinalizeNames = @(
            'wheel_rate_finalized', 'wheel_rate_final_0x124',
            'wheel_rate_final_0x128')
        $wheelRateFinalizeLaneColumns = [int[]]::new($wheelRateFinalizeNames.Count)
        for ($lane = 0; $lane -lt $wheelRateFinalizeNames.Count; ++$lane) {
            $wheelRateFinalizeLaneColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($wheelRateFinalizeNames[$lane])")
        }
        $wheelRateFinalizeColumns += ,$wheelRateFinalizeLaneColumns
        $tireForceNames = @(
            'tire_force_call_reached', 'tire_force_accumulated',
            'tire_force_x', 'tire_force_y', 'tire_force_z')
        $tireForceColumns = [int[]]::new($tireForceNames.Count)
        for ($lane = 0; $lane -lt $tireForceNames.Count; ++$lane) {
            $tireForceColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireForceNames[$lane])")
        }
        $wheelTireForceColumns += ,$tireForceColumns
        $spinMomentNames = @(
            'spin_moment_accumulated', 'spin_moment_x',
            'spin_moment_y', 'spin_moment_z', 'spin_history_0x13c')
        $spinMomentColumns = [int[]]::new($spinMomentNames.Count)
        for ($lane = 0; $lane -lt $spinMomentNames.Count; ++$lane) {
            $spinMomentColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($spinMomentNames[$lane])")
        }
        $wheelSpinMomentColumns += ,$spinMomentColumns
        $tireFollowupForceNames = @(
            'tire_secondary_evaluated', 'tire_secondary_accumulated',
            'tire_secondary_force_x', 'tire_secondary_force_y',
            'tire_secondary_force_z', 'tire_conditional_evaluated',
            'tire_conditional_gate_reached', 'tire_conditional_accumulated',
            'tire_conditional_force_x', 'tire_conditional_force_y',
            'tire_conditional_force_z')
        $tireFollowupForceColumns = [int[]]::new(
            $tireFollowupForceNames.Count)
        for ($lane = 0; $lane -lt $tireFollowupForceNames.Count; ++$lane) {
            $tireFollowupForceColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($tireFollowupForceNames[$lane])")
        }
        $wheelTireFollowupForceColumns += ,$tireFollowupForceColumns
        $wheelScalarColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_value_0xe0")
        $spinColumns = [int[]]::new(5)
        $spinNames = @(
            'spin_rate_0x124', 'spin_auxiliary_0x12c', 'spin_filter_0x17c',
            'spin_response_0x90', 'spin_blend_0x28')
        for ($lane = 0; $lane -lt $spinNames.Count; ++$lane) {
            $spinColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($spinNames[$lane])")
        }
        $wheelSpinColumns += ,$spinColumns
        $wheelQuerySucceededColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_query_succeeded")
        $wheelContactDepthLimitColumns[$wheel] = [Array]::IndexOf(
            $tickHeader, "wheel${wheel}_contact_depth_limit_0x44")
        $collisionNames = @(
            'collision_evaluated', 'collision_response',
            'collision_impulse_accumulated', 'collision_damage_gate_unresolved')
        $collisionColumns = [int[]]::new($collisionNames.Count)
        for ($lane = 0; $lane -lt $collisionNames.Count; ++$lane) {
            $collisionColumns[$lane] = [Array]::IndexOf(
                $tickHeader, "wheel${wheel}_$($collisionNames[$lane])")
        }
        $wheelCollisionColumns += ,$collisionColumns
        $geometryColumns = [int[]]::new(18)
        $geometryColumn = 0
        foreach ($geometryName in $contactGeometryNames) {
            foreach ($axis in @('x', 'y', 'z')) {
                $geometryColumns[$geometryColumn] = [Array]::IndexOf(
                    $tickHeader, "wheel${wheel}_${geometryName}_${axis}")
                ++$geometryColumn
            }
        }
        $wheelContactGeometryColumns += ,$geometryColumns
    }
    if ($tickIndexColumn -lt 0 -or $tickStepColumn -lt 0 -or
        $tickStepCountColumn -lt 0 -or $tickTimeColumn -lt 0 -or
        $tickSourceColumn -lt 0 -or $tickInputColumn -lt 0 -or
        $tickSteeringInputColumn -lt 0 -or $tickThrottleInputColumn -lt 0 -or
        $tickBrakeInputColumn -lt 0 -or
        $pcarCurveGetterColumn -lt 0 -or $pcarBodySampleColumn -lt 0 -or
        $pcarSoundGetterGateColumn -lt 0 -or $pcarSoundGetterColumn -lt 0 -or
        $bodyTimerColumn -lt 0 -or
        @($bodyWheelPairScaleColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        $bodyKinematicsReadyColumn -lt 0 -or
        $bodyKinematicsNonFiniteColumn -lt 0 -or
        $bodyKinematicsMissingCountColumn -lt 0 -or
        @($bodyKinematicsMissingOffsets | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        $bodyIntegratorTimerReadyColumn -lt 0 -or
        $bodyIntegratorTimerFiniteColumn -lt 0 -or
        $bodyIntegratorTimerMissingCountColumn -lt 0 -or
        $bodyPlacementTransformMatchesColumn -lt 0 -or
        $bodyIntegratorRampColumn -lt 0 -or $bodyVehicleOrdinalColumn -lt 0 -or
        $integratorWheelScalarReachedColumn -lt 0 -or
        $integratorWheelScalarSelectorColumn -lt 0 -or
        $integratorWheelScalarResponseGateColumn -lt 0 -or
        $integratorWheelScalarNegativePairColumn -lt 0 -or
        $integratorWheelScalarTargetColumn -lt 0 -or
        @($bodyIntegratorTimerMissingOffsets | Where-Object { $_ -lt 0 }).Count -gt 0) {
        throw 'Fixed-step trace header is missing required input/timing fields'
    }
    if (@($bodyBasisColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($integratorModeForceColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($bodyPositionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($bodyMotionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelActiveColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelSuspensionResetColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelSuspensionForceColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelGateColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelClampedDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelPreupdateColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelProducerOwnedColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelPriorDepthColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelAnimationFlagsColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelAnimationModeColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelAnimationBasisColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelForwardProjectionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelLateralProjectionColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelSlipRatioColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelTireCurveColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelTireDynamicsColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelTireResetColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelTireAccumulatorColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelTireVectorColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelRateFinalizeColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelTireForceColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelSpinMomentColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelTireFollowupForceColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelScalarColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelSpinColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelQuerySucceededColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelContactDepthLimitColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        @($wheelCollisionColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($wheelContactGeometryColumns | ForEach-Object {
            $_ | Where-Object { $_ -lt 0 }
        }).Count -gt 0 -or
        @($bodyConstructorGeometryColumns | Where-Object { $_ -lt 0 }).Count -gt 0) {
        throw 'Fixed-step trace header is missing body or wheel state fields'
    }
    $expectedTick = 0
    $previousOuterUpdate = -1L
    $expectedStepIndex = 0
    $wheelDepthObserved = [bool[]]::new(4)
    $observedScheduledThrottle = $false
    $observedScheduledSteering = $false
    $observedScheduledBrake = $false
    $observedBodyIntegration = $false
    $observedSuspensionForce = $false
    $observedTireCurveDerivation = $false
    $observedTireDynamicsStage = $false
    $observedInactiveTireReset = $false
    $observedTireAccumulatorStage = $false
    $observedTireVectorStage = $false
    $observedWheelRateFinalization = $false
    $observedTireForceAccumulation = $false
    $observedSpinMomentAccumulation = $false
    $observedModeForceDispatch = $false
    $observedSupportedModeForce = $false
    $bodyVerticalVelocityColumn = [Array]::IndexOf($tickHeader, 'body_vel_y')
    for ($index = 1; $index -lt $replayedTickLines.Count; ++$index) {
        if ($replayedTickLines[$index] -cne $repeatedTickLines[$index]) {
            throw "Repeated fixed-step trace differs at CSV line $($index + 1)"
        }
        $fields = $replayedTickLines[$index].Split(',')
        if ($fields.Count -ne $tickHeader.Count) {
            throw "Fixed-step trace field count is invalid at line $($index + 1)"
        }
        $constructorValues = [single[]]::new($bodyConstructorGeometryColumns.Count)
        for ($constructorIndex = 0;
             $constructorIndex -lt $bodyConstructorGeometryColumns.Count;
             ++$constructorIndex) {
            $constructorText = $fields[$bodyConstructorGeometryColumns[$constructorIndex]]
            if ([string]::IsNullOrWhiteSpace($constructorText)) {
                throw "PCAR constructor field +0x$('{0:X}' -f (0xd7c + 4 * $constructorIndex)) is unavailable at line $($index + 1)"
            }
            $constructorValues[$constructorIndex] = [single]::Parse(
                $constructorText, [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($constructorValues[$constructorIndex]) -or
                [single]::IsInfinity($constructorValues[$constructorIndex])) {
                throw "PCAR constructor field +0x$('{0:X}' -f (0xd7c + 4 * $constructorIndex)) is non-finite at line $($index + 1)"
            }
        }
        if ($constructorValues[0] -ne 0.0 -or
            $constructorValues[3] -ne 0.0 -or
            $constructorValues[1] -ne $constructorValues[4]) {
            throw "PCAR constructor body fields do not match recovered 0x433C00 stores at line $($index + 1)"
        }
        if ([string]::IsNullOrWhiteSpace($fields[$pcarCurveGetterColumn]) -or
            [string]::IsNullOrWhiteSpace($fields[$pcarBodySampleColumn]) -or
            [string]::IsNullOrWhiteSpace($fields[$pcarSoundGetterGateColumn]) -or
            [string]::IsNullOrWhiteSpace($fields[$pcarSoundGetterColumn])) {
            throw "PCAR curve getter/body sample is missing at line $($index + 1)"
        }
        $curveGetterValue = [single]::Parse(
            $fields[$pcarCurveGetterColumn], [cultureinfo]::InvariantCulture)
        $bodySampleValue = [single]::Parse(
            $fields[$pcarBodySampleColumn], [cultureinfo]::InvariantCulture)
        $soundGetterValue = [single]::Parse(
            $fields[$pcarSoundGetterColumn], [cultureinfo]::InvariantCulture)
        if ([single]::IsNaN($curveGetterValue) -or
            [single]::IsInfinity($curveGetterValue) -or
            [single]::IsNaN($bodySampleValue) -or
            [single]::IsInfinity($bodySampleValue) -or
            [single]::IsNaN($soundGetterValue) -or
            [single]::IsInfinity($soundGetterValue) -or
            $fields[$pcarSoundGetterGateColumn] -ne '0' -or
            $curveGetterValue -ne $bodySampleValue -or
            $soundGetterValue -ne 0.0) {
            throw "PCAR sound inputs are invalid or the curve callback result does not match the body +0xD70 sample at line $($index + 1)"
        }
        if ($fields[$integratorModeForceColumns[0]] -ne '1') {
            throw "Shared-integrator mode-force branch result is missing at line $($index + 1)"
        }
        $observedModeForceDispatch = $true
        if ($fields[$integratorModeForceColumns[1]] -eq '1') {
            $observedSupportedModeForce = $true
            foreach ($column in $integratorModeForceColumns[5..7]) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                    throw "Supported shared-integrator mode-force output is missing at line $($index + 1)"
                }
                $forceValue = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($forceValue) -or
                    [single]::IsInfinity($forceValue)) {
                    throw "Supported shared-integrator mode-force output is non-finite at line $($index + 1)"
                }
            }
            if ($fields[$integratorModeForceColumns[4]] -eq '1' -and
                ($fields[$integratorModeForceColumns[2]] -ne '1' -or
                 $fields[$integratorModeForceColumns[3]] -ne '1')) {
                throw "Shared-integrator mode force accumulated without both native gates at line $($index + 1)"
            }
        }
        $outerUpdate = [long]::Parse($fields[0], [cultureinfo]::InvariantCulture)
        $tickSteering = [single]::Parse(
            $fields[$tickSteeringInputColumn], [cultureinfo]::InvariantCulture)
        $tickThrottle = [single]::Parse(
            $fields[$tickThrottleInputColumn], [cultureinfo]::InvariantCulture)
        $tickBrake = [single]::Parse(
            $fields[$tickBrakeInputColumn], [cultureinfo]::InvariantCulture)
        if ($outerUpdate -lt 96 -and $tickThrottle -gt 0.2) {
            $observedScheduledThrottle = $true
        }
        if ($outerUpdate -ge 160 -and $outerUpdate -lt 208 -and
            [Math]::Abs($tickSteering) -gt 0.2) {
            $observedScheduledSteering = $true
        }
        if ($outerUpdate -ge 240 -and $outerUpdate -lt 288 -and
            $tickBrake -gt 0.2) {
            $observedScheduledBrake = $true
        }
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
        if ($fields[$integratorWheelScalarReachedColumn] -ne '1' -or
            $fields[$integratorWheelScalarSelectorColumn] -notin @('0', '1') -or
            $fields[$integratorWheelScalarResponseGateColumn] -notin @('0', '1') -or
            $fields[$integratorWheelScalarNegativePairColumn] -notin @('0', '1')) {
            throw "Shared integrator +0x4C callback result is missing at line $($index + 1)"
        }
        $targetResponse = [single]::Parse(
            $fields[$integratorWheelScalarTargetColumn], [cultureinfo]::InvariantCulture)
        if ([single]::IsNaN($targetResponse) -or [single]::IsInfinity($targetResponse)) {
            throw "Shared integrator +0x4C target response is non-finite at line $($index + 1)"
        }
        foreach ($wheel in 0..3) {
            $wheelScalar = [single]::Parse(
                $fields[$wheelScalarColumns[$wheel]], [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($wheelScalar) -or [single]::IsInfinity($wheelScalar)) {
                throw "Wheel $wheel +0xE0 callback output is non-finite at line $($index + 1)"
            }
            foreach ($column in $wheelSpinColumns[$wheel]) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                    throw "Wheel $wheel spin/filter callback output is unavailable at line $($index + 1)"
                }
                $spinValue = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($spinValue) -or [single]::IsInfinity($spinValue)) {
                    throw "Wheel $wheel spin/filter callback output is non-finite at line $($index + 1)"
                }
            }
            foreach ($column in $wheelTireCurveColumns[$wheel][0..2]) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) { continue }
                $observedTireCurveDerivation = $true
                $curveValue = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($curveValue) -or
                    [single]::IsInfinity($curveValue)) {
                    throw "Wheel $wheel tire-curve derivation is non-finite at line $($index + 1)"
                }
            }
            $tireCurveValues = @($wheelTireCurveColumns[$wheel][0..2] |
                ForEach-Object { $fields[$_] })
            $tireCurveStatus = $fields[$wheelTireCurveColumns[$wheel][3]]
            $tireCurveValueCount = @($tireCurveValues | Where-Object {
                -not [string]::IsNullOrWhiteSpace($_)
            }).Count
            if ($tireCurveValueCount -ne 3 -or
                $tireCurveStatus -notin @('0', '1')) {
                throw "Wheel $wheel tire-curve division status is invalid at line $($index + 1)"
            }
            $dynamicsColumns = $wheelTireDynamicsColumns[$wheel]
            $dynamicsApplied = $fields[$dynamicsColumns[0]]
            $wheelContactActive = $fields[$wheelActiveColumns[$wheel]] -eq '1'
            $resetApplied = $fields[$wheelTireResetColumns[$wheel]]
            if (-not $wheelContactActive -and $resetApplied -ne '1') {
                throw "Wheel $wheel inactive-contact tire filter reset did not apply at line $($index + 1)"
            }
            if (-not $wheelContactActive) { $observedInactiveTireReset = $true }
            if ($wheelContactActive -and $tireCurveStatus -eq '1' -and
                $dynamicsApplied -ne '1') {
                throw "Wheel $wheel active-contact tire filter stage did not apply at line $($index + 1)"
            }
            if ($dynamicsApplied -eq '1') {
                $observedTireDynamicsStage = $true
                foreach ($column in $dynamicsColumns[1..5]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel tire filter output is missing at line $($index + 1)"
                    }
                    $filterValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($filterValue) -or
                        [single]::IsInfinity($filterValue)) {
                        throw "Wheel $wheel tire filter output is non-finite at line $($index + 1)"
                    }
                }
            }
            $accumulatorColumns = $wheelTireAccumulatorColumns[$wheel]
            $accumulatorApplied = $fields[$accumulatorColumns[0]]
            if ($dynamicsApplied -eq '1' -and $accumulatorApplied -ne '1') {
                throw "Wheel $wheel active-contact tire accumulator did not apply at line $($index + 1)"
            }
            if ($accumulatorApplied -eq '1') {
                $observedTireAccumulatorStage = $true
                foreach ($column in $accumulatorColumns[1..4]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel tire accumulator output is missing at line $($index + 1)"
                    }
                    $accumulatorValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($accumulatorValue) -or
                        [single]::IsInfinity($accumulatorValue)) {
                        throw "Wheel $wheel tire accumulator output is non-finite at line $($index + 1)"
                    }
                }
            }
            $vectorColumns = $wheelTireVectorColumns[$wheel]
            $vectorsApplied = $fields[$vectorColumns[0]]
            if ($dynamicsApplied -eq '1' -and $vectorsApplied -ne '1') {
                throw "Wheel $wheel active-contact tire response vectors did not apply at line $($index + 1)"
            }
            if ($vectorsApplied -eq '1') {
                $observedTireVectorStage = $true
                foreach ($column in $vectorColumns[1..6]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel tire response vector output is missing at line $($index + 1)"
                    }
                    $vectorValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($vectorValue) -or
                        [single]::IsInfinity($vectorValue)) {
                        throw "Wheel $wheel tire response vector output is non-finite at line $($index + 1)"
                    }
                }
                $tireForceColumns = $wheelTireForceColumns[$wheel]
                if ($fields[$tireForceColumns[0]] -ne '1') {
                    throw "Wheel $wheel corrected tire vector did not reach the 0x43E6F0 force call at line $($index + 1)"
                }
                foreach ($column in $tireForceColumns[2..4]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel tire force vector is missing at line $($index + 1)"
                    }
                    $forceValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($forceValue) -or
                        [single]::IsInfinity($forceValue)) {
                        throw "Wheel $wheel applied tire force is non-finite at line $($index + 1)"
                    }
                }
                if ($fields[$tireForceColumns[1]] -eq '1') {
                    $observedTireForceAccumulation = $true
                }
                $spinMomentFields = $wheelSpinMomentColumns[$wheel]
                if ($spinMomentFields -contains -1 -or
                    $fields[$spinMomentFields[0]] -ne '1') {
                    throw "Wheel $wheel spin moment did not reach the body accumulator at line $($index + 1)"
                }
                foreach ($column in $spinMomentFields[1..3]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel spin moment vector is missing at line $($index + 1)"
                    }
                    $momentValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($momentValue) -or
                        [single]::IsInfinity($momentValue)) {
                        throw "Wheel $wheel spin moment is non-finite at line $($index + 1)"
                    }
                    if ($momentValue -ne 0.0) { $observedSpinMomentAccumulation = $true }
                }
                $spinRate = [single]::Parse(
                    $fields[$wheelSpinColumns[$wheel][0]],
                    [cultureinfo]::InvariantCulture)
                $spinHistory = [single]::Parse(
                    $fields[$spinMomentFields[4]],
                    [cultureinfo]::InvariantCulture)
                if ([Math]::Abs($spinRate - $spinHistory) -gt 0.000001) {
                    throw "Wheel $wheel +0x13C history does not match current +0x124 spin at line $($index + 1)"
                }
                $followupColumns = $wheelTireFollowupForceColumns[$wheel]
                if ($fields[$followupColumns[0]] -ne '1' -or
                    $fields[$followupColumns[5]] -ne '1') {
                    throw "Wheel $wheel tire vector did not reach both later native force calls at line $($index + 1)"
                }
                foreach ($column in @(
                    $followupColumns[2], $followupColumns[3],
                    $followupColumns[4], $followupColumns[8],
                    $followupColumns[9], $followupColumns[10])) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel later tire force vector is missing at line $($index + 1)"
                    }
                    $forceValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($forceValue) -or
                        [single]::IsInfinity($forceValue)) {
                        throw "Wheel $wheel later tire force is non-finite at line $($index + 1)"
                    }
                }
            }
            $rateFinalizeColumns = $wheelRateFinalizeColumns[$wheel]
            $rateFinalized = $fields[$rateFinalizeColumns[0]]
            if ($vectorsApplied -eq '1' -and $rateFinalized -ne '1') {
                throw "Wheel $wheel tire response did not reach final wheel-rate stores at line $($index + 1)"
            }
            if ($rateFinalized -eq '1') {
                $observedWheelRateFinalization = $true
                foreach ($column in $rateFinalizeColumns[1..2]) {
                    if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                        throw "Wheel $wheel final wheel-rate output is missing at line $($index + 1)"
                    }
                    $rateValue = [single]::Parse(
                        $fields[$column], [cultureinfo]::InvariantCulture)
                    if ([single]::IsNaN($rateValue) -or
                        [single]::IsInfinity($rateValue)) {
                        throw "Wheel $wheel final wheel-rate output is non-finite at line $($index + 1)"
                    }
                }
            }
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
        if ($fields[$bodyIntegratorTimerReadyColumn] -ne '1' -or
            $fields[$bodyIntegratorTimerFiniteColumn] -ne '1' -or
            $fields[$bodyIntegratorTimerMissingCountColumn] -ne '0') {
            throw "Viper/Bemidji body integrator constructor lanes are not complete and finite at line $($index + 1)"
        }
        if ($index -eq 1) {
            # The timer/history ring is still disconnected, but the bounded
            # 0x431150 paired differential producer may now own +0x158 when
            # all source fields and contact bytes are initialized.
            if ([string]::IsNullOrWhiteSpace($fields[$bodyIntegratorRampColumn]) -or
                [Math]::Abs([single]::Parse(
                    $fields[$bodyIntegratorRampColumn], [cultureinfo]::InvariantCulture) -
                    1.0) -gt 0.00001 -or
                $fields[$bodyVehicleOrdinalColumn] -ne '7' -or
                $fields[$bodyPlacementTransformMatchesColumn] -ne '1') {
                throw 'First tick does not expose the initialized PCAR +0x50C/+0x510 constructor state'
            }
            for ($wheel = 0; $wheel -lt 4; ++$wheel) {
                $pairHistory = $fields[$wheelPreupdateColumns[$wheel]]
                if ($fields[$wheelProducerOwnedColumns[$wheel]] -ne '1' -or
                    [string]::IsNullOrWhiteSpace($pairHistory) -or
                    [Math]::Abs([single]::Parse(
                        $pairHistory, [cultureinfo]::InvariantCulture)) -gt 0.00001 -or
                    $fields[$wheelPriorDepthColumns[$wheel]] -ne '0' -or
                    $fields[$wheelAnimationFlagsColumns[$wheel]] -ne '0' -or
                    $fields[$wheelAnimationModeColumns[$wheel]] -ne '0') {
                    throw "Wheel $wheel first-tick producer output, inactive contact history, or +0xE8 state differs"
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
        for ($wheel = 0; $wheel -lt 4; ++$wheel) {
            if ($fields[$wheelQuerySucceededColumns[$wheel]] -ne '1') {
                if ($fields[$wheelCollisionColumns[$wheel][0]] -ne '0') {
                    throw "Wheel $wheel collision solver ran without a successful terrain query at line $($index + 1)"
                }
                if ($fields[$wheelActiveColumns[$wheel]] -ne '0' -or
                    $fields[$wheelSuspensionResetColumns[$wheel]] -ne '1') {
                    throw "Wheel $wheel terrain miss did not clear contact and reset suspension at line $($index + 1)"
                }
                continue
            }
            if ($fields[$wheelCollisionColumns[$wheel][0]] -ne '1' -or
                $fields[$wheelCollisionColumns[$wheel][3]] -ne '0') {
                throw "Wheel $wheel collision trace omitted its successful-query evaluation or standard-session gate value at line $($index + 1)"
            }
            $collisionResponseText = $fields[$wheelCollisionColumns[$wheel][1]]
            if (-not [string]::IsNullOrWhiteSpace($collisionResponseText)) {
                $collisionResponse = [single]::Parse(
                    $collisionResponseText, [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($collisionResponse) -or
                    [single]::IsInfinity($collisionResponse)) {
                    throw "Wheel $wheel collision response is non-finite at line $($index + 1)"
                }
            }
            if ($fields[$wheelCollisionColumns[$wheel][2]] -notin @('0', '1')) {
                throw "Wheel $wheel collision impulse flag is invalid at line $($index + 1)"
            }
            $depthLimit = [single]::Parse(
                $fields[$wheelContactDepthLimitColumns[$wheel]],
                [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($depthLimit) -or [single]::IsInfinity($depthLimit)) {
                throw "Wheel $wheel depth limit is non-finite at line $($index + 1)"
            }
            $geometry = [single[]]::new(18)
            for ($lane = 0; $lane -lt 18; ++$lane) {
                $geometry[$lane] = [single]::Parse(
                    $fields[$wheelContactGeometryColumns[$wheel][$lane]],
                    [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($geometry[$lane]) -or
                    [single]::IsInfinity($geometry[$lane])) {
                    throw "Wheel $wheel contact geometry is non-finite at line $($index + 1)"
                }
            }
            $recomputedDepth =
                ($geometry[3] - $geometry[0]) * $geometry[9] +
                ($geometry[4] - $geometry[1]) * $geometry[10] +
                ($geometry[5] - $geometry[2]) * $geometry[11]
            $measuredDepth = [single]::Parse(
                $fields[$wheelDepthColumns[$wheel]],
                [cultureinfo]::InvariantCulture)
            if ($fields[$wheelGateColumns[$wheel]] -eq '1' -and
                [Math]::Abs($recomputedDepth - $measuredDepth) -gt 0.00001) {
                throw "Wheel $wheel projected depth differs from its recorded query geometry at line $($index + 1)"
            }
            $localAnchor = [single[]]::new(3)
            for ($axis = 0; $axis -lt 3; ++$axis) {
                $localAnchor[$axis] = $geometry[12 + $axis] + $geometry[15 + $axis]
            }
            $bodyPosition = [single[]]::new(3)
            for ($axis = 0; $axis -lt 3; ++$axis) {
                $bodyPosition[$axis] = [single]::Parse(
                    $fields[$bodyPositionColumns[$axis]],
                    [cultureinfo]::InvariantCulture)
            }
            $expectedOrigin = [single[]]::new(3)
            for ($axis = 0; $axis -lt 3; ++$axis) {
                $expectedOrigin[$axis] = $bodyPosition[$axis]
                for ($localAxis = 0; $localAxis -lt 3; ++$localAxis) {
                    $expectedOrigin[$axis] += [single]::Parse(
                        $fields[$bodyBasisColumns[$localAxis * 3 + $axis]],
                        [cultureinfo]::InvariantCulture) * $localAnchor[$localAxis]
                }
            }
            for ($axis = 0; $axis -lt 3; ++$axis) {
                if ([Math]::Abs($expectedOrigin[$axis] - $geometry[$axis]) -gt 0.00002) {
                    throw "Wheel $wheel contact origin differs from constructor anchor transformed by byte-backed body pose at line $($index + 1)"
                }
            }
            if ($measuredDepth -lt 0.0) {
                $clampedDepth = [single]::Parse(
                    $fields[$wheelClampedDepthColumns[$wheel]],
                    [cultureinfo]::InvariantCulture)
                if ($fields[$wheelActiveColumns[$wheel]] -ne '0' -or
                    $clampedDepth -ne 0.0) {
                    throw "Wheel $wheel negative projection did not remain inactive at line $($index + 1)"
                }
            } elseif ($fields[$wheelGateColumns[$wheel]] -eq '1' -and
                $fields[$wheelActiveColumns[$wheel]] -ne '1' -and
                $fields[$wheelSuspensionResetColumns[$wheel]] -ne '1') {
                throw "Wheel $wheel valid nonnegative contact did not become active at line $($index + 1)"
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
        if ($tickIndex -ge 1) {
            $verticalVelocity = [single]::Parse(
                $fields[$bodyVerticalVelocityColumn], [cultureinfo]::InvariantCulture)
            if ($verticalVelocity -lt -0.001) { $observedBodyIntegration = $true }
        }
        for ($wheel = 0; $wheel -lt 4; ++$wheel) {
            $suspensionForceSquared = 0.0
            foreach ($column in $wheelSuspensionForceColumns[$wheel]) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                    throw "Wheel $wheel finalized suspension force is unavailable at line $($index + 1)"
                }
                $forceComponent = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($forceComponent) -or
                    [single]::IsInfinity($forceComponent)) {
                    throw "Wheel $wheel finalized suspension force is non-finite at line $($index + 1)"
                }
                $suspensionForceSquared += $forceComponent * $forceComponent
            }
            if ($suspensionForceSquared -gt 0.000001) {
                $observedSuspensionForce = $true
            }
            if ($fields[$wheelProducerOwnedColumns[$wheel]] -notin @('0', '1')) {
                throw "Wheel $wheel +0x158 producer-ownership flag is invalid"
            }
            $active = $fields[$wheelActiveColumns[$wheel]]
            if (-not [string]::IsNullOrWhiteSpace($active) -and $active -notin @('0', '1')) {
                throw "Wheel $wheel contact-active value is invalid at line $($index + 1)"
            }
            if ($active -eq '0' -and
                ($fields[$wheelAnimationFlagsColumns[$wheel]] -ne '0' -or
                 $fields[$wheelAnimationModeColumns[$wheel]] -ne '0')) {
                throw "Wheel $wheel inactive-contact feature outputs were not cleared at line $($index + 1)"
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
            foreach ($column in $wheelAnimationBasisColumns[$wheel]) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                    throw "Wheel $wheel byte-backed animation basis output is unavailable at line $($index + 1)"
                }
                $basisValue = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($basisValue) -or [single]::IsInfinity($basisValue)) {
                    throw "Wheel $wheel byte-backed animation basis output is non-finite at line $($index + 1)"
                }
            }
            foreach ($column in @(
                $wheelForwardProjectionColumns[$wheel],
                $wheelLateralProjectionColumns[$wheel])) {
                if ([string]::IsNullOrWhiteSpace($fields[$column])) {
                    throw "Wheel $wheel velocity projection is unavailable at line $($index + 1)"
                }
                $projection = [single]::Parse(
                    $fields[$column], [cultureinfo]::InvariantCulture)
                if ([single]::IsNaN($projection) -or [single]::IsInfinity($projection)) {
                    throw "Wheel $wheel velocity projection is non-finite at line $($index + 1)"
                }
            }
            $slipRatioText = $fields[$wheelSlipRatioColumns[$wheel]]
            if ([string]::IsNullOrWhiteSpace($slipRatioText)) {
                throw "Wheel $wheel source-derived tire slip ratio is unavailable at line $($index + 1)"
            }
            $slipRatio = [single]::Parse(
                $slipRatioText, [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($slipRatio) -or [single]::IsInfinity($slipRatio)) {
                throw "Wheel $wheel source-derived tire slip ratio is non-finite at line $($index + 1)"
            }
        }
        ++$expectedTick
    }
    if ($wheelDepthObserved -contains $false) {
        throw 'Fixed-step trace did not capture projected depth for all four wheels'
    }
    $header = $replayedLines[0].Split(',')
    $diagnosticsOnlyColumn = [Array]::IndexOf($header, 'diagnostics_only')
    $routeMotionStepColumn = [Array]::IndexOf($header, 'route_motion_step_count')
    $playerBodyVelocityColumns = @(
        [Array]::IndexOf($header, 'player_body_velocity_x'),
        [Array]::IndexOf($header, 'player_body_velocity_y'),
        [Array]::IndexOf($header, 'player_body_velocity_z'))
    $playerHudSpeedColumn = [Array]::IndexOf($header, 'player_hud_speed_kmh')
    if ($diagnosticsOnlyColumn -lt 0 -or $routeMotionStepColumn -lt 0 -or
        @($playerBodyVelocityColumns | Where-Object { $_ -lt 0 }).Count -gt 0 -or
        $playerHudSpeedColumn -lt 0) {
        throw 'Trace omitted fields needed to prove diagnostics-only mode'
    }
    foreach ($line in $replayedLines | Select-Object -Skip 1) {
        $fields = $line.Split(',')
        if ($fields[$diagnosticsOnlyColumn] -ne '1') {
            throw "Trace row $($fields[0]) diagnostics-only marker is '$($fields[$diagnosticsOnlyColumn])'"
        }
        if ([uint32]::Parse($fields[$routeMotionStepColumn],
                [cultureinfo]::InvariantCulture) -ne 0) {
            throw 'Diagnostics-only replay called the ideal-line route motion adapter'
        }
        $bodyVelocity = [single[]]::new(3)
        for ($axis = 0; $axis -lt 3; ++$axis) {
            $bodyVelocity[$axis] = [single]::Parse(
                $fields[$playerBodyVelocityColumns[$axis]],
                [cultureinfo]::InvariantCulture)
            if ([single]::IsNaN($bodyVelocity[$axis]) -or
                [single]::IsInfinity($bodyVelocity[$axis])) {
                throw "Route-disabled HUD body velocity is non-finite at frame $($fields[0])"
            }
        }
        $hudSpeed = [single]::Parse(
            $fields[$playerHudSpeedColumn], [cultureinfo]::InvariantCulture)
        $bodySpeed = [Math]::Sqrt(
            [double]$bodyVelocity[0] * $bodyVelocity[0] +
            [double]$bodyVelocity[1] * $bodyVelocity[1] +
            [double]$bodyVelocity[2] * $bodyVelocity[2])
        if ([single]::IsNaN($hudSpeed) -or [single]::IsInfinity($hudSpeed) -or
            [Math]::Abs($hudSpeed - $bodySpeed * 3.6) -gt 0.0001) {
            throw "Route-disabled HUD speed does not follow the live PCAR body velocity at frame $($fields[0])"
        }
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
    if (-not $observedScheduledThrottle -or -not $observedScheduledSteering -or
        -not $observedScheduledBrake) {
        throw "Authored input phases did not reach recovered PCAR controls: throttle=$observedScheduledThrottle steering=$observedScheduledSteering brake=$observedScheduledBrake"
    }
    if (-not $observedBodyIntegration) {
        throw 'Fixed-step replay did not transfer gravity, suspension, and tire force into body velocity'
    }
    if (-not $observedSuspensionForce) {
        throw 'Fixed-step replay never produced a nonzero finalized wheel suspension force'
    }
    if (-not $observedTireCurveDerivation) {
        throw 'Fixed-step replay never produced live tire-curve factors for all four wheels'
    }
    if (-not $observedTireDynamicsStage) {
        throw 'Fixed-step replay never entered the finite active-contact tire-filter stage'
    }
    if (-not $observedInactiveTireReset) {
        throw 'Fixed-step replay never entered the inactive-contact tire-filter reset stage'
    }
    if (-not $observedTireAccumulatorStage) {
        throw 'Fixed-step replay never entered the finite active-contact tire accumulator stage'
    }
    if (-not $observedTireVectorStage) {
        throw 'Fixed-step replay never entered the finite active-contact tire response vector stage'
    }
    if (-not $observedWheelRateFinalization) {
        throw 'Fixed-step replay never entered finite final wheel-rate stores'
    }
    if (-not $observedTireForceAccumulation) {
        throw 'Fixed-step replay never accumulated a nonzero tire force into body force/moment state'
    }
    if (-not $observedSpinMomentAccumulation) {
        throw 'Fixed-step replay never accumulated a nonzero wheel spin moment into body state'
    }
    if (-not $observedModeForceDispatch) {
        throw 'Fixed-step replay did not record the shared-integrator mode-force dispatch'
    }

    # Opposite driver steering must reach wheel +0xDC (the recovered input to
    # the wheel-basis updater) and produce different body motion. Route motion
    # stays disabled in both runs, so this cannot pass through ideal-line pose
    # advancement.
    $rightTickHeader = $rightReplayedTickLines[0].Split(',')
    $rightSteeringColumn = [Array]::IndexOf($rightTickHeader, 'steering')
    $rightWheelSteeringColumn = [Array]::IndexOf(
        $rightTickHeader, 'wheel0_steering_0xdc')
    $rightBodyMotionColumns = @(
        [Array]::IndexOf($rightTickHeader, 'body_pos_x'),
        [Array]::IndexOf($rightTickHeader, 'body_pos_z'),
        [Array]::IndexOf($rightTickHeader, 'body_ang_y'))
    if ($rightSteeringColumn -lt 0 -or $rightWheelSteeringColumn -lt 0 -or
        $rightBodyMotionColumns -contains -1) {
        throw 'Mirrored steering replay omitted driver, wheel-steering, or body-motion trace fields'
    }
    $observedNegativeWheelSteering = $false
    $observedPositiveWheelSteering = $false
    $oppositeSteeringChangedBodyMotion = $false
    for ($index = 1; $index -lt $replayedTickLines.Count; ++$index) {
        $leftFields = $replayedTickLines[$index].Split(',')
        $rightFields = $rightReplayedTickLines[$index].Split(',')
        $leftSteering = [single]::Parse(
            $leftFields[$rightSteeringColumn], [cultureinfo]::InvariantCulture)
        $rightSteering = [single]::Parse(
            $rightFields[$rightSteeringColumn], [cultureinfo]::InvariantCulture)
        if ($leftSteering -lt -0.05 -and $rightSteering -gt 0.05) {
            $leftWheelSteering = [single]::Parse(
                $leftFields[$rightWheelSteeringColumn], [cultureinfo]::InvariantCulture)
            $rightWheelSteering = [single]::Parse(
                $rightFields[$rightWheelSteeringColumn], [cultureinfo]::InvariantCulture)
            if ($leftWheelSteering -lt -0.0001) { $observedNegativeWheelSteering = $true }
            if ($rightWheelSteering -gt 0.0001) { $observedPositiveWheelSteering = $true }
            foreach ($column in $rightBodyMotionColumns) {
                $leftValue = [single]::Parse(
                    $leftFields[$column], [cultureinfo]::InvariantCulture)
                $rightValue = [single]::Parse(
                    $rightFields[$column], [cultureinfo]::InvariantCulture)
                if ([Math]::Abs($leftValue - $rightValue) -gt 0.001) {
                    $oppositeSteeringChangedBodyMotion = $true
                }
            }
        }
    }
    if (-not $observedNegativeWheelSteering -or -not $observedPositiveWheelSteering) {
        throw "Mirrored driver steering did not produce opposite wheel +0xDC values: left=$observedNegativeWheelSteering right=$observedPositiveWheelSteering"
    }
    if (-not $oppositeSteeringChangedBodyMotion) {
        throw 'Opposite wheel steering did not change route-free body motion'
    }
    foreach ($line in $rightReplayedLines | Select-Object -Skip 1) {
        $fields = $line.Split(',')
        if ([uint32]::Parse($fields[$routeMotionStepColumn],
                [cultureinfo]::InvariantCulture) -ne 0) {
            throw 'Mirrored diagnostics-only replay called the ideal-line route motion adapter'
        }
    }
    $yawTickHeader = $yawReplayedTickLines[0].Split(',')
    $yawSelectorColumn = [Array]::IndexOf(
        $yawTickHeader, 'integrator_wheel_scalar_selector_nonzero')
    $yawTargetColumn = [Array]::IndexOf(
        $yawTickHeader, 'integrator_wheel_scalar_target_response')
    $yawBodyAngularVelocityColumn = [Array]::IndexOf(
        $yawTickHeader, 'body_ang_y')
    $yawBodyPositionColumn = [Array]::IndexOf($yawTickHeader, 'body_pos_x')
    if ($yawSelectorColumn -lt 0 -or $yawTargetColumn -lt 0 -or
        $yawBodyPositionColumn -lt 0 -or $yawBodyAngularVelocityColumn -lt 0) {
        throw 'Yaw-control replay omitted selector, response, or body-motion fields'
    }
    $yawBodyPositions = [System.Collections.Generic.List[single]]::new()
    $yawSteeringAngularVelocities = [System.Collections.Generic.List[single]]::new()
    for ($index = 1; $index -lt $yawReplayedTickLines.Count; ++$index) {
        $fields = $yawReplayedTickLines[$index].Split(',')
        if ($fields[$yawSelectorColumn] -cne '1') {
            throw "Yaw-control selector did not remain active at physics tick $($index - 1)"
        }
        $targetResponse = [single]::Parse(
            $fields[$yawTargetColumn], [cultureinfo]::InvariantCulture)
        if ([single]::IsNaN($targetResponse) -or
            [single]::IsInfinity($targetResponse)) {
            throw "Yaw-control response was non-finite at physics tick $($index - 1)"
        }
        $yawBodyPositions.Add([single]::Parse(
            $fields[$yawBodyPositionColumn], [cultureinfo]::InvariantCulture))
        $steering = [single]::Parse(
            $fields[$tickSteeringInputColumn], [cultureinfo]::InvariantCulture)
        if ([Math]::Abs($steering) -gt 0.05) {
            $yawSteeringAngularVelocities.Add([single]::Parse(
                $fields[$yawBodyAngularVelocityColumn],
                [cultureinfo]::InvariantCulture))
        }
    }
    $yawBodyRange = ($yawBodyPositions | Measure-Object -Minimum -Maximum)
    if (($yawBodyRange.Maximum - $yawBodyRange.Minimum) -lt 0.001) {
        throw 'Yaw-control route-free replay did not advance physical body position'
    }
    if ($yawSteeringAngularVelocities.Count -eq 0) {
        throw 'Yaw-control replay did not sample the authored steering interval'
    }
    $yawRateRange = ($yawSteeringAngularVelocities | Measure-Object -Minimum -Maximum)
    if (($yawRateRange.Maximum - $yawRateRange.Minimum) -lt 0.001 -and
        [Math]::Abs($yawSteeringAngularVelocities[$yawSteeringAngularVelocities.Count - 1]) -lt 0.001) {
        throw 'Yaw-control steering interval produced no measurable body angular response'
    }
    $yawFrameHeader = $yawReplayedLines[0].Split(',')
    $yawRouteMotionColumn = [Array]::IndexOf(
        $yawFrameHeader, 'route_motion_step_count')
    if ($yawRouteMotionColumn -lt 0) {
        throw 'Yaw-control replay omitted the route-motion acceptance field'
    }
    foreach ($line in $yawReplayedLines | Select-Object -Skip 1) {
        $fields = $line.Split(',')
        if ([uint32]::Parse($fields[$yawRouteMotionColumn],
                [cultureinfo]::InvariantCulture) -ne 0) {
            throw 'Yaw-control diagnostics-only replay called the ideal-line route motion adapter'
        }
    }

    $modeForceSummary = if ($observedSupportedModeForce) {
        'supported mode-force outputs were finite and gate-checked'
    } else {
        'mode-force branch was traced as unsupported for this race mode'
    }
    Write-Output "Authored throttle/coast/steering/brake/coast schedule replayed identically across $($replayedLines.Count - 1) frames; mirrored steering reached opposite wheel +0xDC values and changed body motion. The yaw_control=1 route-free replay completed with finite wheel-response values, physical body movement, and measurable body angular response during steering. Route motion stayed at zero in all cases. Body gravity, suspension, and all three reconstructed tire-force call sites reached integration. The $modeForceSummary. Terrain misses cleared contact and reset suspension deterministically."
}
finally {
    Remove-Item -LiteralPath $capture, $rightCapture, $replay, $rightReplay,
        $yawReplay, $repeat, $replayTicks, $rightReplayTicks, $yawReplayTicks,
        $repeatTicks -Force `
        -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $yawDataDir) {
        $rootFullPath = [System.IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
        $yawDataFullPath = [System.IO.Path]::GetFullPath($yawDataDir)
        if ($yawDataFullPath.StartsWith(
                $rootFullPath, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $yawDataFullPath -Recurse -Force `
                -ErrorAction SilentlyContinue
        }
    }
    Remove-Item -LiteralPath $root -Force -ErrorAction SilentlyContinue
}
