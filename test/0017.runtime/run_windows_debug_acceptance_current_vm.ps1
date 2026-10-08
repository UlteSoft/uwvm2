# SOURCE ONLY. Run in the keeper-owned Windows11 x64 VM, never an r10 PE.
param(
    [Parameter(Mandatory=$true)][string]$ArtifactRoot,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$QualificationSha256,
    [ValidateSet('ordinary','ros')][string]$Repository = 'ordinary'
)
$ErrorActionPreference = 'Stop'
function Require([bool]$Ok,[string]$Why) { if(-not $Ok) { throw $Why } }
function Sha([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$work = Join-Path $env:USERPROFILE ('Documents\uwvm-current-debug-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $work | Out-Null
$result = [ordered]@{passed=$false;scope='fresh joint-full C DWARF5 source finish, Wasm and consecutive native steps; no full platform acceptance claim';repository=$Repository;cases=@();work=$work}
$rows = New-Object 'System.Collections.Generic.List[object]'
function Read-TextBounded([string]$Path) {
    if(-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    $stream = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        Require ($stream.Length -le 4194304) 'product stream exceeded hard evidence extent'
        $bytes = New-Object byte[] ([int]$stream.Length)
        $at=0
        while($at -lt $bytes.Length) {
            $read=$stream.Read($bytes,$at,$bytes.Length-$at)
            Require ($read -gt 0) 'product evidence unexpectedly truncated'
            $at += $read
        }
        return [Text.Encoding]::UTF8.GetString($bytes)
    } finally { $stream.Dispose() }
}
function Prompt($Session) {
    $clock=[Diagnostics.Stopwatch]::StartNew(); $marker='(uwvm-debug) '
    while($clock.ElapsedMilliseconds -lt 40000) {
        Require (-not $Session.owned.CaptureFaulted) 'owned launcher capture overflow'
        $text=Read-TextBounded $Session.stdout
        Require ($text.Length -ge $Session.cursor) 'regular product transcript was truncated between prompts'
        $at=$text.IndexOf($marker,$Session.cursor,[StringComparison]::Ordinal)
        if($at -ge 0) {
            $end=$at+$marker.Length; $reply=$text.Substring($Session.cursor,$end-$Session.cursor); $Session.cursor=$end
            return [regex]::Replace($reply,'\x1b\[[0-?]*[ -/]*[@-~]','')
        }
        Require (-not $Session.owned.HasExited) ('current product exited before prompt: '+(Read-TextBounded $Session.stderr))
        Start-Sleep -Milliseconds 10
    }
    throw 'actual prompt deadline expired'
}
function Send($Session,[string]$Command) {
    $Session.commands.Add($Command) | Out-Null
    $Session.owned.WriteInputLine($Command,5000)
    return Prompt $Session
}
function New-Console([string]$Policy,[string]$Scenario,[string]$Name) {
    $stdout=Join-Path $work ($Name+'.product.stdout.log'); $stderr=Join-Path $work ($Name+'.product.stderr.log')
    $argv=@($script:pe,$script:wasm,$stdout,$stderr,$Policy,$Repository,$Scenario)
    $command=Quote-CurrentDebugNativeArgument $script:launcher
    foreach($arg in $argv) { $command += ' '+(Quote-CurrentDebugNativeArgument $arg) }
    $owned=[UwvmWindowsDebugAcceptanceOwnedProcessR1]::Start($script:launcher,$command,$false)
    try { Require $owned.AssignedBeforeResume 'owned launcher did not enter job before execution' }
    catch { $owned.Dispose(); throw }
    return [pscustomobject]@{owned=$owned;stdout=$stdout;stderr=$stderr;cursor=0;commands=(New-Object 'System.Collections.Generic.List[string]');positions=(New-Object 'System.Collections.Generic.List[object]');argv=$argv;thread=[uint64]0;stop=[uint64]0;name=$Name}
}
function Retire($Session,[bool]$Success,$Row) {
    try {
        if(-not $Success) { $Session.owned.Kill() }
        $Session.owned.CloseInput()
        Require ($Session.owned.WaitForExit(15000)) 'owned launcher retirement deadline'
        $Row.exit_code=$Session.owned.ExitCode
        Require ($Session.owned.RetireJob(10000) -eq 0) 'actual product subtree did not retire'
        Require ($Session.owned.Drain(10000)) 'owned launcher capture drain failed'
        $Row.owned_job_active_after_retirement=0
        $Row.pid=$Session.owned.Id; $Row.creation_filetime=$Session.owned.CreationFileTime
        $Row.assigned_before_resume=$Session.owned.AssignedBeforeResume
        $Row.argv=$Session.argv; $Row.commands=@($Session.commands.ToArray()); $Row.positions=@($Session.positions.ToArray())
        [IO.File]::WriteAllBytes((Join-Path $work ($Session.name+'.launcher.stdout.log')),$Session.owned.StdoutBytes())
        [IO.File]::WriteAllBytes((Join-Path $work ($Session.name+'.launcher.stderr.log')),$Session.owned.StderrBytes())
        foreach($path in @($Session.stdout,$Session.stderr)) {
            Read-TextBounded $path | Out-Null
            if(Test-Path -LiteralPath $path) { $Row.streams += @{path=$path;sha256=(Sha $path);bytes=(Get-Item -LiteralPath $path).Length} }
        }
        if($Success) { Require ($Row.exit_code -eq 0) 'product guest result/console exit failed' }
    } finally { $Session.owned.Dispose() }
}
function Position($Session,[bool]$NeedSource) {
    $text=Send $Session ('bt '+$Session.thread)
    $thread=[regex]::Matches($text,'(?m)^thread ([0-9]+) module=([0-9]+) function=([0-9]+) byte-offset=([0-9]+) generation=([0-9]+)\r?$')
    $label=[regex]::Matches($text,'(?m)^stop-id ([0-9]+)\r?$')
    Require ($thread.Count -eq 1 -and $label.Count -eq 1) 'one actual current participant/stop label required'
    $m=$thread[0]; $id=[uint64]$m.Groups[1].Value; $module=[uint64]$m.Groups[2].Value; $fn=[uint64]$m.Groups[3].Value; $offset=[uint64]$m.Groups[4].Value
    $stop=[uint64]$label[0].Groups[1].Value
    Require ($id -eq $Session.thread -and $module -eq 0 -and $stop -gt 0 -and [uint64]$m.Groups[5].Value -gt 0) 'unrelated/unpublished current participant'
    $p=[ordered]@{function=$fn;offset=$offset;stop_id=$stop;raw=$text;is_statement=$false}
    $src=[regex]::Matches($text,'(?m)^  source (.+):([0-9]+):([0-9]+)\r?$')
    if($NeedSource -or $src.Count -gt 0) {
        Require ($src.Count -eq 1) 'actual current source row missing or ambiguous'
        Require ([IO.Path]::GetFileName($src[0].Groups[1].Value) -eq $script:oracle.source_basename) 'unrelated fixture source'
        $expr=$script:oracle.expressions.PSObject.Properties[[string]$fn]
        $extent=$script:oracle.expression_sizes.PSObject.Properties[[string]$fn]
        Require ($null -ne $expr -and $null -ne $extent -and $offset -lt [uint64]$extent.Value) 'actual local function/code extent not in official fixture'
        $pc=[uint64]$expr.Value+$offset; $matches=@()
        foreach($seq in $script:oracle.sequences) {
            if($seq.Count -gt 1 -and [uint64]$seq[0].address -le $pc -and $pc -lt [uint64]$seq[-1].address) {
                $active=@($seq | Where-Object { [uint64]$_.address -le $pc -and -not $_.end_sequence })
                if($active.Count -gt 0) { $matches += $active[-1] }
            }
        }
        Require ($matches.Count -eq 1) 'Code-relative PC has no unique official statement row'
        $r=$matches[0]; Require ([uint64]$r.line -eq [uint64]$src[0].Groups[2].Value -and [uint64]$r.column -eq [uint64]$src[0].Groups[3].Value) 'product disagrees with official line/column oracle'
        $p.line=[uint64]$r.line; $p.column=[uint64]$r.column; $p.code_offset=$pc; $p.discriminator=[uint64]$r.discriminator; $p.is_statement=[bool]$r.is_statement
    }
    $Session.positions.Add($p) | Out-Null; return $p
}
function Begin($Session,[uint64]$Function) {
    Prompt $Session | Out-Null
    Require ((Send $Session 'status').Contains('prepared; no Wasm instruction executed')) 'not actual debug-full initial state'
    $reply=Send $Session ('break 0 '+$Function+' 0'); $m=[regex]::Match($reply,'breakpoint ([0-9]+)')
    Require $m.Success 'real emitted entry breakpoint missing'; $breakpoint=$m.Groups[1].Value
    Send $Session 'continue' | Out-Null
    $found=$false
    for($i=0;$i -lt 20;++$i) {
        $reply=Send $Session 'wait'
        if($reply.Contains('stopped: breakpoint')) { $found=$true;break }
    }
    Require $found 'actual guest breakpoint missing'
    $m=[regex]::Match($reply,'thread ([0-9]+) module=0 function='+$Function+' byte-offset=')
    Require $m.Success 'actual target function/thread missing'
    $Session.thread=[uint64]$m.Groups[1].Value
    Send $Session ('delete '+$breakpoint) | Out-Null
}
function Seek-Statement($Session,[uint64]$Function,[uint64]$Line) {
    for($i=0;$i -lt 512;++$i) {
        $probe=Send $Session ('bt '+$Session.thread)
        if($probe.Contains("`n  source ")) {
            $p=Position $Session $true
            if($p.function -eq $Function -and $p.is_statement -and ($Line -eq 0 -or $p.line -eq $Line)) { return $p }
        }
        $reply=Send $Session ('step wasm '+$Session.thread)
        Require ($reply.Contains('stopped: selected participant step')) 'actual mapped origin search failed'
    }
    throw 'official fixture statement origin was not reached'
}
function Guest-Exit($Session) {
    Send $Session 'continue' | Out-Null
    for($i=0;$i -lt 20;++$i) { $text=Send $Session 'wait'; if($text.Contains('guest exited:')) { Require ($text.Contains('guest exited: 0')) 'fixture 52/58 computation failed'; return } }
    throw 'guest result was not observed'
}
try {
    Require ([IntPtr]::Size -eq 8 -and $env:PROCESSOR_ARCHITECTURE -eq 'AMD64') 'actual Windows x64 required'
    $qpath=Join-Path $ArtifactRoot 'qualification.json'; Require ((Sha $qpath) -eq $QualificationSha256.ToLowerInvariant()) 'qualification bytes changed'
    $q=Get-Content -LiteralPath $qpath -Raw | ConvertFrom-Json
    Require ($q.schema -eq 3 -and $q.purpose -eq 'current-joint-debug-full-c5-v1' -and $q.repository -eq $Repository -and $q.source_id -match '^sha256:[0-9a-f]{64}$') 'current joint source/full build qualification required; r10 unsupported'
    Require ($q.source_before_sha256 -eq $q.source_after_sha256 -and $q.source_before_sha256 -match '^[0-9a-f]{64}$' -and $q.production_pins.controller -match '^[0-9a-f]{64}$' -and $q.production_pins.activation_api -match '^[0-9a-f]{64}$') 'actual complete fresh build/source pins missing'
    $required=@('uwvm.exe','windows_debug_current_launcher.exe','windows_debug_current_owned_process.ps1','run_windows_debug_acceptance_current_vm.ps1','c-dwarf5-O1.wasm','c-dwarf5-O1.oracle.json','build-receipt.json')
    foreach($name in $required) { Require ($q.files_sha256.PSObject.Properties[$name] -and (Sha (Join-Path $ArtifactRoot $name)) -eq [string]$q.files_sha256.PSObject.Properties[$name].Value) ('artifact mismatch '+$name) }
    Require ((Sha $PSCommandPath) -eq $q.files_sha256.'run_windows_debug_acceptance_current_vm.ps1') 'actual guest driver differs'
    $build=Get-Content -LiteralPath (Join-Path $ArtifactRoot 'build-receipt.json') -Raw | ConvertFrom-Json
    Require ($build.schema -eq 1 -and $build.purpose -eq 'actual-current-win64-debug-full-build' -and $build.repository -eq $Repository -and $build.target -eq 'x86_64-w64-windows-gnu' -and $build.source_id -eq $q.source_id) 'actual original build receipt identity differs from qualification'
    Require ($build.product_link.returncode -eq 0 -and $build.launcher_build.returncode -eq 0 -and $build.product_link.output_sha256 -eq $q.files_sha256.'uwvm.exe' -and $build.launcher_build.output_sha256 -eq $q.files_sha256.'windows_debug_current_launcher.exe') 'original linked product/launcher differ from staged artifacts'
    Require ($build.memory_max -eq '68719476736' -and $build.memory_swap_max -eq '0' -and $build.cpuset -eq '0,2,4,6,16-31' -and $build.oom_before -eq 0 -and $build.oom_after -eq 0 -and $build.oom_kill_before -eq 0 -and $build.oom_kill_after -eq 0) 'actual original build resource proof differs'
    foreach($pin in @('controller','activation_api','joint_source_activation','source_api','debug_host_wrapper')) { Require ($q.production_pins.PSObject.Properties[$pin] -and $q.production_pins.PSObject.Properties[$pin].Value -match '^[0-9a-f]{64}$' -and $build.production_pins.PSObject.Properties[$pin] -and $build.production_pins.PSObject.Properties[$pin].Value -eq $q.production_pins.PSObject.Properties[$pin].Value) ('current production source pin missing or different: '+$pin) }
    foreach($dll in $q.non_system_dlls) { Require ($dll.name -match '^[A-Za-z0-9_.+-]+\.dll$' -and (Sha (Join-Path $ArtifactRoot $dll.name)) -eq $dll.sha256) 'actual LLVM/runtime DLL closure mismatch' }
    $script:pe=Join-Path $ArtifactRoot 'uwvm.exe'; $script:launcher=Join-Path $ArtifactRoot 'windows_debug_current_launcher.exe'; $script:wasm=Join-Path $ArtifactRoot 'c-dwarf5-O1.wasm'
    $script:oracle=Get-Content -LiteralPath (Join-Path $ArtifactRoot 'c-dwarf5-O1.oracle.json') -Raw | ConvertFrom-Json
    Require ($script:oracle.schema -eq 1 -and $script:oracle.wasm_sha256 -eq (Sha $script:wasm) -and $script:oracle.actual_import_policy -eq 'reject-all') 'actual official fixture oracle required'
    $result.qualification_sha256=(Sha $qpath); $result.source_id=$q.source_id; $result.product_sha256=(Sha $script:pe)
    . (Join-Path $ArtifactRoot 'windows_debug_current_owned_process.ps1')
    Initialize-CurrentDebugOwnedProcessTypes
    foreach($policy in @('instruction','unwind')) {
        foreach($kind in @('source-finish','wasm-asm')) {
            $s=$null; $success=$false; $row=[ordered]@{name=$policy+'-'+$kind;passed=$false;streams=@()}
            try {
                $s=New-Console $policy 'full' $row.name
                $origin=if($kind -eq 'source-finish'){[uint64]$script:oracle.functions.leaf}else{[uint64]$script:oracle.functions.outer}
                Begin $s $origin
                $before=Seek-Statement $s $origin $(if($kind -eq 'source-finish'){[uint64]$script:oracle.leaf_entry_line}else{0})
                if($kind -eq 'source-finish') {
                    $step=Send $s ('step source '+$s.thread+' out')
                    Require ($step.Contains('stopped: selected participant step') -and -not $step.Contains('source stepping unavailable')) 'actual current producer source finish rejected'
                    $after=Position $s $true
                    Require ($after.function -eq [uint64]$script:oracle.functions.outer -and $after.is_statement -and $after.stop_id -gt $before.stop_id -and -not $after.raw.Contains("`n  inline ")) 'finish did not reach its genuine current caller statement'
                    Guest-Exit $s
                } else {
                    $step=Send $s ('step wasm '+$s.thread)
                    Require ($step.Contains('stopped: selected participant step')) 'real Wasm step rejected'
                    $after=Position $s $false; Require ($after.stop_id -gt $before.stop_id) 'Wasm step reused a stopped episode'
                    $previousTo=''
                    for($i=0;$i -lt 2;++$i) {
                        $step=Send $s ('step asm '+$s.thread)
                        $ins=[regex]::Match($step,'native instruction 0x([0-9a-fA-F]+) bytes=([0-9a-fA-F]{2}(?: [0-9a-fA-F]{2})*)  ([^\r\n]+) -> 0x([0-9a-fA-F]+)')
                        Require ($ins.Success -and $step.Contains('stopped: native instruction step')) 'actual decoded native instruction/trap missing'
                        if($previousTo){Require ($ins.Groups[1].Value -ieq $previousTo) 'second native step rearmed a different PC'}
                        $previousTo=$ins.Groups[4].Value
                        $native=Position $s $false
                        Require ($native.stop_id -gt $after.stop_id -and $native.raw.Contains('native trap has no current Wasm source position') -and -not $native.raw.Contains("`n  source ")) 'native trap reused Wasm source metadata'
                        $pc=[regex]::Matches($native.raw,'(?m)^  native-pc=0x([0-9a-fA-F]+)\r?$')
                        Require ($pc.Count -eq 1 -and $pc[0].Groups[1].Value -ieq $previousTo) 'actual native trap PC differs from the decoded single-step result'
                        $after=$native
                    }
                    $rejected=Send $s ('step source '+$s.thread+' into')
                    Require ($rejected.Contains('source stepping unavailable') -and -not $rejected.Contains('stopped: selected participant step')) 'native trap granted source-step authority'
                    Guest-Exit $s
                }
                # quit exits the real console without producing another prompt.
                $s.commands.Add('quit') | Out-Null
                $s.owned.WriteInputLine('quit',5000)
                $success=$true
            } catch { $row.error=[string]$_; throw }
            finally { if($s){try{Retire $s $success $row}catch{$row.retirement_error=[string]$_;$success=$false}}; $row.passed=$success; $rows.Add($row) }
            Require $success 'complete owned case retirement failed'
        }
    }
    $s=$null; $row=[ordered]@{name='unsupported-mode';passed=$false;streams=@()}
    try {
        $s=New-Console 'instruction' 'unsupported' 'unsupported-mode'; $s.owned.CloseInput()
        Require ($s.owned.WaitForExit(15000)) 'unsupported mode did not reject early'
        $text=(Read-TextBounded $s.stdout)+(Read-TextBounded $s.stderr); $plain=[regex]::Replace($text,'\x1b\[[0-?]*[ -/]*[@-~]','')
        $code=$s.owned.ExitCode; $expected=if($Repository -eq 'ros'){'uwvm-int/full'}else{'llvm-jit/lazy'}
        Require ($code -gt 0 -and $code -le 255 -and $plain.Contains('[fatal]') -and $plain.Contains('unsupported in the current mode: '+$expected)) 'crash/parser/missing-DLL is not fatal unsupported-mode PASS'
        Require ($s.owned.RetireJob(10000) -eq 0 -and $s.owned.Drain(10000)) 'negative case subtree not retired'
        $row.exit_code=$code; $row.passed=$true; $row.raw=$plain; $row.owned_job_active_after_retirement=0
    } catch {$row.error=[string]$_;throw}
    finally {
        if($s){
            try {
                $s.owned.CloseInput()
                Require ($s.owned.RetireJob(10000) -eq 0) 'negative case subtree retirement failed'
                Require ($s.owned.WaitForExit(15000) -and $s.owned.Drain(10000)) 'negative case launcher/capture did not retire'
                $row.exit_code=$s.owned.ExitCode; $row.owned_job_active_after_retirement=0
                $row.pid=$s.owned.Id; $row.creation_filetime=$s.owned.CreationFileTime; $row.assigned_before_resume=$s.owned.AssignedBeforeResume; $row.argv=$s.argv
                [IO.File]::WriteAllBytes((Join-Path $work 'unsupported-mode.launcher.stdout.log'),$s.owned.StdoutBytes())
                [IO.File]::WriteAllBytes((Join-Path $work 'unsupported-mode.launcher.stderr.log'),$s.owned.StderrBytes())
                foreach($path in @($s.stdout,$s.stderr)){Read-TextBounded $path | Out-Null;if(Test-Path -LiteralPath $path){$row.streams += @{path=$path;sha256=(Sha $path);bytes=(Get-Item -LiteralPath $path).Length}}}
            } catch {$row.retirement_error=[string]$_;$row.passed=$false;throw}
            finally {$s.owned.Dispose()}
        }
        $rows.Add($row)
    }
    foreach($name in $required) { Require ((Sha (Join-Path $ArtifactRoot $name)) -eq [string]$q.files_sha256.PSObject.Properties[$name].Value) ('artifact changed during VM '+$name) }
    foreach($dll in $q.non_system_dlls) { Require ((Sha (Join-Path $ArtifactRoot $dll.name)) -eq $dll.sha256) 'DLL changed during VM cases' }
    $result.passed=$true
} catch {$result.error=[string]$_}
$result.cases=@($rows.ToArray())
[IO.File]::WriteAllText((Join-Path $work 'result.json'),($result | ConvertTo-Json -Depth 20),(New-Object Text.UTF8Encoding($false)))
[Console]::WriteLine('Evidence directory: '+$work)
if(-not $result.passed){exit 1}
