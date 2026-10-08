# Run only inside the bounded Windows x64 VM. The host operator supplies the
# broker's console-only random pipe name and capability. Neither is persisted.
param(
    [Parameter(Mandatory=$true)][string]$PipeName,
    [Parameter(Mandatory=$true)][string]$Token,
    [Parameter(Mandatory=$true)][string]$ProductExe,
    [Parameter(Mandatory=$true)][string]$BrokerExe,
    [string]$ResultPath,
    [string]$ResultUrl
)

$ErrorActionPreference = 'Stop'
if ($Token -notmatch '^[0-9a-f]{64}$' -or
    $PipeName -notmatch '^\\\\\.\\pipe\\uwvm-debug-host-[0-9a-f]{32}$') {
    throw 'expected the broker console-issued pipe and 256-bit capability'
}
$productSha256 = (Get-FileHash $ProductExe -Algorithm SHA256).Hash.ToLowerInvariant()
$brokerSha256 = (Get-FileHash $BrokerExe -Algorithm SHA256).Hash.ToLowerInvariant()

$wrong = @('ping') | & $BrokerExe connect $PipeName ('0' * 64) 2>&1 | Out-String
$wrongExit = $LASTEXITCODE
Start-Sleep -Milliseconds 250
$scheduled = @("attempt-descendant $PipeName $Token") |
    & $BrokerExe connect $PipeName $Token 2>&1 | Out-String
$scheduleExit = $LASTEXITCODE
Start-Sleep -Milliseconds 1600
$final = @('status', 'quit') | & $BrokerExe connect $PipeName $Token 2>&1 | Out-String
$finalExit = $LASTEXITCODE
$result = [ordered]@{
    scope = 'Real Windows x64 secure broker, wrong token, guest descendant, command, detach'
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    product_sha256 = $productSha256
    broker_sha256 = $brokerSha256
    wrong_exit = $wrongExit
    schedule_exit = $scheduleExit
    schedule = $scheduled
    final_exit = $finalExit
    final = $final
    passed = ($wrongExit -eq 6 -and $scheduleExit -eq 0 -and
              $scheduled -match 'scheduled' -and $finalExit -eq 0 -and
              $final -match 'guest-denied' -and $final -match 'detached')
}
$json = $result | ConvertTo-Json -Compress
if ($ResultPath) { [System.IO.File]::WriteAllText($ResultPath, $json) }
if ($ResultUrl) {
    Invoke-WebRequest -UseBasicParsing -Uri $ResultUrl -Method Post -ContentType 'application/json' -Body $json | Out-Null
}
Write-Output $json
if (-not $result.passed) { exit 1 }
