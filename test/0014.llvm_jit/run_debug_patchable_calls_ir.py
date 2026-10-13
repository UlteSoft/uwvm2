#!/usr/bin/env python3
"""Actual debug-full live-call IR and x86-64 executable-object qualification.

The ordinary full-JIT object is compared with a qualified baseline. Debug-full
intentionally resolves live tables instead of reading the former compact view.
Neither probe publishes replacement code. All tests run in the cgroup.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
from elf_executable_sections import compare_executable_images, normalize_relocations, write_executable_evidence


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe',type=Path,required=True)
    p.add_argument('--baseline-probe',type=Path)
    p.add_argument('--baseline-dir',type=Path,help='retained qualified probe output for both call-stack policies')
    p.add_argument('--guard',type=Path,required=True)
    p.add_argument('--llvm',type=Path,default=Path('/toolchain/bin'))
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    if bool(a.baseline_probe)==bool(a.baseline_dir):
        p.error('pass exactly one of --baseline-probe or --baseline-dir')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    subprocess.run(['bash',str(a.guard)],check=True)
    a.out.mkdir(parents=True,exist_ok=False)
    commands=[]; rows=[]
    def run(command,log):
        command=list(map(str,command)); r=subprocess.run(command,capture_output=True,timeout=180)
        log.write_bytes(r.stdout);log.with_suffix(log.suffix+'.stderr').write_bytes(r.stderr)
        commands.append({'command':command,'exit':r.returncode})
        (a.out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        r.check_returncode();return r.stdout.decode()
    def native(path):
        optimized=path.with_suffix('.optimized.ll'); obj=path.with_suffix('.o')
        run([a.llvm/'opt','-passes=default<O3>','-S',path,'-o',optimized],path.with_suffix('.opt.log'))
        run([a.llvm/'llc','-O3','-filetype=obj','-code-model=large','-relocation-model=pic',optimized,'-o',obj],path.with_suffix('.llc.log'))
        reloc=run([a.llvm/'llvm-readobj','--relocations',obj],path.with_suffix('.relocations.txt'))
        asm=run([a.llvm/'llvm-objdump','-dr',obj],path.with_suffix('.assembly.txt'))
        sections=path.parent/(path.stem+'-sections');sections.mkdir()
        return write_executable_evidence(obj,sections),normalize_relocations(reloc),asm
    def retained_native(path, output):
        obj=path.with_suffix('.o')
        reloc=path.with_suffix('.relocations.txt').read_text()
        sections=output/(path.stem+'-baseline-sections');sections.mkdir()
        return write_executable_evidence(obj,sections),normalize_relocations(reloc)
    def live_resolver_symbol(ir):
        symbols={match.group(1) for line in ir.splitlines()
                 if 'call i64 @uwvm_bridge_' in line and '%debug.call_indirect.target.address)' in line
                 if (match:=re.search(r'@(uwvm_bridge_[0-9a-f_]+)\(',line))}
        if len(symbols)!=1:raise RuntimeError('missing unique debug live-table resolver bridge')
        return symbols.pop()
    for policy in ['instruction','unwind']:
        current=a.out/policy;current.mkdir()
        old=a.out/('baseline-'+policy) if a.baseline_probe else a.baseline_dir/policy
        if a.baseline_probe:old.mkdir()
        run([a.probe,current,policy],current/'probe.log')
        if a.baseline_probe:run([a.baseline_probe,old,policy],old/'probe.log')
        if (current/'fixture.wasm').read_bytes()!=(old/'fixture.wasm').read_bytes():
            raise RuntimeError('baseline and candidate guest bytes differ')
        images={};normal_relocations='';debug_resolver=''
        for name in ['off-normal','off-debug']:
            path=current/(name+'.ll');new=native(path)
            raw_ir=path.read_text()
            if '_debug_full_typed_targets' in new[1] or 'call.debug.full.target' in raw_ir:
                raise RuntimeError('disabled mode contains patchable table code')
            if name=='off-normal':
                prior=native(old/(name+'.ll')) if a.baseline_probe else retained_native(old/(name+'.ll'),current)
                compare_executable_images(new[0],prior[0])
                if new[1]!=prior[1]:raise RuntimeError('ordinary full complete relocations changed')
                if 'debug.call_indirect.target.slot' in raw_ir:
                    raise RuntimeError('ordinary full unexpectedly uses debug live resolver')
                normal_relocations=new[1]
            elif raw_ir.count('debug.call_indirect.target.slot')<2 or 'call_indirect.table_view' in raw_ir:
                raise RuntimeError('debug-full fallback did not resolve the live table')
            else:
                debug_resolver=live_resolver_symbol(raw_ir)
                if debug_resolver in normal_relocations or debug_resolver not in new[1] or debug_resolver not in new[2]:
                    raise RuntimeError('debug fallback live-table resolver missing from actual machine object')
            images[name]=new[0].summary()
        enabled=native(current/'on-debug.ll')
        if '_debug_full_typed_targets' not in enabled[1]:raise RuntimeError('enabled object has no actual target table relocation')
        raw_enabled=(current/'on-debug.ll').read_text()
        if raw_enabled.count('debug.call_indirect.target.slot')<2 or 'call_indirect.table_view' in raw_enabled:
            raise RuntimeError('patchable debug-full did not resolve the live table')
        if live_resolver_symbol(raw_enabled)!=debug_resolver or debug_resolver not in enabled[1] or debug_resolver not in enabled[2]:
            raise RuntimeError('patchable debug-full live-table resolver missing from actual machine object')
        optimized=(current/'on-debug.optimized.ll').read_text()
        if not re.search(r'load atomic volatile i\d+, ptr .* acquire, align',optimized):
            raise RuntimeError('optimizer removed acquire/volatile target loads')
        if enabled[0].machine!=62:raise RuntimeError('assembly-specific runner currently qualifies ELF x86-64 only')
        # Ignore safe-point/stack helper calls: the final transfer in each tail
        # body must still be a native indirect jump, including former self loops.
        native_tails=[]
        for index in [2,3,5,8]:
            match=re.search(r'(?ms)^[0-9a-f]+ <uwvm_m_[0-9a-f]+_func_'+str(index)+r'>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',enabled[2])
            if not match or not re.search(r'\bjmpq?\s+\*',match.group(1)):
                raise RuntimeError(f'function {index} has no actual indirect tail jump')
            native_tails.append(index)
        rows.append({'policy':policy,'ordinary_full_executable_and_relocations_identical':True,
                     'debug_full_live_table_resolver':True,'resolver_symbol':debug_resolver,
                     'disabled_objects':images,'enabled_object':enabled[0].summary(),'native_tail_functions':native_tails})
    subprocess.run(['bash',str(a.guard)],check=True)
    summary={'passed':True,'probe_sha256':hashlib.file_digest(a.probe.open('rb'),'sha256').hexdigest(),
             'baseline_sha256':hashlib.file_digest(a.baseline_probe.open('rb'),'sha256').hexdigest() if a.baseline_probe else
                 hashlib.file_digest((a.baseline_dir/'summary.json').open('rb'),'sha256').hexdigest(),
             'checks':rows,'scope':'Actual Wasm translation, LLVM verification and O3 ELF x86-64 objects; debug-full live-table routing and ordinary full code identity, without replacement publication or retirement.'}
    (a.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('PASS exact typed slots, live-table resolver, self/cross/indirect musttail, mode/bounds rejection and unchanged ordinary-full machine code')


if __name__=='__main__':main()
