$ErrorActionPreference = 'Stop'
$url = 'http://10.0.2.4:18018/uwvm-native-step-windows-x64'
$work = Join-Path $env:USERPROFILE 'Documents\uwvm-control-pipe-probe'
New-Item -ItemType Directory -Force $work | Out-Null
$result = [ordered]@{
    passed = $false
    date_utc = [DateTime]::UtcNow.ToString('o')
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    scope = 'Real Windows x64 inherited-client named-pipe identity and message boundary'
}
try {
    $exe = Join-Path $work 'windows_control_pipe_probe.exe'
    Invoke-WebRequest -UseBasicParsing "$url/windows_control_pipe_probe.exe" -OutFile $exe
    $result.sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $exe
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.Start() | Out-Null
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'pipe probe timed out' }
    $process.WaitForExit()
    $result.exit_code = [int]$process.ExitCode
    $result.stdout = [string]$stdout.Result
    $result.stderr = [string]$stderr.Result
    $process.Dispose()
    if ($result.exit_code -ne 0 -or -not $result.stdout.Contains('windows_control_pipe_probe=pass')) {
        throw "pipe probe failed: $($result.stdout) $($result.stderr)"
    }
    $result.passed = $true
} catch {
    $result.error = [string]$_.Exception.Message
}
$json = $result | ConvertTo-Json -Depth 8 -Compress
[System.IO.File]::WriteAllText((Join-Path $work 'result.json'), $json, [System.Text.Encoding]::UTF8)
Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' -Body ([System.Text.Encoding]::UTF8.GetBytes($json)) "$url/result" | Out-Null
if (-not $result.passed) { exit 1 }
