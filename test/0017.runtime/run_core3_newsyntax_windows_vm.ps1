# Real Win11 LLVM-full Core 3 syntax test. The trusted host supplies exact
# PE/Wasm hashes; guest Wasm never receives the artifact-server capability.
param(
    [string]$BaseUrl = 'http://10.0.2.4:18024/uwvm-newsyntax-windows-x64',
    [string]$ArtifactRoot = '',
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:USERPROFILE 'Documents\uwvm-core3-newsyntax'
New-Item -ItemType Directory -Force $work | Out-Null
$script:rows = New-Object 'System.Collections.Generic.List[object]'
$result = [ordered]@{
    passed = $false
    source_id = $null
    product_sha256 = $null
    qualification_sha256 = $null
    oracle_sha256 = $null
    mode = 'llvm-jit-full'
    policies = @('instruction', 'unwind')
    os = [string](Get-CimInstance Win32_OperatingSystem).Caption
    os_version = [string](Get-CimInstance Win32_OperatingSystem).Version
    architecture = [string]$env:PROCESSOR_ARCHITECTURE
    cases = @()
}
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Get-VerifiedArtifact([string]$Name) {
    $property = $script:qualification.files_sha256.PSObject.Properties[$Name]
    Require ($null -ne $property) "qualification lacks $Name"
    $expected = [string]$property.Value
    Require ($expected -match '^[0-9a-fA-F]{64}$') "invalid $Name hash"
    $target = if ($ArtifactRoot) { Join-Path $ArtifactRoot $Name } else { Join-Path $work $Name }
    if (-not $ArtifactRoot) {
        Invoke-WebRequest -UseBasicParsing "$BaseUrl/$Name" -OutFile $target | Out-Null
    }
    Require ((Get-FileHash $target -Algorithm SHA256).Hash -ieq $expected) `
        "staged artifact changed: $Name"
    return $target
}
function Check-Run([string]$Name, [string[]]$Arguments, [bool]$Expected,
                   [string]$Diagnostic = '') {
    $log = Join-Path $work "$Name.log"
    & $script:uwvm @Arguments *> $log
    $code = $LASTEXITCODE
    $output = [System.IO.File]::ReadAllText($log)
    $plain = [regex]::Replace($output, '\x1b\[[0-9;]*[A-Za-z]', '')
    $pass = (($code -eq 0) -eq $Expected)
    if (-not $Expected -and $Diagnostic) {
        $pass = $pass -and $plain.ToLowerInvariant().Contains($Diagnostic)
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
        throw "$Name failed: exit=$code expected=$Expected diagnostic=$Diagnostic output=$($plain.Substring([Math]::Max(0, $plain.Length - 1200)))"
    }
}
try {
    $qualificationFile = Join-Path $work 'qualification.json'
    if ($ArtifactRoot) {
        Copy-Item -LiteralPath (Join-Path $ArtifactRoot 'qualification.json') -Destination $qualificationFile -Force
    } else {
        Invoke-WebRequest -UseBasicParsing "$BaseUrl/qualification.json" -OutFile $qualificationFile | Out-Null
    }
    Require ((Get-FileHash $qualificationFile -Algorithm SHA256).Hash -ieq $QualificationSha256) `
        'Core 3 new-syntax qualification changed'
    $script:qualification = Get-Content -Raw $qualificationFile | ConvertFrom-Json
    Require ($qualification.source_id -match '^sha256:[0-9a-fA-F]{64}$' -and
             $qualification.product_sha256 -match '^[0-9a-fA-F]{64}$' -and
             $qualification.oracle_sha256 -match '^[0-9a-fA-F]{64}$') `
        'Core 3 new-syntax qualification lacks exact source/PE/oracle hashes'
    $result.source_id = [string]$qualification.source_id
    $result.qualification_sha256 = (Get-FileHash $qualificationFile -Algorithm SHA256).Hash
    $result.oracle_sha256 = [string]$qualification.oracle_sha256
    $script:uwvm = Get-VerifiedArtifact 'uwvm.exe'
    $result.product_sha256 = (Get-FileHash $script:uwvm -Algorithm SHA256).Hash
    Require ($result.product_sha256 -ieq $qualification.product_sha256) `
        'Core 3 new-syntax PE differs from qualification'
    $cases = @(
        @{ stem = 'memory64'; features = @('memory64'); off = @(
            @{ feature = 'memory64'; diagnostic = 'memory64' }) },
        @{ stem = 'relaxed-simd'; features = @('simd', 'relaxed-simd'); off = @(
            @{ feature = 'relaxed-simd'; diagnostic = 'relaxed-simd' },
            @{ feature = 'simd'; diagnostic = 'illegal value type' }) },
        @{ stem = 'exnref-table64'; features = @('reference-types', 'table-instructions', 'exceptions', 'table64'); off = @(
            @{ feature = 'table64'; diagnostic = 'table64' },
            @{ feature = 'exceptions'; diagnostic = 'exceptions' },
            @{ feature = 'reference-types'; diagnostic = 'reference-types' },
            @{ feature = 'table-instructions'; diagnostic = 'table-instructions' }) },
        @{ stem = 'atomic-fence'; features = @('threads'); off = @(
            @{ feature = 'threads'; diagnostic = 'threads' }) }
    )
    foreach ($case in $cases) {
        Get-VerifiedArtifact "$($case.stem).wasm" | Out-Null
    }
    foreach ($policy in @('instruction', 'unwind')) {
        $engine = @('-Raot', '-Rct', '0', '-Rllvm-call-stack', $policy,
            '-Rllvm-cache-path', 'disable')
        foreach ($case in $cases) {
            $wasm = Join-Path $work "$($case.stem).wasm"
            $enabled = @($engine) + @($case.features | ForEach-Object { "-WFE-$_" })
            Check-Run "$($case.stem)-$policy" ($enabled + @('--run', $wasm)) $true
            foreach ($gate in $case.off) {
                $feature = [string]$gate.feature
                $disabled = @($enabled | Where-Object { $_ -ne "-WFE-$feature" }) +
                    @("-WFD-$feature")
                Check-Run "$($case.stem)-$policy-$feature-off" `
                    ($disabled + @('--run', $wasm)) $false ([string]$gate.diagnostic)
            }
        }
    }
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
