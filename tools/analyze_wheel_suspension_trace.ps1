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
        contact_gate_not_passed = 0
        negative_projected_depth = 0
        negative_response_with_valid_contact = 0
        unclassified_or_inconsistent = 0
    }
    foreach ($row in $rows) {
        if ($row."${prefix}suspension_reset_contact" -ne '1') { continue }
        $totals.reset++

        if ($row."${prefix}contact_gate_passed" -ne '1') {
            $totals.contact_gate_not_passed++
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
