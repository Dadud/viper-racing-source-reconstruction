param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [Parameter(Mandatory = $true)][string]$DataDir
)

$ErrorActionPreference = 'Stop'
$installRoot = Split-Path -Parent $DataDir
$isolatedRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    "viper_asset_discovery_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $isolatedRoot | Out-Null

function Invoke-Game([string]$WorkingDirectory, [string]$Arguments,
                     [string]$EnvironmentDataDir) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $GameExe
    $startInfo.Arguments = $Arguments
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    [void]$startInfo.EnvironmentVariables.Remove('VIPER_DATA_DIR')
    if ($null -ne $EnvironmentDataDir) {
        $startInfo.EnvironmentVariables['VIPER_DATA_DIR'] = $EnvironmentDataDir
    }
    $process = [Diagnostics.Process]::Start($startInfo)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Output = $stdout + $stderr
    }
}

try {
    $discovered = Invoke-Game $installRoot '--validate-assets' $null
    if ($discovered.ExitCode -ne 0) {
        throw "Install-relative discovery failed ($($discovered.ExitCode)): $($discovered.Output)"
    }

    $fromEnvironment = Invoke-Game $isolatedRoot '--validate-assets' $DataDir
    if ($fromEnvironment.ExitCode -ne 0) {
        throw "VIPER_DATA_DIR discovery failed ($($fromEnvironment.ExitCode)): $($fromEnvironment.Output)"
    }

    $badEnvironment = Join-Path $isolatedRoot 'missing-assets'
    $fromCommandLine = Invoke-Game $isolatedRoot `
        "--data-dir `"$DataDir`" --validate-assets" $badEnvironment
    if ($fromCommandLine.ExitCode -ne 0) {
        throw "Explicit --data-dir did not take precedence over VIPER_DATA_DIR ($($fromCommandLine.ExitCode)): $($fromCommandLine.Output)"
    }

    $missing = Invoke-Game $isolatedRoot '--validate-assets' $null
    if ($missing.ExitCode -ne 2 -or
        $missing.Output -notmatch 'common\.res' -or
        $missing.Output -notmatch '--data-dir' -or
        $missing.Output -notmatch 'VIPER_DATA_DIR' -or
        $missing.Output -notmatch [regex]::Escape($isolatedRoot)) {
        throw "Missing-asset diagnostic was not actionable ($($missing.ExitCode)): $($missing.Output)"
    }

    Write-Output 'Asset-root discovery passed: install-relative GameData, VIPER_DATA_DIR, explicit CLI precedence, and missing-asset diagnostics.'
} finally {
    Remove-Item -LiteralPath $isolatedRoot -Recurse -Force -ErrorAction SilentlyContinue
}
