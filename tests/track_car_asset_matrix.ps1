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
    }
}

Write-Output "Validated all $validated shipped track/car asset combinations. This checks resource compatibility only; it does not establish vehicle physics or driveability."
