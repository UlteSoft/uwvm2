#!/usr/bin/env python3
"""Real guest language numeric expressions and Objective-C selector dispatch.

Every value is queried through the actual stopped product. Official LLVM line
rows and original compiler output supply the oracle; no DWARF/guest mutation.
Run only in the designated Linux test cgroup. Missing capabilities fail.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import run_debug_source_step_cli as step
import run_debug_source_tinygo_cli as tinygo
import run_debug_language_experience_cli as language


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm','wasm','source','oracle','out'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--language',choices=('c','cpp','rust','objc','tinygo'),required=True)
    ap.add_argument('--ros',action='store_true')
    ap.add_argument('--conditional-subset',action='store_true',help='also check finite C-style numeric ?: at the actual stop; C/C++/Objective-C only')
    ap.add_argument('--address-bits',type=int,choices=(32,64),default=32)
    ap.add_argument('--diagnostic-policy',choices=('instruction','unwind'),default='instruction')
    a=ap.parse_args();root=Path(__file__).resolve().parents[2]
    if a.conditional_subset and a.language not in ('c','cpp','objc'):
        raise ValueError('conditional subset is C-style syntax; Rust/Go have no native ?: expression')
    if sys.platform!='linux':raise RuntimeError('designated Linux cgroup only')
    subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    a.out.mkdir(parents=True,exist_ok=False)
    paths=[a.uwvm,a.wasm,a.source,a.oracle,Path(__file__),Path(step.__file__),Path(tinygo.__file__),Path(language.__file__)]
    before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    record={'passed':False,'language':a.language,'inputs_before':before,'actions':[],'positions':[],
            'host_expression_execution':False,'guest_memory_modified_by_debugger':False,
            'conditional_subset_requested':a.conditional_subset,'conditional_subset_qualified':False}
    session=None
    try:
        oracle=a.oracle.read_text();sequences=step.line_sequences(oracle)
        mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
        features=['--wasm-feature-enable-memory64','--wasm-feature-enable-table64'] if a.address_bits==64 else []
        prefix=[str(a.uwvm),'-m','debug-jit',*mode,'-Rct','0','-Rllvm-call-stack',a.diagnostic_policy,
                '-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable',*features,'--run',str(a.wasm)]
        session=step.Session(prefix,a.out/'console.log',step.code_expressions(a.wasm),sequences,a.source)
        record.update(argv=prefix,actions=session.actions,positions=session.positions)
        if a.language=='objc':
            target,_=tinygo.named_function(a.wasm,b'_i_LanguageProbe__add_')
            # A real method symbol containing whitespace, not a C export alias.
            reply=language.query(session,'break 0 -[LanguageProbe add:]')
            found=re.search(rb'breakpoint (\d+)',reply);step.require(found is not None,'real Objective-C method name breakpoint',reply)
            session.send('continue')
            for _ in range(20):
                reply=session.send('wait')
                if b'stopped: breakpoint' in reply:break
            else:raise AssertionError('method name breakpoint did not stop')
            match=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=',reply)
            step.require(match is not None and int(match[2])==target,'method breakpoint stopped a different guest function',reply)
            session.thread=int(match[1]);session.send('delete '+found[1].decode())
            marker='OBJC_METHOD_READY'
        elif a.language in ('c','cpp','rust'):
            target,_=step.metadata_cli.function(a.wasm,'numeric_probe');session.begin(target);marker='NUMERIC_READY'
        else:
            target,_=tinygo.named_function(a.wasm,b'main.probeOuter');session.begin(target);marker='VALUES'
        marker_lines=[i for i,line in enumerate(a.source.read_text().splitlines(),1) if ('/* '+marker+' */') in line or ('// '+marker) in line]
        step.require(len(marker_lines)==1,'one genuine source marker',marker_lines)
        position=language.own_seek(session,lambda p:p['function']==target and p['line']==marker_lines[0] and p['is_statement'],marker)
        checks=[('decimal32',1.25),('decimal64',-2.5),('decimal32 + decimal64',-1.25),('decimal32 * 2',2.5),
                ('static_cast<int>(decimal64)',-2),('float64(decimal32) + .5',1.75),('decimal32 as f64',1.25)] if a.language!='tinygo' else [
                ('decimal',1.25),('decimal + 2',3.25),('float32(decimal) * 2',2.5),('int32(decimal)',1),('decimal > 1.0',1)]
        if a.language=='objc':checks = [('self->value',7),('amount',4),('cookie',11)]+checks
        checks += [('true',1),('false',0),('sizeof(double)',8),('sizeof(long)',a.address_bits//8),("'A' + '\\n'",75),('1.5 < 2',1),('!-0.0',1),('0.0 && missing',0),('1.0 || missing',1)]
        if a.conditional_subset:
            checks += [('decimal32 > 0 ? decimal32 : decimal64',1.25),
                       ('decimal32 < 0 ? decimal32 : decimal64',-2.5),
                       ('1 ? decimal32 : 1 / 0',1.25),('0 ? 1 / 0 : decimal64',-2.5),
                       ('1 ? 7 : 2147483647 + 1',7),('0 ? 1 : 0 ? 2 : 3',3),
                       ('1 ? decimal32 : sizeof(decimal64)',1.25),
                       ('decimal32 > 0 ? (decimal64 < 0 ? 7 : 8) : 9',7)]
            if a.language=='objc': checks += [('amount > 0 ? self->value : 1/0',7)]
            if a.language in ('c','cpp'):
                checks += [('1 ? 7 : sizeof(packet)',7),('0 ? 7 : sizeof(packet)',28),
                           ('1 ? 7 : sizeof(null_packet)',7),('0 ? 7 : sizeof(null_packet)',a.address_bits//8),
                           ('1 ? 7 : sizeof(*null_packet)',7),('0 ? 7 : sizeof(*null_packet)',28)]
        record['values']=[]
        for expression,expected in checks:
            reply=language.command_value(session,position,expression)
            values=re.findall(rb'(?:, value=| value=| = f(?:32|64)=| = [iu]\d+=)([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)(?: \([^\r\n]*\))?\r?$',reply,re.M)
            step.require(len(values)==1 and float(values[0])==expected,'real guest numeric expression mismatch',(expression,expected,reply))
            record['values'].append({'expression':expression,'expected':expected,'reply':reply.decode()})
        for expression,width in (('1L',a.address_bits//8),('1UL',a.address_bits//8),('1LL',8)):
            reply=language.command_value(session,position,expression)
            step.require(b', bytes='+str(width).encode()+b', value=1' in reply,'numeric literal width differs from real guest ABI',(expression,reply))
        for expression in ('(int)2147483648.0','(unsigned int)-1.0','decimal32 & 1' if a.language!='tinygo' else 'decimal & 1','(int*)1','self->value = 99' if a.language=='objc' else 'decimal = 99' if a.language=='tinygo' else 'decimal32 = 99'):
            reply=language.query(session,f'print {session.thread} {position["stop_id"]} {expression}')
            step.require(b'error:' in reply and b'source-value stop=' not in reply,'invalid conversion/side effect returned a value',reply)
        if a.conditional_subset:
            for expression,expected_bytes in (('1 ? decimal32 : decimal64',8),('1 ? -1 : 0u',4),
                    ('1 ? 1L : 0u',a.address_bits//8)):
                reply=language.command_value(session,position,expression)
                step.require(b', bytes='+str(expected_bytes).encode()+b', value=' in reply,
                    'conditional result type differs from both arm metadata',(expression,reply))
            for expression in ('1 ? decimal32 : no_such_declaration','1 ? decimal32 : sizeof(no_such_declaration)',
                    '0 ? decimal32 : 1/0','1 ? decimal32 : __builtin_trap()',
                    '1 ? decimal32 : (decimal32 = 99)', '1 ? (char)1 : (char)2', '1 ? true : false'):
                reply=language.query(session,f'print-frame {session.thread} {position["stop_id"]} 0 {expression}')
                step.require(b'error:' in reply and b'source-value stop=' not in reply,
                    'conditional unsupported syntax/type or selected arithmetic returned a value',(expression,reply))
            if a.language in ('c','cpp'):
                for expression in ('sizeof(bits.flags)','1 ? 7 : sizeof(bits.flags)','0 ? 7 : sizeof(bits.flags)',
                                   '1 ? 7 : packet','1 ? 7 : null_packet'):
                    reply=language.query(session,f'print-frame {session.thread} {position["stop_id"]} 0 {expression}')
                    step.require(b'error:' in reply and b'source-value stop=' not in reply,
                        'sizeof bit-field or unsupported aggregate/pointer returned a value',(expression,reply))
            record['conditional_subset_qualified']=True
        session.send(f'step wasm {session.thread}')
        stale=language.query(session,f'print {session.thread} {position["stop_id"]} 1.5 + 2')
        step.require(b'error:' in stale and b'source-value stop=' not in stale,'literal arithmetic bypassed retired stop authentication',stale)
        if a.conditional_subset:
            stale=language.query(session,f'print-frame {session.thread} {position["stop_id"]} 0 1 ? 7 : 1/0')
            step.require(b'error:' in stale and b'source-value stop=' not in stale,'conditional bypassed retired stop authentication',stale)
        session.finish_guest();record['passed']=True
    except BaseException as e:
        record['error']=repr(e);raise
    finally:
        if session is not None:
            try:session.console.finish()
            except BaseException as e:record.update(passed=False,close_error=repr(e));raise
            finally:
                record['quit_returncode']=session.console.child.returncode
                record['managed_shutdown_complete']=b'managed shutdown complete' in session.console.transcript
        record['inputs_after']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        if record['inputs_after']!=before:record.update(passed=False,inputs_changed=True)
        (a.out/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
    if not record['passed']:raise AssertionError('language numeric qualification failed')
    print('PASS actual '+a.language+' source numeric CLI')


if __name__=='__main__':main()
