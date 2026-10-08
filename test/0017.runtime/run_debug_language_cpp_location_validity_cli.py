#!/usr/bin/env python3
"""A bad optimized static C++ this location must be unavailable, not a value.

This is a refusal regression, not a complete native-language capability pass.
The original optimized producer module, source and assertions stay unchanged.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
import run_debug_source_step_cli as step
import run_debug_language_experience_cli as language

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm','wasm','source','oracle','out'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--diagnostic-policy',choices=('instruction','unwind'),required=True)
    ap.add_argument('--ros',action='store_true');a=ap.parse_args()
    assert sys.platform=='linux'
    subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    a.out.mkdir(parents=True,exist_ok=False)
    paths=[a.uwvm,a.wasm,a.source,a.oracle,Path(__file__),Path(step.__file__),Path(language.__file__)]
    before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    d={'passed':False,'native_optimized_this_values_available':False,'original_producer_unchanged':True,
       'inputs_before':before,'actions':[],'positions':[]};s=None
    try:
        oracle=a.oracle.read_text()
        assert 'DW_AT_object_pointer' in oracle and 'DW_OP_WASM_location 0x0 0x0, DW_OP_stack_value' in oracle
        mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
        argv=[str(a.uwvm),'-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',a.diagnostic_policy,'-Rllvm-cache-path','disable','--run',str(a.wasm)]
        s=step.Session(argv,a.out/'console.log',step.code_expressions(a.wasm),step.line_sequences(oracle),a.source)
        d.update(argv=argv,actions=s.actions,positions=s.positions)
        target,_=step.metadata_cli.function(a.wasm,'numeric_probe');s.begin(target)
        wanted=next(i for i,line in enumerate(a.source.read_text().splitlines(),1) if '/* NUMERIC_READY */' in line)
        origin=language.own_seek(s,lambda p:p['function']==target and p['is_statement'] and p['line']<wanted,'actual source before invalid-location stop')
        reply=language.query(s,f'break-source 0 {origin["file"]}:{wanted}');bid=re.search(rb'breakpoint (\d+)',reply);assert bid
        s.send('continue')
        for _ in range(20):
            reply=s.send('wait')
            if b'stopped: breakpoint' in reply:break
        else:raise AssertionError('actual optimized stop missing')
        position=s.position();assert position['function']==target and position['line']==wanted and position['is_statement']
        locals_reply=language.query(s,'locals '+str(s.thread))
        assert re.search(rb'^local 0 i32=7\r?$',locals_reply,re.M),locals_reply
        d['actual_reused_Wasm_local_0']=locals_reply.decode()
        table=language.query(s,'info breakpoints')
        for expression in ('this','*this','this->value','this->value + 1'):
            reply=language.query(s,f'print {s.thread} {position["stop_id"]} {expression}')
            assert b'error:' in reply and b'source-value stop=' not in reply,(expression,reply)
            d.setdefault('rejected_unsafe_locations',[]).append({'expression':expression,'reply':reply.decode()})
        assert table==language.query(s,'info breakpoints')
        metadata=language.query(s,f'ptype {s.thread} {position["stop_id"]} this')
        assert b' type=const LanguageThis * kind=pointer' in metadata,metadata
        d['type_metadata_preserved']=metadata.decode()
        for expression,expected in [('amount',4),('cookie',11),('decimal32',1.25)]:
            reply=language.command_value(s,position,expression)
            values=re.findall(rb'(?:, value=| value=| = [ifu]\d+=)([-+]?\d+(?:\.\d+)?)\r?$',reply,re.M)
            assert len(values)==1 and float(values[0])==expected,(expression,reply)
        s.send('delete '+bid[1].decode());s.finish_guest();d['passed']=True
    except BaseException as e:d['error']=repr(e);raise
    finally:
        if s is not None:
            try:s.console.finish()
            except BaseException as e:d.update(passed=False,close_error=repr(e));raise
            finally:d.update(quit_returncode=s.console.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in s.console.transcript)
        d['inputs_after']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        if d['inputs_after']!=before:d.update(passed=False,inputs_changed=True)
        (a.out/'summary.json').write_text(json.dumps(d,indent=2)+'\n')
    assert d['passed'];print('PASS real optimized C++ this location refusal, valid peer values and guest result/worker join')

if __name__=='__main__':main()
