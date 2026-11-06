param(
    [Parameter(Mandatory = $true)]
    [string]$TickCsv,
    [switch]$All
)

$statusNames = @(
    'not_attempted',
    'inactive_contact',
    'missing_state',
    'non_finite_state',
    'non_finite_input',
    'zero_divisor',
    'non_finite_output',
    'write_rejected',
    'applied'
)
$rows = @(Import-Csv -LiteralPath $TickCsv)
if ($rows.Count -eq 0) { throw "Tick trace is empty: $TickCsv" }

$required = @('tick_index')
for ($wheel = 0; $wheel -lt 4; ++$wheel) {
    $required += @(
        "wheel${wheel}_contact_active",
        "wheel${wheel}_tire_filter_applied",
        "wheel${wheel}_tire_dynamics_division_valid",
        "wheel${wheel}_tire_accumulator_applied",
        "wheel${wheel}_tire_accumulator_status",
        "wheel${wheel}_tire_curve_input",
        "wheel${wheel}_tire_lateral_curve_value",
        "wheel${wheel}_tire_response_gate_0x11c"
    )
}
$columns = @($rows[0].PSObject.Properties.Name)
$missing = @($required | Where-Object { $_ -notin $columns })
if ($missing.Count -gt 0) {
    throw "Tick trace lacks required fields: $($missing -join ', ')"
}

$skips = [System.Collections.Generic.List[object]]::new()
foreach ($row in $rows) {
    for ($wheel = 0; $wheel -lt 4; ++$wheel) {
        $prefix = "wheel${wheel}_"
        $active = $row."${prefix}contact_active" -eq '1'
        $dynamicsApplied = $row."${prefix}tire_filter_applied" -eq '1'
        $accumulatorApplied = $row."${prefix}tire_accumulator_applied" -eq '1'
        $status = [int]::Parse(
            $row."${prefix}tire_accumulator_status",
            [cultureinfo]::InvariantCulture)
        if ($active -and $dynamicsApplied -and -not $accumulatorApplied) {
            if ($status -lt 0 -or $status -ge $statusNames.Count) {
                $statusName = "unknown_status_$status"
            } else {
                $statusName = $statusNames[$status]
            }
            $skips.Add([pscustomobject]@{
                tick_index = $row.tick_index
                wheel = $wheel
                status = $statusName
                dynamics_division_valid = $row."${prefix}tire_dynamics_division_valid"
                curve_input = $row."${prefix}tire_curve_input"
                lateral_curve_value = $row."${prefix}tire_lateral_curve_value"
                response_gate_0x11c = $row."${prefix}tire_response_gate_0x11c"
            })
        }
    }
}

if ($skips.Count -eq 0) {
    Write-Output 'No active-contact tire accumulator skips followed an applied dynamics stage.'
    exit 0
}
if ($All) {
    $skips | Format-Table -AutoSize
} else {
    $skips | Select-Object -First 12 | Format-Table -AutoSize
    if ($skips.Count -gt 12) {
        Write-Output "Showing 12 of $($skips.Count) skips; use -All to show the rest."
    }
}
