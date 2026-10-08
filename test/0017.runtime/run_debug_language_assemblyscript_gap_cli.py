from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys

ap=argparse.ArgumentParser()
for key in ('uwvm','wasm','source','map','out','source-root'):
    ap.add_argument('--'+key,type=Path,required=True)
ap.add_argument('--ros',action='store_true')
ap.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
a=ap.parse_args()
assert sys.platform=='linux'
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
sys.path.insert(0,str(a.source_root/'test/0017.runtime'))
import run_debug_source_inline_metadata_cli as metadata
import run_debug_language_experience_cli as language
a.out.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
paths=[a.uwvm,a.wasm,a.source,a.map,Path(__file__),Path(metadata.__file__),Path(language.__file__)]
before={str(p):sha(p) for p in paths}
row={'passed':False,'language':'assemblyscript','language_level_passed':False,'typed_DWARF_present':False,'source_map_supported':False,'qualification':'real Wasm control/name display and explicit source/typed-value refusal; not language parity','inputs_before':before,'actions':[]}
c=None
def send(command):
    reply=language.clean(c.send(command));row['actions'].append({'command':command,'reply':reply.decode()})
    assert len(c.transcript)<8*1024*1024
    return reply
try:
    customs=[metadata.named_custom(p).decode() for kind,p in metadata.sections(a.wasm) if kind==0]
    assert 'name' in customs and 'sourceMappingURL' in customs
    assert not any(x.startswith('.debug_') for x in customs)
    mapping=json.loads(a.map.read_text());assert mapping['version']==3 and mapping['mappings'] and mapping['sources']
    assert any(Path(x).name==a.source.name for x in mapping['sources'])
    row['custom_sections']=customs;row['source_map_sources']=mapping['sources']
    mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
    argv=[str(a.uwvm),'-m','debug-jit',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm)]
    row['argv']=argv;c=metadata.Console(argv,a.out/'console.log')
    target,_=metadata.function(a.wasm,'probe_outer')
    assert b'prepared; no Wasm instruction executed' in send('status')
    reply=send(f'break 0 {target} 0');bid=re.search(rb'breakpoint (\d+)',reply);assert bid is not None
    send('continue')
    for _ in range(20):
        stopped=send('wait')
        if b'stopped: breakpoint' in stopped:break
    else:raise AssertionError('real AssemblyScript Wasm breakpoint did not stop')
    match=re.search(rb'thread (\d+) module=0 function='+str(target).encode()+rb' byte-offset=(\d+) generation=(\d+)',stopped)
    assert match is not None and int(match[3])>0,stopped
    thread=int(match[1]);send('delete '+bid[1].decode())
    bt=send(f'bt {thread}');assert b'probe_outer' in bt and b'\n  source ' not in bt,bt
    stop=re.search(rb'^stop-id (\d+)\r?$',bt,re.M);assert stop is not None
    commands=[f'break-source 0 {a.source}:18',f'step source {thread} into',f'print {thread} {stop[1].decode()} shadow',f'ptype {thread} {stop[1].decode()} packet']
    row['unavailable_language_features']=[]
    table=send('info breakpoints')
    for command in commands:
        reply=send(command)
        assert b'error:' in reply and b'source-value stop=' not in reply and b'\n  source ' not in reply,(command,reply)
        assert table==send('info breakpoints')
        row['unavailable_language_features'].append({'command':command,'reply':reply.decode(),'language_PASS':False})
    stepped=send(f'step wasm {thread}');assert b'stopped: selected participant step' in stepped and b'\n  source ' not in stepped
    later=send(f'bt {thread}');later_stop=re.search(rb'^stop-id (\d+)\r?$',later,re.M)
    assert later_stop is not None and int(later_stop[1])>int(stop[1])
    send('continue')
    for _ in range(20):
        result=send('wait')
        if b'guest exited:' in result:
            assert b'guest exited: 0' in result,result
            break
    else:raise AssertionError('AssemblyScript genuine workload exit missing')
    row['passed']=True
except BaseException as e:
    row['error']=repr(e)
    raise
finally:
    if c is not None:
        try:c.finish()
        except BaseException as e:row.update(passed=False,close_error=repr(e));raise
        finally:
            row['quit_returncode']=c.child.returncode
            row['managed_shutdown_complete']=b'managed shutdown complete' in c.transcript
            if row['quit_returncode']!=0 or not row['managed_shutdown_complete']:row['passed']=False
    row['inputs_after']={str(p):sha(p) for p in paths}
    if row['inputs_after']!=before:row.update(passed=False,inputs_changed=True)
    (a.out/'summary.json').write_text(json.dumps(row,indent=2)+'\n')
assert row['passed']
print('QUALIFIED AssemblyScript real Wasm control/name stack; language metadata remains UNAVAILABLE')
