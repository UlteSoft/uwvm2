# Run in a real Windows console in the controlled VM. Keep the broker's random
# capability in this host PowerShell process only; never redirect the server's
# console output or write the pipe name/token to a file or guest-visible path.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18020/uwvm-broker-ordinary-x64',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ProductSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$BrokerSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ChildSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$WasmSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ProbeSha256
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE 'Documents\uwvm-broker-ordinary-host'
New-Item -ItemType Directory -Force $work | Out-Null
try {
    foreach ($name in @('uwvm.exe', 'uwvm-debug-server.exe',
                        'windows_control_broker_child.exe', 'native-step-fixture.wasm',
                        'run_windows_control_broker_vm.ps1')) {
        Invoke-WebRequest -UseBasicParsing "$BaseUrl/$name" -OutFile (Join-Path $work $name)
    }
    $expected = @{
        'uwvm.exe' = $ProductSha256
        'uwvm-debug-server.exe' = $BrokerSha256
        'windows_control_broker_child.exe' = $ChildSha256
        'native-step-fixture.wasm' = $WasmSha256
        'run_windows_control_broker_vm.ps1' = $ProbeSha256
    }
    foreach ($name in $expected.Keys) {
        if ((Get-FileHash (Join-Path $work $name) -Algorithm SHA256).Hash -ine $expected[$name]) {
            throw "staged broker input changed before guest execution: $name"
        }
    }
} catch {
    # No capability exists yet. Keep the diagnostic bounded and publish it
    # without executing a downloaded PE or the second-stage probe.
    $failure = '{"passed":false,"error":"broker-input-verification-failed"}'
    try {
        [System.IO.File]::WriteAllText((Join-Path $work 'result.json'), $failure,
            [System.Text.Encoding]::UTF8)
    } catch {}
    try {
        Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' `
            -Body $failure "$BaseUrl/result" | Out-Null
    } catch {}
    exit 1
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class UwvmHostConsoleTail {
    [StructLayout(LayoutKind.Sequential)]
    public struct Coord { public short X; public short Y; }
    [StructLayout(LayoutKind.Sequential)]
    public struct SmallRect { public short Left; public short Top; public short Right; public short Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct BufferInfo {
        public Coord Size;
        public Coord Cursor;
        public ushort Attributes;
        public SmallRect Window;
        public Coord MaximumWindow;
    }
    [DllImport("kernel32.dll")]
    static extern IntPtr GetStdHandle(int handle);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetConsoleScreenBufferInfo(IntPtr handle, out BufferInfo info);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool ReadConsoleOutputCharacterW(IntPtr handle,
        [Out, MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.U2)] char[] characters,
        uint count, Coord origin, out uint read);

    public static int CursorRow() {
        IntPtr handle = GetStdHandle(-11);
        if (handle == IntPtr.Zero || handle == new IntPtr(-1))
            throw new InvalidOperationException("host stdout is not a console");
        BufferInfo info;
        if (!GetConsoleScreenBufferInfo(handle, out info) || info.Size.X < 80)
            throw new InvalidOperationException("host console buffer is unavailable or too narrow");
        if (info.Cursor.X != 0)
            throw new InvalidOperationException("host console cursor must start a fresh line");
        return info.Cursor.Y;
    }

    public static string ReadFrom(int first) {
        IntPtr handle = GetStdHandle(-11);
        if (handle == IntPtr.Zero || handle == new IntPtr(-1))
            throw new InvalidOperationException("host stdout is not a console");
        BufferInfo info;
        if (!GetConsoleScreenBufferInfo(handle, out info) || info.Size.X < 80)
            throw new InvalidOperationException("host console buffer is unavailable or too narrow");
        if (first < 0 || first > info.Cursor.Y || info.Cursor.Y - first > 64)
            throw new InvalidOperationException("host console buffer moved during capability capture");
        var result = new StringBuilder();
        for (int row = first; row <= info.Cursor.Y; ++row) {
            var line = new char[info.Size.X];
            uint read;
            if (!ReadConsoleOutputCharacterW(handle, line, (uint)info.Size.X,
                    new Coord { X = 0, Y = (short)row }, out read))
                throw new InvalidOperationException("cannot read this host console");
            result.Append(new string(line, 0, (int)read).TrimEnd()).Append('\n');
        }
        return result.ToString();
    }
}
'@

$product = Join-Path $work 'uwvm.exe'
$broker = Join-Path $work 'uwvm-debug-server.exe'
$wasm = Join-Path $work 'native-step-fixture.wasm'
$probe = Join-Path $work 'run_windows_control_broker_vm.ps1'
$child = Join-Path $work 'windows_control_broker_child.exe'
$env:UWVM_WINDOWS_BROKER_CHILD = $child
$brokerArguments = 'serve "' + $product + '" -m run -Rcc jit -Rcm full -Rct 0 ' +
    '-Rllvm-call-stack unwind -Rllvm-cache-path disable --run "' + $wasm + '"'
$server = $null
$guestPid = $null
$posted = $false
try {
    Clear-Host
    $firstRow = [UwvmHostConsoleTail]::CursorRow()
    $server = Start-Process -FilePath $broker -ArgumentList $brokerArguments -NoNewWindow -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    $pipe = $null
    $token = $null
    while ([DateTime]::UtcNow -lt $deadline -and -not $server.HasExited) {
        $console = [UwvmHostConsoleTail]::ReadFrom($firstRow)
        $pipeMatches = [regex]::Matches($console, 'debug server: (\\\\\.\\pipe\\uwvm-debug-host-[0-9a-f]{32})')
        $tokenMatches = [regex]::Matches($console, 'capability: ([0-9a-f]{64})')
        if ($pipeMatches.Count -gt 1 -or $tokenMatches.Count -gt 1) {
            throw 'ambiguous host console capability announcement'
        }
        if ($pipeMatches.Count -eq 1 -and $tokenMatches.Count -eq 1) {
            $pipe = $pipeMatches[0].Groups[1].Value
            $token = $tokenMatches[0].Groups[1].Value
            break
        }
        Start-Sleep -Milliseconds 200
    }
    if (-not $pipe -or -not $token) { throw 'broker did not publish a console-only capability' }
    $guest = @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $($server.Id)" |
               Where-Object { $_.Name -ieq 'uwvm.exe' })
    if ($guest.Count -eq 1) { $guestPid = [int]$guest[0].ProcessId }
    $probeOutput = & $probe -PipeName $pipe -Token $token -ProductExe $product `
        -BrokerExe $broker | Out-String
    $probeExit = $LASTEXITCODE
    if ($probeExit -ne 0) { throw 'secure broker guest probe failed' }
    $probeResult = $probeOutput | ConvertFrom-Json
    if (-not $probeResult.passed -or
        $probeResult.product_sha256 -ne (Get-FileHash $product -Algorithm SHA256).Hash.ToLowerInvariant() -or
        $probeResult.broker_sha256 -ne (Get-FileHash $broker -Algorithm SHA256).Hash.ToLowerInvariant()) {
        throw 'secure broker probe returned invalid product-bound evidence'
    }
    $server.WaitForExit(5000) | Out-Null
    if (-not $server.HasExited -or $server.ExitCode -ne 0) {
        throw 'secure broker did not exit cleanly after detach'
    }
    Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' `
        -Body $probeOutput.Trim() "$BaseUrl/result" | Out-Null
    $posted = $true
} catch {
    if (-not $posted) {
        # The one-result transport still records a bounded failure without
        # including the console capability or pipe name.
        $failure = [ordered]@{
            passed = $false
            product_sha256 = (Get-FileHash $product -Algorithm SHA256).Hash.ToLowerInvariant()
            broker_sha256 = (Get-FileHash $broker -Algorithm SHA256).Hash.ToLowerInvariant()
            error = 'broker-host-vm-orchestration-failed'
        } | ConvertTo-Json -Compress
        Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' `
            -Body $failure "$BaseUrl/result" | Out-Null
    }
    throw
} finally {
    $pipe = $null
    $token = $null
    $console = $null
    $pipeMatches = $null
    $tokenMatches = $null
    $probeOutput = $null
    $probeResult = $null
    if ($server -and -not $server.HasExited) { Stop-Process -Id $server.Id -Force }
    if ($guestPid) { Stop-Process -Id $guestPid -Force -ErrorAction SilentlyContinue }
    Remove-Item Env:\UWVM_WINDOWS_BROKER_CHILD -ErrorAction SilentlyContinue
}
