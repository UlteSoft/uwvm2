# SOURCE ONLY: fixed read-only CD-ROM bootstrap for standalone Windows tests.
# The Linux controller types only a pinned UTF-16LE EncodedCommand. It owns
# QMP, the guest process tree and VM retirement; this script is not a debugger.
param(
    [Parameter(Mandatory=$true)][string]$ArtifactRoot,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$QualificationSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{64}$')][string]$Nonce
)

$ErrorActionPreference = 'Stop'
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function PinnedRegular([string]$Name, [string]$Expected, [long]$Maximum) {
    Require ($Name -cmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,120}$') 'flat bootstrap input required'
    Require ($Expected -cmatch '^[0-9a-f]{64}$') 'exact bootstrap input hash required'
    $path = Join-Path $ArtifactRoot $Name
    $item = Get-Item -LiteralPath $path
    Require (-not $item.PSIsContainer -and
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0 -and
        $item.Length -gt 0 -and $item.Length -le $Maximum) 'bounded regular bootstrap input required'
    Require ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ceq $Expected) 'bootstrap input hash changed'
    return $path
}

$root = [IO.Path]::GetFullPath($ArtifactRoot)
$drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($root))
Require ($drive.DriveType -eq [IO.DriveType]::CDRom -and $drive.IsReady) 'actual mounted CD-ROM required'
Require ($root.TrimEnd('\') -ceq $drive.RootDirectory.FullName.TrimEnd('\')) 'bootstrap must use the CD-ROM root'
$qualificationPath = PinnedRegular 'qualification.json' $QualificationSha256 1048576
$qualification = Get-Content -Raw -LiteralPath $qualificationPath | ConvertFrom-Json
Require ($qualification.schema -ceq 'uwvm-windows-raii-r5-paired-launcher-guest-qualification-v1' -and
    [string]$qualification.nonce -ceq $Nonce) 'wrong fixed bootstrap qualification'
$runner = PinnedRegular 'run_windows_raii_r5_paired_launcher_vm.ps1' ([string]$qualification.runner_sha256) 65536
& $runner -ArtifactRoot $root -QualificationSha256 $QualificationSha256 -Nonce $Nonce
# The host accepts only completed case facts from the nonce frame. It cannot
# infer this script exit or later SerialPort disposal from that earlier frame.
exit $LASTEXITCODE
