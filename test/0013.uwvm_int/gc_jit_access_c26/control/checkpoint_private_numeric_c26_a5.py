from pathlib import Path
import sys,json,os,ctypes,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
f=open(N.BASE/'control/native-c26-checkpoint-build.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
S=R/'source/uwvm2-candidate-c26-a3';BD=R/'build/deferred-private-numeric-array-lock-numeric-struct';b=json.loads((BD/'build-binding-c26-a3.json').read_text());plan=json.loads((R/'source/build-plan-private-numeric-array-lock-c26-a3.json').read_text());T=R/'source/checkpoint-c26-a4';D=R/'build/checkpoint-c26-a4';D.mkdir(exist_ok=True)
# Exact same flags as runtime and host translation units, including packed ABI and private-lock feature.
flags=plan['common']+next(iter(plan['variants'].values()))+['-I'+str(S/'src')]+['-I'+str(S/'third-parties'/x/'include') for x in ['fast_io','bizwen','boost_unordered']]
sys.path.insert(0,str(R/'source/call-ref-identity-c10'));from fixture_converter import wat_to_wasm
for p in (T/'test/0017.runtime/fixtures').glob('*.wat'):wat_to_wasm(p,p.with_suffix('.wasm'))
rows=[]
for name in ['debug_checkpoint_actual_nested_reference_runtime','debug_checkpoint_gc_two_participants_runtime']:
 src=T/'test/0017.runtime'/(name+'.cc');obj=D/(name+'.o');elf=D/name
 if name=='debug_checkpoint_actual_nested_reference_runtime':
  for suffix in ['compile','link']:
   rec=json.loads((R/'evidence'/('checkpoint-c26-a4-'+suffix+'-'+name+'.json')).read_text());assert rec['returncode']==0 and rec['guard'] is None
  assert elf.exists() and obj.exists()
 else:
  N.run('checkpoint-c26-a5-compile-'+name,[plan['compiler'],*flags,'-c',src,'-o',obj],cpu=10,timeout=600,build=True)
  N.run('checkpoint-c26-a5-link-'+name,[plan['compiler'],obj,BD/'0.o',BD/'1.o','@'+plan['link_rsp'],'-stdlib=libc++','-pthread','-ldl','-lz','-l:libzstd.so.1','-lm','-lcrypto','-lssl','-Wl,--gc-sections','-Wl,--strip-debug','-o',elf],cpu=10,timeout=120,build=True);N.persist(elf)
 for policy in ['instruction','unwind']:
  kinds=['struct','array','function','wrapper','i31','null','exception'] if name.endswith('reference_runtime') else ['two-participants']
  for kind in kinds:
   wasm=T/'test/0017.runtime/fixtures'/('debug_checkpoint_gc_two_participants.wasm' if kind=='two-participants' else 'checkpoint_actual_nested_reference_'+kind+'.wasm');lab='checkpoint-c26-a5-'+policy+'-'+kind;env=dict(os.environ)
   for k in list(env):
    if k.startswith('WASM_PERF_NATIVE') or k=='UWVM_GC_ARRAY_BUDGET_MIB':env.pop(k,None)
   cmd=[elf,wasm,policy]+([] if kind=='two-participants' else [kind]);N.run(lab,cmd,cpu=6,timeout=90,env=env);out=(R/'evidence'/(lab+'.stdout')).read_text();assert 'PASS' in out,out;rows.append(dict(kind=kind,policy=policy,ELF_sha=N.sha(elf),WASM_sha=N.sha(wasm),receipt=lab,pass_=True));print('CHECKPOINT_C26_PASS',policy,kind,flush=True)
 N.save(D/(name+'-binding.json'),dict(ELF_sha=N.sha(elf),test_source_sha=N.sha(src),runtime_build_binding_sha=N.sha(BD/'build-binding-c26-a3.json'),flags=flags,scope='Native checkpoint fallback of private measured C26a3 source/runtime; same macro ABI in all TUs. Actual source parse/translation/capture and returned GC carriers, two real participants and stale/foreign cohort rejection. Not complete GC graph rollback or three fresh repository product checkpoint qualification.'))
 obj.unlink()
N.save(R/'evidence/checkpoint-private-numeric-c26-a5.json',dict(cells=len(rows),rows=rows,scope='14 real restored native-reference continuations +2 real concurrent participants checkpoint census; originalGC/root borrowing and checkpoint fallback retained. No complete graph rollback claim.'))
