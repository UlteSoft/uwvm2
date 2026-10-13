# Supplemental real-Windows ROS LLVM-full Core 3 syntax and feature-gate test.
# The source-matched product and these Wasm binaries are served by the bounded
# Linux cgroup's immutable-hash artifact server, never by a guest-writable path.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18020/uwvm-core3-windows-x64',
    [string]$ArtifactRoot = '',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE 'Documents\uwvm-core3-r38'
New-Item -ItemType Directory -Force $work | Out-Null
$script:rows = New-Object 'System.Collections.Generic.List[object]'
$result = [ordered]@{
    passed = $false
    source_id = $null
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    cases = @()
}
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Check-Run([string]$Name, [string[]]$Arguments, [bool]$Expected,
                   [string]$Diagnostic = '') {
    $log = Join-Path $work "$Name.log"
    & $script:uwvm @Arguments *> $log
    $code = $LASTEXITCODE
    $output = [System.IO.File]::ReadAllText($log)
    $diagnosticOutput = [regex]::Replace($output, '\x1b\[[0-9;]*[A-Za-z]', '')
    $pass = (($code -eq 0) -eq $Expected)
    if (-not $Expected -and $Diagnostic) {
        $pass = $pass -and $diagnosticOutput.ToLowerInvariant().Contains($Diagnostic)
    }
    $script:rows.Add([ordered]@{
        name = $Name
        exit_code = $code
        expected_success = $Expected
        diagnostic = $Diagnostic
        passed = [bool]$pass
        log_sha256 = (Get-FileHash $log -Algorithm SHA256).Hash
    })
    if (-not $pass) {
        throw "$Name failed: exit=$code expected=$Expected diagnostic=$Diagnostic output=$($output.Substring([Math]::Max(0, $output.Length - 1200)))"
    }
}
try {
    $qualificationFile = Join-Path $work 'qualification.json'
    if ($ArtifactRoot) {
        Require (Test-Path -LiteralPath $ArtifactRoot -PathType Container) `
            'local Core 3 artifact root is unavailable'
        Copy-Item -LiteralPath (Join-Path $ArtifactRoot 'qualification.json') `
            -Destination $qualificationFile -Force
    } else {
        Invoke-WebRequest -UseBasicParsing "$BaseUrl/qualification.json" -OutFile $qualificationFile
    }
    Require ((Get-FileHash $qualificationFile -Algorithm SHA256).Hash -ieq $QualificationSha256) `
        'Core 3 qualification changed before guest execution'
    $qualification = Get-Content -Raw $qualificationFile | ConvertFrom-Json
    Require ($qualification.source_id -match '^sha256:[0-9a-fA-F]{64}$' -and
             $qualification.product_sha256 -match '^[0-9a-fA-F]{64}$' -and
             $qualification.llvm_certificate_sha256 -match '^[0-9a-fA-F]{64}$') `
        'Core 3 qualification differs from this exact source and PE'
    $result.source_id = [string]$qualification.source_id
    $result.qualification_sha256 = (Get-FileHash $qualificationFile -Algorithm SHA256).Hash
    $result.llvm_certificate_sha256 = [string]$qualification.llvm_certificate_sha256
    $result.branch_reference_sha256 = [string]$qualification.branch_reference_sha256
    $result.gap_oracle_sha256 = [string]$qualification.gap_oracle_sha256
    $gcStems = @('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
        'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
        'gc_struct_into_anyref_table', 'i31_table64')
    $branchStems = @('branch_semantics_br_if',
        'branch_semantics_br_on_null', 'branch_semantics_br_on_non_null')
    $gapStems = @('ref-test-exn-null', 'ref-cast-exn-null',
        'br-on-cast-exn-null', 'exn-cast-retained', 'ref-cast-exn-null-trap',
        'exnref_call_retained', 'eh-cross-function-catch-ref',
        'eh-cross-function-win64-seh')
    $files = @('uwvm.exe', 'core3-eh-threads.wasm', 'core3-typed-ref.wasm',
        'core3-throw-ref-bottom.wasm', 'core3-br-on-null-bottom.wasm',
        'gc_const_expr_execution.wasm', 'wasm3_initializers.wasm',
        'exnref_struct_payload_execution.wasm') +
        @($gcStems | ForEach-Object { "$($_).wasm" }) +
        @($branchStems | ForEach-Object { "$($_).wasm" }) +
        @($gapStems | ForEach-Object { "$($_).wasm" })
    foreach ($name in $files) {
        $property = $qualification.files_sha256.PSObject.Properties[$name]
        Require ($null -ne $property) "Core 3 qualification lacks staged artifact: $name"
        $expected = [string]$property.Value
        Require ($expected -match '^[0-9a-fA-F]{64}$') "invalid staged SHA-256: $name"
        $target = if ($ArtifactRoot -and $name -eq 'uwvm.exe') {
            Join-Path $ArtifactRoot $name
        } else {
            Join-Path $work $name
        }
        if ($ArtifactRoot -and $name -ne 'uwvm.exe') {
            Copy-Item -LiteralPath (Join-Path $ArtifactRoot $name) -Destination $target -Force
        } elseif (-not $ArtifactRoot) {
            Invoke-WebRequest -UseBasicParsing "$BaseUrl/$name" -OutFile $target
        }
        Require ((Get-FileHash $target -Algorithm SHA256).Hash -ieq $expected) `
            "staged Core 3 artifact changed before guest execution: $name"
    }
    $script:uwvm = if ($ArtifactRoot) { Join-Path $ArtifactRoot 'uwvm.exe' } else { Join-Path $work 'uwvm.exe' }
    $result.product_sha256 = (Get-FileHash $script:uwvm -Algorithm SHA256).Hash
    Require ($result.product_sha256 -ieq $qualification.product_sha256) `
        'Core 3 PE differs from qualification'
    $base = @('-Raot', '-WFE-gc', '-WFE-reference-types',
        '-WFE-table-instructions', '-WFE-bulk-memory', '-WFE-extended-const',
        '-WFD-exceptions', '-WFD-function-references', '-WFD-threads')
    foreach ($stem in $gcStems) {
        $wasm = Join-Path $work "$stem.wasm"
        $extra = if ($stem -eq 'i31_table64') { @('-WFE-table64') } else { @() }
        Check-Run "gc-$stem" ($base + $extra + @('--run', $wasm)) $true
        $gcOff = @($base | Where-Object { $_ -ne '-WFE-gc' }) + @('-WFD-gc')
        Check-Run "gc-$stem-gc-off" ($gcOff + $extra + @('--run', $wasm)) $false 'gc'
        if ($stem -eq 'i31_table64') {
            Check-Run 'gc-i31_table64-table64-off' ($base + @('-WFD-table64', '--run', $wasm)) $false 'table64'
        }
    }
    # A Core 3 GC constructor is itself a valid constant expression. It must
    # initialize before linking even with extended-const explicitly disabled.
    $gcConst = Join-Path $work 'gc_const_expr_execution.wasm'
    foreach ($policy in @('instruction', 'unwind')) {
        $constBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-gc', '-WFD-extended-const')
        Check-Run "gc-const-expr-$policy-extended-const-off" ($constBase + @('--run', $gcConst)) $true
        $constGcOff = @($constBase | Where-Object { $_ -ne '-WFE-gc' }) + @('-WFD-gc')
        Check-Run "gc-const-expr-$policy-gc-off" ($constGcOff + @('--run', $gcConst)) $false 'gc'
    }
    # The official Function References proposal groups table initializers with
    # that proposal. UWVM also exposes table-initializer as a finer-grained
    # switch; this checks that UWVM's independent gate accepts the legacy
    # function type. The Wasmtime oracle is run with function references on.
    $initializer = Join-Path $work 'wasm3_initializers.wasm'
    foreach ($policy in @('instruction', 'unwind')) {
        $initBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-extended-const',
            '-WFE-table-initializer', '-WFD-function-references')
        Check-Run "ref-func-table-initializer-$policy" ($initBase + @('--run', $initializer)) $true
        $extendedOff = @($initBase | Where-Object { $_ -ne '-WFE-extended-const' }) + @('-WFD-extended-const')
        Check-Run "ref-func-table-initializer-$policy-extended-const-off" `
            ($extendedOff + @('--run', $initializer)) $false 'extended-const'
        $initializerOff = @($initBase | Where-Object { $_ -ne '-WFE-table-initializer' }) + @('-WFD-table-initializer')
        Check-Run "ref-func-table-initializer-$policy-feature-off" `
            ($initializerOff + @('--run', $initializer)) $false 'table-initializer'
    }
    $focus = Join-Path $work 'i31_into_anyref_table.wasm'
    $refOff = @($base | Where-Object { $_ -ne '-WFE-reference-types' }) + @('-WFD-reference-types')
    Check-Run 'gc-reference-types-off' ($refOff + @('--run', $focus)) $false 'reference-types'
    $tableOff = @($base | Where-Object { $_ -ne '-WFE-table-instructions' }) + @('-WFD-table-instructions')
    Check-Run 'gc-table-instructions-off' ($tableOff + @('--run', $focus)) $false 'table-instructions'
    $eh = Join-Path $work 'core3-eh-threads.wasm'
    foreach ($policy in @('instruction', 'unwind')) {
        $ehBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-exceptions', '-WFE-threads')
        Check-Run "eh-threads-$policy" ($ehBase + @('--run', $eh)) $true
        $exceptionOff = @($ehBase | Where-Object { $_ -ne '-WFE-exceptions' }) + @('-WFD-exceptions')
        Check-Run "eh-threads-$policy-exceptions-off" ($exceptionOff + @('--run', $eh)) $false 'exceptions'
        $threadsOff = @($ehBase | Where-Object { $_ -ne '-WFE-threads' }) + @('-WFD-threads')
        Check-Run "eh-threads-$policy-threads-off" ($threadsOff + @('--run', $eh)) $false 'threads'
    }
    # Windows x64 must resolve auto to its supported native unwind path.
    Check-Run 'eh-threads-auto' @('-Raot', '-Rct', '0', '-Rllvm-call-stack', 'auto',
        '-Rclog', 'err', '-Rllvm-cache-path', 'disable', '-WFE-exceptions', '-WFE-threads', '--run', $eh) $true
    $autoText = [System.IO.File]::ReadAllText((Join-Path $work 'eh-threads-auto.log'))
    Require ($autoText.Contains('call_stack=unwind unwind_backend=win64-seh') -and
             $autoText.Contains('unwind_replace_frames=yes call_stack_frames=omit')) `
        'auto did not select native Win64 SEH unwind without instruction frames'
    $typed = Join-Path $work 'core3-typed-ref.wasm'
    Check-Run 'typed-ref-call-ref-full' @('-Raot', '-WFE-function-references', '--run', $typed) $true
    Check-Run 'typed-ref-call-ref-feature-off' @('-m', 'validation', '-WFD-function-references', '--run', $typed) $false 'function-references'
    foreach ($policy in @('instruction', 'unwind')) {
        $bottomBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-function-references', '-WFE-exceptions')
        foreach ($stem in @('core3-throw-ref-bottom', 'core3-br-on-null-bottom')) {
            $bottom = Join-Path $work "$stem.wasm"
            Check-Run "$stem-$policy" ($bottomBase + @('--run', $bottom)) $true
            $exceptionsOff = @($bottomBase | Where-Object { $_ -ne '-WFE-exceptions' }) + @('-WFD-exceptions')
            Check-Run "$stem-$policy-exceptions-off" ($exceptionsOff + @('--run', $bottom)) $false 'exceptions'
            if ($stem -eq 'core3-br-on-null-bottom') {
                $functionReferencesOff = @($bottomBase | Where-Object { $_ -ne '-WFE-function-references' }) + @('-WFD-function-references')
                Check-Run "$stem-$policy-function-references-off" ($functionReferencesOff + @('--run', $bottom)) $false '--wasm-feature-enable-function-references'
            }
        }
    }
    # Each module checks both taken and fallthrough branch results. Core 3's
    # br_on_* instructions require function references, while br_if remains
    # valid when that proposal is disabled.
    foreach ($policy in @('instruction', 'unwind')) {
        $branchBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-function-references')
        $branchOff = @($branchBase | Where-Object { $_ -ne '-WFE-function-references' }) +
            @('-WFD-function-references')
        foreach ($stem in $branchStems) {
            $branch = Join-Path $work "$stem.wasm"
            Check-Run "$stem-$policy" ($branchBase + @('--run', $branch)) $true
            if ($stem -eq 'branch_semantics_br_if') {
                Check-Run "$stem-$policy-function-references-off" `
                    ($branchOff + @('--run', $branch)) $true
            } else {
                Check-Run "$stem-$policy-function-references-off" `
                    ($branchOff + @('--run', $branch)) $false 'function-references'
            }
        }
    }
    # A typed GC object inside an exception must survive catch_ref, throw_ref,
    # and the outer catch with both LLVM JIT call stack strategies.
    $exnref = Join-Path $work 'exnref_struct_payload_execution.wasm'
    foreach ($mode in @('jit-instruction', 'jit-unwind')) {
        $policy = if ($mode -eq 'jit-instruction') { 'instruction' } else { 'unwind' }
        $engine = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable')
        $enabled = $engine + @('-WFE-gc', '-WFE-exceptions',
            '-WFE-reference-types')
        Check-Run "exnref-struct-$mode" ($enabled + @('--run', $exnref)) $true
        foreach ($feature in @('exceptions', 'gc', 'reference-types')) {
            $disabled = @($enabled | Where-Object { $_ -ne "-WFE-$feature" }) +
                @("-WFD-$feature")
            Check-Run "exnref-struct-$mode-$feature-off" `
                ($disabled + @('--run', $exnref)) $false $feature
        }
    }
    # Core 3 permits exception references as GC cast/test carriers. Exercise
    # all three newly added encodings in both JIT call-stack policies, then
    # reject each when its GC or exceptions feature is independently off.
    $crossFailures = New-Object 'System.Collections.Generic.List[string]'
    foreach ($policy in @('instruction', 'unwind')) {
        $gapBase = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable', '-WFE-gc', '-WFE-exceptions')
        foreach ($stem in $gapStems) {
            $gap = Join-Path $work "$stem.wasm"
            $caseBase = $gapBase
            if ($stem -eq 'eh-cross-function-catch-ref') {
                # ref.as_non_null in the rethrow path has an independent gate.
                $caseBase = $gapBase + @('-WFE-function-references')
            }
            if ($stem -eq 'ref-cast-exn-null-trap') {
                Check-Run "$stem-$policy" ($caseBase + @('--run', $gap)) `
                    $false 'reference cast failed: value does not match target heap type'
                $trapText = [regex]::Replace(
                    [System.IO.File]::ReadAllText((Join-Path $work "$stem-$policy.log")),
                    '\x1b\[[0-9;]*[A-Za-z]', '')
                Require ($trapText.Contains('[fatal] Runtime crash') -and
                         $trapText.Contains('Call stack:') -and
                         $trapText.Contains('func_idx=')) `
                    "ref.cast failure lacks fatal VM call stack: $policy"
            } elseif ($stem.StartsWith('eh-cross-function-')) {
                try { Check-Run "$stem-$policy" ($caseBase + @('--run', $gap)) $true }
                catch { $crossFailures.Add([string]$_) }
            } else {
                Check-Run "$stem-$policy" ($caseBase + @('--run', $gap)) $true
            }
            $gapGcOff = @($caseBase | Where-Object { $_ -ne '-WFE-gc' }) + @('-WFD-gc')
            if ($stem.StartsWith('eh-cross-function-')) {
                try { Check-Run "$stem-$policy-gc-off" ($gapGcOff + @('--run', $gap)) $true }
                catch { $crossFailures.Add([string]$_) }
            } elseif ($stem -eq 'exnref_call_retained') {
                Check-Run "$stem-$policy-gc-off" ($gapGcOff + @('--run', $gap)) $true
            } else {
                Check-Run "$stem-$policy-gc-off" ($gapGcOff + @('--run', $gap)) $false 'gc'
            }
            $gapExceptionsOff = @($caseBase | Where-Object { $_ -ne '-WFE-exceptions' }) +
                @('-WFD-exceptions')
            if ($stem.StartsWith('eh-cross-function-')) {
                try {
                    Check-Run "$stem-$policy-exceptions-off" `
                        ($gapExceptionsOff + @('--run', $gap)) $false 'exceptions'
                } catch { $crossFailures.Add([string]$_) }
            } else {
                Check-Run "$stem-$policy-exceptions-off" `
                    ($gapExceptionsOff + @('--run', $gap)) $false 'exceptions'
            }
            if ($stem -eq 'eh-cross-function-catch-ref') {
                $functionReferencesOff = @($caseBase | Where-Object {
                    $_ -ne '-WFE-function-references' }) + @('-WFD-function-references')
                try {
                    Check-Run "$stem-$policy-function-references-off" `
                        ($functionReferencesOff + @('--run', $gap)) $false 'function-references'
                } catch { $crossFailures.Add([string]$_) }
            }
        }
    }
    Require ($crossFailures.Count -eq 0) `
        "cross-function EH failures: $($crossFailures -join ' | ')"
    $result.passed = $true
} catch {
    $result.error = [string]$_.Exception.Message
}
$result.cases = @($script:rows.ToArray())
$json = $result | ConvertTo-Json -Depth 12 -Compress
[System.IO.File]::WriteAllText((Join-Path $work 'result.json'), $json,
    [System.Text.UTF8Encoding]::new($false))
Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' -Body ([System.Text.Encoding]::UTF8.GetBytes($json)) "$BaseUrl/result" | Out-Null
if (-not $result.passed) { exit 1 }
