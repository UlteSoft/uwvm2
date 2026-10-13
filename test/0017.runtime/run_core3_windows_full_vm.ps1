# Run inside the controlled Windows x64 VM against an ordinary LLVM-full PE.
# qualification.json and every Wasm file are staged from independently
# Wasmtime-checked bytes and served through the immutable-hash artifact server.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18021/uwvm-core3-ordinary-x64',
    [string]$ArtifactRoot = '',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE 'Documents\uwvm-core3-ordinary-r38'
New-Item -ItemType Directory -Force $work | Out-Null
$script:rows = New-Object 'System.Collections.Generic.List[object]'
$result = [ordered]@{
    passed = $false
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
    $exitCode = $LASTEXITCODE
    $text = [System.IO.File]::ReadAllText($log)
    $diagnosticText = [regex]::Replace($text, '\x1b\[[0-9;]*[A-Za-z]', '')
    $passed = (($exitCode -eq 0) -eq $Expected)
    if (-not $Expected -and $Diagnostic) {
        $passed = $passed -and $diagnosticText.ToLowerInvariant().Contains($Diagnostic)
    }
    $script:rows.Add([ordered]@{
        name = $Name
        exit_code = $exitCode
        expected_success = $Expected
        diagnostic = $Diagnostic
        passed = [bool]$passed
        log_sha256 = (Get-FileHash $log -Algorithm SHA256).Hash
    })
    if (-not $passed) {
        throw "$Name failed: exit=$exitCode expected=$Expected diagnostic=$Diagnostic output=$text"
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
    Require ($qualification.source_id -match '^sha256:[0-9a-f]{64}$') `
        'Core 3 qualification lacks an exact source fingerprint'
    $result.source_id = [string]$qualification.source_id
    $result.llvm_certificate_sha256 = [string]$qualification.llvm_certificate_sha256
    $result.reference_wasmtime_sha256 = [string]$qualification.wasmtime_sha256
    $result.gap_oracle_sha256 = [string]$qualification.gap_oracle_sha256
    $result.qualification_sha256 = (Get-FileHash $qualificationFile -Algorithm SHA256).Hash

    $gcStems = @('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
        'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
        'gc_struct_into_anyref_table', 'i31_table64')
    $gapStems = @('ref-test-exn-null', 'ref-cast-exn-null',
        'br-on-cast-exn-null', 'exn-cast-retained', 'ref-cast-exn-null-trap',
        'exnref_call_retained', 'eh-cross-function-catch-ref',
        'eh-cross-function-win64-seh')
    $names = @('uwvm.exe', 'core3-eh-threads.wasm',
        'gc_const_expr_execution.wasm',
        'exnref_struct_payload_execution.wasm',
        'wasm3_initializers.wasm',
        'branch_semantics_br_if.wasm',
        'branch_semantics_br_on_null.wasm',
        'branch_semantics_br_on_non_null.wasm') +
        @($gcStems | ForEach-Object { "$($_).wasm" }) +
        @($gapStems | ForEach-Object { "$($_).wasm" })
    foreach ($name in $names) {
        $path = if ($ArtifactRoot -and $name -eq 'uwvm.exe') {
            Join-Path $ArtifactRoot $name
        } else {
            Join-Path $work $name
        }
        if ($ArtifactRoot -and $name -ne 'uwvm.exe') {
            Copy-Item -LiteralPath (Join-Path $ArtifactRoot $name) -Destination $path -Force
        } elseif (-not $ArtifactRoot) {
            Invoke-WebRequest -UseBasicParsing "$BaseUrl/$name" -OutFile $path
        }
        $expected = if ($name -eq 'uwvm.exe') {
            [string]$qualification.product_sha256
        } else {
            [string]($qualification.fixture_sha256.PSObject.Properties[$name].Value)
        }
        Require ($expected -match '^[0-9a-f]{64}$' -and
                 (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expected) `
            "Core 3 staging hash mismatch: $name"
    }
    $script:uwvm = if ($ArtifactRoot) { Join-Path $ArtifactRoot 'uwvm.exe' } else { Join-Path $work 'uwvm.exe' }
    $result.product_sha256 = (Get-FileHash $script:uwvm -Algorithm SHA256).Hash

    $crossFailures = New-Object 'System.Collections.Generic.List[string]'
    foreach ($policy in @('instruction', 'unwind')) {
        $base = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-gc', '-WFE-reference-types', '-WFE-table-instructions',
            '-WFE-bulk-memory', '-WFE-extended-const', '-WFD-exceptions',
            '-WFD-function-references', '-WFD-threads')
        foreach ($stem in $gcStems) {
            $wasm = Join-Path $work "$stem.wasm"
            $extra = if ($stem -eq 'i31_table64') { @('-WFE-table64') } else { @() }
            Check-Run "gc-$stem-$policy" ($base + $extra + @('--run', $wasm)) $true
            $gcOff = @($base | Where-Object { $_ -ne '-WFE-gc' }) + @('-WFD-gc')
            Check-Run "gc-$stem-$policy-gc-off" ($gcOff + $extra + @('--run', $wasm)) $false 'gc'
            if ($stem -eq 'i31_table64') {
                Check-Run "gc-$stem-$policy-table64-off" `
                    ($base + @('-WFD-table64', '--run', $wasm)) $false 'table64'
            }
        }
        $focus = Join-Path $work 'i31_into_anyref_table.wasm'
        $refOff = @($base | Where-Object { $_ -ne '-WFE-reference-types' }) +
            @('-WFD-reference-types')
        Check-Run "gc-reference-types-$policy-off" ($refOff + @('--run', $focus)) `
            $false 'reference-types'
        $tableOff = @($base | Where-Object { $_ -ne '-WFE-table-instructions' }) +
            @('-WFD-table-instructions')
        Check-Run "gc-table-instructions-$policy-off" ($tableOff + @('--run', $focus)) `
            $false 'table-instructions'

        # Core 3 GC constant initializers remain valid with the older extended-const
        # proposal disabled; the GC gate alone must control this new syntax.
        $constExpr = Join-Path $work 'gc_const_expr_execution.wasm'
        $constBase = @($base | Where-Object { $_ -ne '-WFE-extended-const' }) +
            @('-WFD-extended-const')
        Check-Run "gc-const-expr-$policy" ($constBase + @('--run', $constExpr)) $true
        $constGcOff = @($constBase | Where-Object { $_ -ne '-WFE-gc' }) +
            @('-WFD-gc')
        Check-Run "gc-const-expr-$policy-gc-off" `
            ($constGcOff + @('--run', $constExpr)) $false '--wasm-feature-enable-gc'

        $eh = Join-Path $work 'core3-eh-threads.wasm'
        $ehBase = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-exceptions', '-WFE-threads')
        Check-Run "eh-threads-$policy" ($ehBase + @('--run', $eh)) $true
        $exceptionsOff = @($ehBase | Where-Object { $_ -ne '-WFE-exceptions' }) +
            @('-WFD-exceptions')
        Check-Run "eh-threads-$policy-exceptions-off" `
            ($exceptionsOff + @('--run', $eh)) $false 'exceptions'
        $threadsOff = @($ehBase | Where-Object { $_ -ne '-WFE-threads' }) +
            @('-WFD-threads')
        Check-Run "eh-threads-$policy-threads-off" `
            ($threadsOff + @('--run', $eh)) $false 'threads'

        $exnref = Join-Path $work 'exnref_struct_payload_execution.wasm'
        $exnJit = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-exceptions', '-WFE-gc', '-WFD-function-references', '-WFD-threads')
        Check-Run "exnref-struct-jit-$policy" ($exnJit + @('--run', $exnref)) $true
        $exnOff = @($exnJit | Where-Object { $_ -ne '-WFE-exceptions' }) +
            @('-WFD-exceptions')
        Check-Run "exnref-struct-jit-$policy-exceptions-off" `
            ($exnOff + @('--run', $exnref)) $false '--wasm-feature-enable-exceptions'
        $exnGcOff = @($exnJit | Where-Object { $_ -ne '-WFE-gc' }) + @('-WFD-gc')
        Check-Run "exnref-struct-jit-$policy-gc-off" `
            ($exnGcOff + @('--run', $exnref)) $false '--wasm-feature-enable-gc'
        Check-Run "exnref-struct-jit-$policy-reference-types-off" `
            ($exnJit + @('-WFD-reference-types', '--run', $exnref)) `
            $false 'requires reference-types'

        # The retained-exception case reaches ref.test/ref.cast/br_on_cast with
        # a non-null exnref; the other cases cover each null carrier encoding.
        $gapBase = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-gc', '-WFE-exceptions')
        foreach ($stem in $gapStems) {
            $gapWasm = Join-Path $work "$stem.wasm"
            $caseBase = $gapBase
            if ($stem -eq 'eh-cross-function-catch-ref') {
                # ref.as_non_null in the rethrow path has an independent gate.
                $caseBase = $gapBase + @('-WFE-function-references')
            }
            if ($stem -eq 'ref-cast-exn-null-trap') {
                Check-Run "$stem-$policy" ($caseBase + @('--run', $gapWasm)) `
                    $false 'reference cast failed: value does not match target heap type'
                $trapText = [regex]::Replace(
                    [System.IO.File]::ReadAllText((Join-Path $work "$stem-$policy.log")),
                    '\x1b\[[0-9;]*[A-Za-z]', '')
                Require ($trapText.Contains('[fatal] Runtime crash') -and
                         $trapText.Contains('Call stack:') -and
                         $trapText.Contains('func_idx=')) `
                    "ref.cast failure lacks fatal VM call stack: $policy"
            } elseif ($stem.StartsWith('eh-cross-function-')) {
                try { Check-Run "$stem-$policy" ($caseBase + @('--run', $gapWasm)) $true }
                catch { $crossFailures.Add([string]$_) }
            } else {
                Check-Run "$stem-$policy" ($caseBase + @('--run', $gapWasm)) $true
            }
            $gapGcOff = @($caseBase | Where-Object { $_ -ne '-WFE-gc' }) +
                @('-WFD-gc')
            if ($stem.StartsWith('eh-cross-function-')) {
                try {
                    Check-Run "$stem-$policy-gc-off" `
                        ($gapGcOff + @('--run', $gapWasm)) $true
                } catch { $crossFailures.Add([string]$_) }
            } elseif ($stem -eq 'exnref_call_retained') {
                Check-Run "$stem-$policy-gc-off" `
                    ($gapGcOff + @('--run', $gapWasm)) $true
            } else {
                Check-Run "$stem-$policy-gc-off" `
                    ($gapGcOff + @('--run', $gapWasm)) $false '--wasm-feature-enable-gc'
            }
            $gapExceptionsOff = @($caseBase | Where-Object { $_ -ne '-WFE-exceptions' }) +
                @('-WFD-exceptions')
            if ($stem.StartsWith('eh-cross-function-')) {
                try {
                    Check-Run "$stem-$policy-exceptions-off" `
                        ($gapExceptionsOff + @('--run', $gapWasm)) $false '--wasm-feature-enable-exceptions'
                } catch { $crossFailures.Add([string]$_) }
            } else {
                Check-Run "$stem-$policy-exceptions-off" `
                    ($gapExceptionsOff + @('--run', $gapWasm)) $false '--wasm-feature-enable-exceptions'
            }
            if ($stem -eq 'eh-cross-function-catch-ref') {
                $functionReferencesOff = @($caseBase | Where-Object {
                    $_ -ne '-WFE-function-references' }) + @('-WFD-function-references')
                try {
                    Check-Run "$stem-$policy-function-references-off" `
                        ($functionReferencesOff + @('--run', $gapWasm)) $false 'function-references'
                } catch { $crossFailures.Add([string]$_) }
            }
        }

        # Standard Core 3 accepts this ref.func table initializer. UWVM's
        # separate table-initializer gate permits the older funcref signature
        # while its independent function-references extension is disabled.
        $initializer = Join-Path $work 'wasm3_initializers.wasm'
        $initBase = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-extended-const', '-WFE-table-initializer',
            '-WFD-function-references')
        Check-Run "ref-func-table-initializer-$policy" `
            ($initBase + @('--run', $initializer)) $true
        $extendedOff = @($initBase | Where-Object { $_ -ne '-WFE-extended-const' }) +
            @('-WFD-extended-const')
        Check-Run "ref-func-table-initializer-$policy-extended-const-off" `
            ($extendedOff + @('--run', $initializer)) $false 'extended-const'
        $initializerOff = @($initBase | Where-Object { $_ -ne '-WFE-table-initializer' }) +
            @('-WFD-table-initializer')
        Check-Run "ref-func-table-initializer-$policy-feature-off" `
            ($initializerOff + @('--run', $initializer)) $false 'table-initializer'

        # Each fixture invokes its probe with both 0 and 1 and traps if either
        # the taken or fallthrough result is wrong. br_if itself remains valid
        # with function references disabled; the two Core 3 br_on_* opcodes do
        # require that independent feature gate.
        $branchBase = @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full', '-Rct', '0',
            '-Rllvm-call-stack', $policy, '-Rllvm-cache-path', 'disable',
            '-WFE-function-references')
        $branchOff = @($branchBase | Where-Object { $_ -ne '-WFE-function-references' }) +
            @('-WFD-function-references')
        foreach ($stem in @('branch_semantics_br_if',
                            'branch_semantics_br_on_null',
                            'branch_semantics_br_on_non_null')) {
            $branchWasm = Join-Path $work "$stem.wasm"
            Check-Run "$stem-$policy" ($branchBase + @('--run', $branchWasm)) $true
            if ($stem -eq 'branch_semantics_br_if') {
                Check-Run "$stem-$policy-function-references-off" `
                    ($branchOff + @('--run', $branchWasm)) $true
            } else {
                Check-Run "$stem-$policy-function-references-off" `
                    ($branchOff + @('--run', $branchWasm)) $false 'function-references'
            }
        }
    }
    # Windows x64 auto must select its native unwind implementation for a
    # combined Core 3 exception/shared-memory atomic execution.
    $ehAuto = Join-Path $work 'core3-eh-threads.wasm'
    Check-Run 'eh-threads-auto' @('-m', 'run', '-Rcc', 'jit', '-Rcm', 'full',
        '-Rct', '0', '-Rllvm-call-stack', 'auto', '-Rllvm-cache-path', 'disable',
        '-Rclog', 'err', '-WFE-exceptions', '-WFE-threads', '--run', $ehAuto) $true
    $autoLog = [System.IO.File]::ReadAllText((Join-Path $work 'eh-threads-auto.log'))
    $autoText = [regex]::Replace($autoLog, '\x1b\[[0-9;]*[A-Za-z]', '')
    foreach ($token in @('call_stack=unwind', 'unwind_backend=win64-seh',
                         'unwind_replace_frames=yes', 'call_stack_frames=omit')) {
        Require ($autoText.Contains($token)) "auto unwind did not select native Win64 SEH: $token"
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
Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' `
    -Body ([System.Text.Encoding]::UTF8.GetBytes($json)) "$BaseUrl/result" | Out-Null
if (-not $result.passed) { exit 1 }
