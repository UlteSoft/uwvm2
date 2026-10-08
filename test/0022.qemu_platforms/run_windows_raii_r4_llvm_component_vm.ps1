# SOURCE ONLY: fixed, independently pinned Windows RAII component acceptance.
# The original Linux guardian owns this guest inside the shared 64 GiB cgroup.
# No product debugger, current three-TU ABI, or named module is qualified here.
param(
    [Parameter(Mandatory=$true)][string]$ArtifactRoot,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$QualificationSha256
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE ('Documents\uwvm-raii-component-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $work | Out-Null
$rows = New-Object 'System.Collections.Generic.List[object]'
$result = [ordered]@{
    schema = 'uwvm-windows-raii-r4-llvm-actual-guest-v1'
    passed = $false
    standalone_component_only = $true
    product_three_tu_abi_qualified = $false
    named_module_qualified = $false
    new_readonly_sync_provider_qualified = $false
    qualification_sha256 = $QualificationSha256
    actual_os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    actual_os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    actual_architecture = [string]$env:PROCESSOR_ARCHITECTURE
    standalone_SDK_static_LLVM_recipe_only = $true
    ROS_vendored_LLVM23_full_product_qualified = $false
    cases = @()
    error = $null
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Sha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}
function BoundFile([string]$Relative, [string]$Expected, [long]$Limit) {
    # Qualification admits flat artifact names, never an arbitrary path or URL.
    Require ($Relative -cmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,120}$') 'invalid fixed artifact name'
    Require ($Expected -cmatch '^[0-9a-f]{64}$') 'missing exact artifact hash'
    $path = Join-Path $ArtifactRoot $Relative
    $item = Get-Item -LiteralPath $path
    Require (-not $item.PSIsContainer -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'artifact is not a plain regular file'
    Require ($item.Length -gt 0 -and $item.Length -le $Limit) 'artifact extent outside qualification'
    Require ((Sha256 $path) -ceq $Expected) 'artifact hash mismatch'
    return $path
}
function ReadBounded([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    Require (-not $item.PSIsContainer -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'output is not a plain regular file'
    Require ($item.Length -le 524288) 'component regular-file output exceeds bound'
    return [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($Path))
}

try {
    Require ([IntPtr]::Size -eq 8 -and $env:PROCESSOR_ARCHITECTURE -ceq 'AMD64') 'actual Windows x64 process required'
    $qualificationPath = BoundFile 'qualification.json' $QualificationSha256 1048576
    $qualification = Get-Content -Raw -LiteralPath $qualificationPath | ConvertFrom-Json
    Require ($qualification.schema -ceq 'uwvm-windows-raii-r4-llvm-guest-qualification-v1') 'wrong component qualification schema'
    Require ($qualification.source_manifest_sha256 -ceq 'f34df8d18c3a0bf901609c0a99da30f07769c6b9288b8af0c4723b8bc0480e28') 'wrong immutable owner/header source'
    Require ($qualification.standalone_component_only -eq $true -and
        $qualification.product_three_tu_abi_qualified -eq $false -and
        $qualification.named_module_qualified -eq $false -and
        $qualification.new_readonly_sync_provider_qualified -eq $false) 'unsupported qualification claim'
    Require ($qualification.actual_four_pe_builds_accepted -eq $true -and
        $qualification.actual_bridge_assembly_review_accepted -eq $true -and
        $qualification.actual_guest_dll_closure_bound -eq $true) 'build, assembly and guest DLL closure must be accepted first'
    Require ($qualification.actual_SDK_static_LLVM_runtime_provider_bound -eq $true -and
        $qualification.actual_GNU_unwind_imports_rejected -eq $true -and
        $qualification.ROS_vendored_LLVM23_full_product_qualified -eq $false) 'standalone static LLVM provider evidence required'
    Require (@($qualification.cases).Count -eq 4) 'exactly four fresh component binaries required'
    $runner = BoundFile 'run_windows_raii_r4_llvm_component_vm.ps1' $qualification.runner_sha256 65536
    Require (([IO.Path]::GetFullPath($runner)) -ceq ([IO.Path]::GetFullPath($PSCommandPath))) 'runner was not executed from its pinned artifact'
    $helper = BoundFile 'windows_raii_owned_process.ps1' $qualification.owner_helper_sha256 65536
    Require ($qualification.owner_helper_sha256 -ceq '05d73c0d84fb085bc1b7576e06a307ac436635b0bb031dbc25d7a7b90ffab4f3') 'wrong immutable owned Job helper'
    . $helper
    Initialize-CurrentDebugOwnedProcessTypes
    $launcherSource = BoundFile 'windows_raii_regular_launcher.cc' $qualification.launcher_source_sha256 65536
    $launcher = BoundFile 'windows-raii-launcher.exe' $qualification.launcher_pe_sha256 16777216
    $launcherPrivate = Join-Path $work 'windows-raii-launcher.exe'
    Copy-Item -LiteralPath $launcher -Destination $launcherPrivate
    Require ((Sha256 $launcherPrivate) -ceq $qualification.launcher_pe_sha256) 'private launcher copy changed'
    $expectedLabels = @('uwvm2-ros-eh','uwvm2-ros-noeh','uwvm2-eh','uwvm2-noeh')
    $seen = @{}
    foreach ($case in $qualification.cases) {
        $label = [string]$case.label
        Require ($expectedLabels -ccontains $label -and -not $seen.ContainsKey($label)) 'unknown or duplicate fixed variant'
        $seen[$label] = $true
        $pe = BoundFile ([string]$case.filename) ([string]$case.pe_sha256) 33554432
        $directory = Join-Path $work $label
        New-Item -ItemType Directory $directory | Out-Null
        $private = Join-Path $directory 'native-step.exe'
        Copy-Item -LiteralPath $pe -Destination $private
        Require ((Sha256 $private) -ceq [string]$case.pe_sha256) 'private component PE changed'
        $stdout = Join-Path $directory 'stdout.txt'
        $stderr = Join-Path $directory 'stderr.txt'
        $command = (Quote-CurrentDebugNativeArgument $launcherPrivate) + ' ' +
            (Quote-CurrentDebugNativeArgument $private) + ' ' +
            (Quote-CurrentDebugNativeArgument $stdout) + ' ' +
            (Quote-CurrentDebugNativeArgument $stderr)
        $owned = $null
        $row = [ordered]@{ label=$label; pe_sha256=[string]$case.pe_sha256; passed=$false;
            actual_pid=$null; actual_creation_filetime=$null; assigned_before_resume=$false;
            exit_code=$null; owned_job_active_after_retirement=$null; launcher_stdout=$null;
            launcher_stderr=$null; component_stdout=$null; component_stderr=$null }
        try {
            $owned = [UwvmWindowsDebugAcceptanceOwnedProcessR1]::Start($launcherPrivate, $command, $false)
            $row.actual_pid = $owned.Id
            $row.actual_creation_filetime = $owned.CreationFileTime
            $row.assigned_before_resume = $owned.AssignedBeforeResume
            Require $owned.AssignedBeforeResume 'launcher was not admitted suspended into owned Job'
            $owned.SendInput($null, 1000)
            Require ($owned.WaitForExit(100000)) 'owned launcher exceeded component deadline'
            $row.exit_code = $owned.ExitCode
            Require ($owned.Drain(5000) -and -not $owned.CaptureFaulted) 'bounded launcher capture failed'
            $row.launcher_stdout = [Text.Encoding]::UTF8.GetString($owned.StdoutBytes())
            $row.launcher_stderr = [Text.Encoding]::UTF8.GetString($owned.StderrBytes())
            $row.owned_job_active_after_retirement = $owned.RetireJob(10000)
            Require ($row.owned_job_active_after_retirement -eq 0) 'owned Job retained runnable members'
            Require ($row.exit_code -eq 0) 'real full-width component process status was nonzero'
            Require ($row.launcher_stdout -match 'raii-child-pid=[0-9]+ inherited-job=yes output=regular' -and
                $row.launcher_stdout -match 'raii-child-exit=0') 'missing actual child admission/exit witnesses'
            $row.component_stdout = ReadBounded $stdout
            $row.component_stderr = ReadBounded $stderr
            Require ($row.component_stderr.Length -eq 0) 'component reported stderr'
            $expectDeaths = @(
                'Windows RAII isolated death: --invalid-gate exit=e0000db6',
                'Windows RAII isolated death: --invalid-set-event exit=e0000db6',
                'Windows RAII isolated death: --invalid-first-pc exit=e0000db7',
                'Windows RAII isolated death: --invalid-live-destruction exit=e0000db6')
            foreach ($line in $expectDeaths) {
                Require ($row.component_stdout.Contains($line)) 'missing exact isolated ownership death status'
            }
            Require ($row.component_stdout.Contains('PASS Windows x64 native-step RAII: real zero/one/two instruction traps, owned GPR copy, exact handle mirrors, failed-request emptiness, clear retirement, 256 ready races and four isolated ownership deaths')) 'missing actual complete RAII component result'
            Require ((Sha256 $private) -ceq [string]$case.pe_sha256) 'component PE changed during guest execution'
            $row.passed = $true
        } finally {
            if ($null -ne $owned) {
                try { $row.owned_job_active_after_retirement = $owned.RetireJob(10000) }
                finally { $owned.Dispose() }
            }
            $rows.Add([pscustomobject]$row)
        }
    }
    Require ($seen.Count -eq 4 -and @($rows | Where-Object { -not $_.passed }).Count -eq 0) 'four distinct fresh components must pass'
    $result.passed = $true
} catch {
    $result.error = $_.Exception.ToString()
} finally {
    $result.cases = @($rows)
    $result.work = $work
    $result.runner_sha256 = Sha256 $PSCommandPath
    $report = Join-Path $work 'actual-result.json'
    [IO.File]::WriteAllText($report, ($result | ConvertTo-Json -Depth 20), (New-Object Text.UTF8Encoding($false)))
    Write-Output ($result | ConvertTo-Json -Depth 20)
}
if (-not $result.passed) { exit 1 }
exit 0
