#!/usr/bin/env python3
"""SOURCE ONLY: literal bootstrap text; no QMP, guest or host execution."""
import base64
import hashlib
import re


def fixed_keyboard_command(bootstrap_sha256, qualification_sha256, nonce):
    for value in (bootstrap_sha256, qualification_sha256, nonce):
        if not isinstance(value, str) or re.fullmatch('[0-9a-f]{64}', value) is None:
            raise ValueError('exact frozen bootstrap/qualification/nonce SHA required')
    # Only ready CD-ROM roots are examined. A unique mounted regular input
    # must match BOTH fixed hashes; data from the guest never becomes a command.
    script = (
        "$ErrorActionPreference='Stop';$matches=@();"
        "foreach($d in [IO.DriveInfo]::GetDrives()){"
        "if($d.DriveType -ne [IO.DriveType]::CDRom -or -not $d.IsReady){continue};"
        "$p=Join-Path $d.RootDirectory.FullName 'windows_raii_r5_serial_bootstrap.ps1';"
        "$q=Join-Path $d.RootDirectory.FullName 'qualification.json';"
        "if(-not (Test-Path -LiteralPath $p -PathType Leaf) -or "
        "-not (Test-Path -LiteralPath $q -PathType Leaf)){continue};"
        "if((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()"
        " -ceq '" + bootstrap_sha256 + "' -and "
        "(Get-FileHash -LiteralPath $q -Algorithm SHA256).Hash.ToLowerInvariant()"
        " -ceq '" + qualification_sha256 + "'){$matches+=,$d.RootDirectory.FullName}};"
        "if($matches.Count -ne 1){throw 'unique pinned read-only test CD-ROM required'};"
        "& (Join-Path $matches[0] 'windows_raii_r5_serial_bootstrap.ps1')"
        " -ArtifactRoot $matches[0] -QualificationSha256 '" + qualification_sha256 + "'"
        " -Nonce '" + nonce + "'"
    )
    # Microsoft documents UTF-16LE for Windows PowerShell -EncodedCommand.
    text = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand '
    text += base64.b64encode(script.encode('utf-16le')).decode('ascii')
    if len(text) > 4096 or re.fullmatch('[A-Za-z0-9+/= .-]+', text) is None:
        raise ValueError('fixed keyboard command extent/alphabet exceeded')
    return {'command': text,
            'decoded_script': script,
            'command_ascii_sha256': hashlib.sha256(text.encode('ascii')).hexdigest(),
            'decoded_script_utf16le_sha256': hashlib.sha256(script.encode('utf-16le')).hexdigest(),
            'execution_qualified': False,
            'guest_logged_in_desktop_qualified': False}
