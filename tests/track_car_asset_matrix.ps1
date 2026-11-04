param(
    [Parameter(Mandatory = $true)]
    [string] $GameExe,
    [Parameter(Mandatory = $true)]
    [string] $DataDir
)

$tracks = @(
    'bemidji', 'dundas', 'hastings', 'heaven',
    'kenyon', 'limbo', 'nfield', 'uptown'
)
$cars = @('viper', 'exotic', 'plane', 'sedan', 'sports')

if (-not (Test-Path -LiteralPath $GameExe -PathType Leaf)) {
    throw "Game executable does not exist: $GameExe"
}
if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) {
    throw "External game data directory does not exist: $DataDir"
}

$validated = 0
$runtime_started = 0
foreach ($track in $tracks) {
    foreach ($car in $cars) {
        $output = @(& $GameExe --data-dir $DataDir --track $track --car $car --validate-assets 2>&1)
        $exitCode = $LASTEXITCODE
        $text = $output -join [Environment]::NewLine
        if ($exitCode -ne 0) {
            throw "Asset validation failed for $track/$car (exit $exitCode):`n$text"
        }

        $escapedTrack = [Regex]::Escape($track)
        $escapedCar = [Regex]::Escape($car)
        $requiredOutput = @(
            "(?m)^Loaded\s+$escapedCar\.car:",
            "(?m)^Loaded\s+$escapedTrack\.trk:",
            "(?m)^Parsed\s+$escapedCar\s+CARF:",
            "(?m)^Parsed\s+$escapedTrack\s+terrain:",
            '(?m)^Required assets validated in '
        )
        foreach ($pattern in $requiredOutput) {
            if ($text -notmatch $pattern) {
                throw "Asset validation output for $track/$car omitted '$pattern':`n$text"
            }
        }
        ++$validated

        # Asset validation alone does not prove that a selected car/track pair
        # can be materialized into the route-disabled race session. Run a short,
        # silent WARP session and require at least one finite body-physics tick.
        $suffix = [Guid]::NewGuid().ToString('N')
        $tickTrace = Join-Path -Path ([IO.Path]::GetTempPath()) `
            -ChildPath "viper-matrix-$track-$car-$suffix-ticks.csv"
        $frameTrace = Join-Path -Path ([IO.Path]::GetTempPath()) `
            -ChildPath "viper-matrix-$track-$car-$suffix-frames.csv"
        $runtimeOutput = @(& $GameExe --data-dir $DataDir --track $track --car $car `
            --diagnostics-only --smoke-frames 10 --software `
            --record-tick-trace $tickTrace --record-trace $frameTrace 2>&1)
        $runtimeExit = $LASTEXITCODE
        $runtimeText = $runtimeOutput -join [Environment]::NewLine
        if ($runtimeExit -ne 0) {
            throw "Route-disabled runtime startup failed for $track/$car (exit $runtimeExit):`n$runtimeText"
        }
        if (-not $runtimeText.Contains('Diagnostics-only mode: route motion disabled.')) {
            throw "Runtime smoke for $track/$car did not confirm route motion is disabled:`n$runtimeText"
        }
        if (-not (Test-Path -LiteralPath $tickTrace -PathType Leaf) -or
            -not (Test-Path -LiteralPath $frameTrace -PathType Leaf)) {
            throw "Runtime smoke for $track/$car did not produce both requested traces."
        }

        $tickRows = @(Import-Csv -LiteralPath $tickTrace)
        if ($tickRows.Count -eq 0) {
            throw "Runtime smoke for $track/$car produced no fixed-step physics ticks."
        }
        foreach ($row in $tickRows) {
            if ($row.body_kinematics_input_ready -ne '1' -or
                $row.body_kinematics_non_finite -ne '0' -or
                $row.body_kinematics_missing_float_count -ne '0') {
                throw "Runtime smoke for $track/$car produced an incomplete/non-finite body tick at index $($row.tick_index)."
            }
        }

        $frameRows = @(Import-Csv -LiteralPath $frameTrace)
        if ($frameRows.Count -eq 0 -or
            @($frameRows | Where-Object {
                $_.diagnostics_only -ne '1' -or
                $_.route_motion_step_count -ne '0'
            }).Count -ne 0) {
            throw "Runtime smoke for $track/$car produced an empty trace or used route motion."
        }
        [IO.File]::Delete($tickTrace)
        [IO.File]::Delete($frameTrace)
        ++$runtime_started
    }
}

Write-Output "Validated assets and started route-disabled physics sessions for all $validated shipped track/car combinations ($runtime_started runtime smokes). Each run produced finite body-physics ticks with ideal-line route motion disabled. This checks construction and tick readiness, not powered driving, original parity, or complete races."
