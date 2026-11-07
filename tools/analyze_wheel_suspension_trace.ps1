param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $Path
)

$ErrorActionPreference = 'Stop'
$rows = Import-Csv -LiteralPath $Path
if ($rows.Count -eq 0) {
    throw "Trace has no data rows: $Path"
}

$invariant = [System.Globalization.CultureInfo]::InvariantCulture
foreach ($wheel in 0..3) {
    $prefix = "wheel${wheel}_"
    $required = @(
        "${prefix}suspension_reset_contact",
        "${prefix}contact_gate_passed",
        "${prefix}contact_query_succeeded",
        "${prefix}contact_direction_x",
        "${prefix}contact_direction_y",
        "${prefix}contact_direction_z",
        "${prefix}query_contact_normal_x",
        "${prefix}query_contact_normal_y",
        "${prefix}query_contact_normal_z",
        "${prefix}projected_depth",
        "${prefix}surface_adjusted_response_0x178"
    )
    foreach ($column in $required) {
        if (-not $rows[0].PSObject.Properties[$column]) {
            throw "Trace is missing required column '$column'."
        }
    }

    $totals = [ordered]@{
        reset = 0
        contact_query_not_succeeded = 0
        normal_alignment_rejected = 0
        other_contact_gate_rejection = 0
        negative_projected_depth = 0
        negative_response_with_valid_contact = 0
        unclassified_or_inconsistent = 0
    }
    foreach ($row in $rows) {
        if ($row."${prefix}suspension_reset_contact" -ne '1') { continue }
        $totals.reset++

        if ($row."${prefix}contact_gate_passed" -ne '1') {
            if ($row."${prefix}contact_query_succeeded" -ne '1') {
                $totals.contact_query_not_succeeded++
                continue
            }
            $direction = @(
                [double]$row."${prefix}contact_direction_x",
                [double]$row."${prefix}contact_direction_y",
                [double]$row."${prefix}contact_direction_z"
            )
            $normal = @(
                [double]$row."${prefix}query_contact_normal_x",
                [double]$row."${prefix}query_contact_normal_y",
                [double]$row."${prefix}query_contact_normal_z"
            )
            $alignment = 0.0
            for ($axis = 0; $axis -lt 3; $axis++) {
                $alignment += $direction[$axis] * $normal[$axis]
            }
            if ($alignment -le 0.1) {
                $totals.normal_alignment_rejected++
            }
            else {
                $totals.other_contact_gate_rejection++
            }
            continue
        }

        $depth = 0.0
        if (-not [double]::TryParse(
                $row."${prefix}projected_depth",
                [System.Globalization.NumberStyles]::Float,
                $invariant,
                [ref] $depth)) {
            $totals.unclassified_or_inconsistent++
            continue
        }
        if ($depth -lt 0.0) {
            $totals.negative_projected_depth++
            continue
        }

        $response = 0.0
        if ([double]::TryParse(
                $row."${prefix}surface_adjusted_response_0x178",
                [System.Globalization.NumberStyles]::Float,
                $invariant,
                [ref] $response) -and $response -lt 0.0) {
            $totals.negative_response_with_valid_contact++
        }
        else {
            $totals.unclassified_or_inconsistent++
        }
    }

    [pscustomobject](@{ wheel = $wheel } + $totals)
}
