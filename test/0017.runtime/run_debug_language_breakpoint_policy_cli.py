#!/usr/bin/env python3
"""Actual repeated guest calls qualify breakpoint ignore/toggle/step interruption."""
from __future__ import annotations
import argparse,json,re,subprocess,sys
from pathlib import Path
import run_debug_source_step_cli as step
import run_debug_source_inline_metadata_cli as meta
import run_debug_language_experience_cli as language


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm','wasm','source','oracle','out'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--ros',action='store_true')
    ap.add_argument('--case',choices=('ignore-toggle','next-interrupt'),required=True)
    ap.add_argument('--diagnostic-policy',choices=('instruction','unwind'),default='instruction')
    a=ap.parse_args();root=Path(__file__).resolve().parents[2]
    if sys.platform!='linux':raise RuntimeError('designated Linux cgroup only')
    subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(parents=True,exist_ok=False)
    inputs=[a.uwvm,a.wasm,a.source,a.oracle,Path(__file__),Path(step.__file__),Path(meta.__file__),Path(language.__file__)]
    before={str(p):step.sha(p) for p in inputs};record={'passed':False,'inputs_before':before,'case':a.case};s=None
    try:
        mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
        argv=[str(a.uwvm),'-m','debug-jit',*mode,'-Rct','0','-Rllvm-call-stack',a.diagnostic_policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm)]
        s=step.Session(argv,a.out/'console.log',step.code_expressions(a.wasm),step.line_sequences(a.oracle.read_text()),a.source)
        record.update(argv=argv,actions=s.actions,positions=s.positions)
        leaf=meta.function(a.wasm,'policy_leaf')[0];outer=meta.function(a.wasm,'policy_outer')[0]
        def breakpoint(function):
            reply=s.send(f'break 0 {function} 0');m=re.search(rb'breakpoint (\d+)',reply);step.require(m is not None,'actual breakpoint required',reply);return int(m[1])
        def policy(command):
            reply=language.query(s,command);step.require(b'breakpoint policy updated' in reply,'actual breakpoint policy rejected',(command,reply));return reply
        def wait_stop(function):
            for _ in range(20):
                reply=s.send('wait')
                if b'stopped: breakpoint' in reply:break
            else:raise AssertionError('real breakpoint did not stop')
            m=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=',reply)
            step.require(m is not None and int(m[2])==function,'different guest activation stopped',reply);s.thread=int(m[1])
        def parameter(expected):
            ready=[i for i,l in enumerate(a.source.read_text().splitlines(),1) if 'POLICY_LEAF_READY' in l][0]
            p=language.own_seek(s,lambda p:p['function']==leaf and p['line']==ready and p['is_statement'],'actual initialized leaf scope')
            language.scalar(s,p,'value',expected);language.scalar(s,p,'cookie',expected+10);return p
        if a.case=='ignore-toggle':
            bid=breakpoint(leaf);policy(f'disable {bid}');text=language.query(s,'info breakpoints')
            step.require(b'enabled=no hits=0 ignore=0' in text,'disabled breakpoint state missing',text)
            policy(f'ignore {bid} 2');policy(f'enable {bid}');s.send('continue');wait_stop(leaf)
            p=parameter(2);text=language.query(s,'info breakpoints');step.require(b'enabled=yes hits=3 ignore=0' in text,'ignore count did not consume exactly two genuine hits',text)
            prior=text;bad=language.query(s,'ignore 18446744073709551615 1')
            step.require(b'error:' in bad and prior==language.query(s,'info breakpoints'),'invalid breakpoint ID mutated a valid policy',bad)
            policy('disable');text=language.query(s,'info breakpoints');step.require(b'enabled=no hits=3 ignore=0' in text,'disable all lost real hit count',text)
            s.finish_guest();text=language.query(s,'info breakpoints');step.require(b'hits=3 ignore=0' in text,'disabled fourth hit changed count',text)
        else:
            s.begin(outer)
            marker=[i for i,l in enumerate(a.source.read_text().splitlines(),1) if 'POLICY_PHYSICAL_CALL' in l][0]
            p=language.own_seek(s,lambda p:p['function']==outer and p['line']==marker and p['is_statement'],'genuine caller call statement')
            bid=breakpoint(leaf)
            reply=s.send(f'step source {s.thread} over')
            step.require(b'stopped: breakpoint' in reply,'source next missed the real child breakpoint',reply)
            p=parameter(0);text=language.query(s,'info breakpoints');step.require(b'hits=1 ignore=0' in text,'source next breakpoint hit count invalid',text)
            policy(f'disable {bid}');s.finish_guest()
        record['passed']=True
    except BaseException as e:record['error']=repr(e);raise
    finally:
        if s is not None:
            try:s.console.finish()
            except BaseException as e:record.update(passed=False,close_error=repr(e));raise
            finally:record.update(quit_returncode=s.console.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in s.console.transcript)
        record['inputs_after']={str(p):step.sha(p) for p in inputs}
        if before!=record['inputs_after']:record.update(passed=False,inputs_changed=True)
        (a.out/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
    if not record['passed']:raise AssertionError('breakpoint policy qualification failed')
    print('PASS actual breakpoint policy '+a.case)


if __name__=='__main__':main()
