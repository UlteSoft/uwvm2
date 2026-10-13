# SOURCE ONLY: exact Windows x64 Meta-key adapter component, not debugger product.
# The pinned helper starts the PE suspended, assigns it to a unique kill-on-close
# Job before resume, and retains the owner through bounded retirement.
param(
    [Parameter(Mandatory=$true)][string]$Fixture,
    [Parameter(Mandatory=$true)][string]$FixtureSha256,
    [Parameter(Mandatory=$true)][string]$FixtureSource,
    [Parameter(Mandatory=$true)][string]$FixtureSourceSha256,
    [Parameter(Mandatory=$true)][string]$ConsoleKeyboardHeader,
    [Parameter(Mandatory=$true)][string]$ConsoleKeyboardHeaderSha256,
    [Parameter(Mandatory=$true)][string]$OwnedHelper,
    [Parameter(Mandatory=$true)][string]$OwnedHelperSha256,
    [Parameter(Mandatory=$true)][string]$RunnerSha256,
    [Parameter(Mandatory=$true)][string]$ResultJson
)
$ErrorActionPreference = 'Stop'
function Require([bool]$Ok,[string]$Why) { if(-not $Ok) { throw $Why } }
function Sha([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function ByteDigest([byte[]]$Bytes) {
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return -join ($algorithm.ComputeHash($Bytes) | ForEach-Object { $_.ToString('x2') }) }
    finally { $algorithm.Dispose() }
}
$owned=$null
$row=[ordered]@{schema='uwvm.windows.meta.key_event.vm.r4';passed=$false;scope='real WriteConsoleInputW/ReadConsoleInputExW Meta adapter component only';fixture_sha256='';fixture_source_sha256='';console_keyboard_header_sha256='';runner_sha256='';helper_sha256='';owned_job_active_after_retirement=$null;error='';exit_code=$null;actual_ctrl_c_delivery_tested=$false;product_debugger_tested=$false;source_owned_llvm_provider_qualified=$false;external_build_receipt_required=$true;stdout_base64='';stdout_sha256='';stderr_base64='';stderr_sha256=''}
try {
    Require ([IntPtr]::Size -eq 8 -and $env:PROCESSOR_ARCHITECTURE -eq 'AMD64') 'actual Windows x64 process required'
    foreach($value in @($FixtureSha256,$FixtureSourceSha256,$ConsoleKeyboardHeaderSha256,$OwnedHelperSha256,$RunnerSha256)) { Require ($value -match '^[0-9a-fA-F]{64}$') 'all expected SHA256 pins required' }
    foreach($path in @($Fixture,$FixtureSource,$ConsoleKeyboardHeader,$OwnedHelper,$PSCommandPath)) { Require (Test-Path -LiteralPath $path -PathType Leaf) ('missing exact file: '+$path) }
    $row.fixture_sha256=Sha $Fixture; $row.fixture_source_sha256=Sha $FixtureSource; $row.console_keyboard_header_sha256=Sha $ConsoleKeyboardHeader; $row.helper_sha256=Sha $OwnedHelper; $row.runner_sha256=Sha $PSCommandPath
    Require ($row.fixture_sha256 -eq $FixtureSha256.ToLowerInvariant() -and
             $row.fixture_source_sha256 -eq $FixtureSourceSha256.ToLowerInvariant() -and
             $row.console_keyboard_header_sha256 -eq $ConsoleKeyboardHeaderSha256.ToLowerInvariant() -and
             $row.helper_sha256 -eq $OwnedHelperSha256.ToLowerInvariant() -and
             $row.runner_sha256 -eq $RunnerSha256.ToLowerInvariant()) 'actual staged fixture/source/keyboard header/helper/runner bytes differ'
    . $OwnedHelper
    Initialize-CurrentDebugOwnedProcessTypes
    $application=[IO.Path]::GetFullPath($Fixture)
    $command=Quote-CurrentDebugNativeArgument $application
    # Owned private pipes are only launcher handles. The fixture then creates
    # its own private console with AllocConsole.
    $owned=[UwvmWindowsDebugAcceptanceOwnedProcessR1]::Start($application,$command,$false)
    Require $owned.AssignedBeforeResume 'real PE was not job-owned before resume'
    $row.process_id=$owned.Id; $row.creation_filetime=$owned.CreationFileTime; $row.assigned_before_resume=$owned.AssignedBeforeResume
    $owned.CloseInput()
    Require ($owned.WaitForExit(15000)) 'finite 15-second Meta component deadline expired'
    $row.exit_code=$owned.ExitCode
    Require ($owned.RetireJob(5000) -eq 0) 'owned job subtree did not retire'
    $row.owned_job_active_after_retirement=0
    Require ($owned.Drain(5000)) 'bounded stdout/stderr drain did not finish'
    Require (-not $owned.CaptureFaulted) 'owned capture exceeded 4 MiB extent'
    $stdout=$owned.StdoutBytes(); $stderr=$owned.StderrBytes()
    $row.stdout_base64=[Convert]::ToBase64String($stdout); $row.stdout_sha256=ByteDigest $stdout
    $row.stderr_base64=[Convert]::ToBase64String($stderr); $row.stderr_sha256=ByteDigest $stderr
    Require ($row.exit_code -eq 0 -and $stdout.Length -eq 0 -and $stderr.Length -eq 0) 'actual adapter component failed or emitted unexpected output'
    Require ((Sha $Fixture) -eq $row.fixture_sha256 -and (Sha $FixtureSource) -eq $row.fixture_source_sha256 -and
             (Sha $ConsoleKeyboardHeader) -eq $row.console_keyboard_header_sha256 -and
             (Sha $OwnedHelper) -eq $row.helper_sha256 -and (Sha $PSCommandPath) -eq $row.runner_sha256) 'staged fixture/source/header/helper/runner bytes changed during actual VM run'
    $row.passed=$true
} catch { $row.error=$_.Exception.ToString() }
finally {
    if($null -ne $owned) {
        try { if(-not $owned.HasExited) { $owned.Kill() }; if($owned.ActiveProcesses -ne 0) { $owned.RetireJob(5000) | Out-Null } }
        catch { $row.passed=$false; $row.error += ' | retirement: '+$_.Exception.ToString() }
        try { $owned.Dispose() } catch { $row.passed=$false; $row.error += ' | dispose: '+$_.Exception.ToString() }
    }
    $target=[IO.Path]::GetFullPath($ResultJson)
    foreach($sourcePath in @($Fixture,$FixtureSource,$ConsoleKeyboardHeader,$OwnedHelper,$PSCommandPath)) {
        if([String]::Equals($target,[IO.Path]::GetFullPath($sourcePath),[StringComparison]::OrdinalIgnoreCase)) { throw 'evidence may not overwrite fixture/source/header/helper/runner' }
    }
    $bytes=[Text.Encoding]::UTF8.GetBytes(($row | ConvertTo-Json -Depth 8)+[char]10)
    $stream=[IO.FileStream]::new($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
}
if($row.passed) { exit 0 } else { exit 1 }
