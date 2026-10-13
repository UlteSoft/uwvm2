from pathlib import Path
import sys,json,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
ELF=R/'build/deferred-typed-array-access-inline-static/uwvm'
N.run('c23-inline-native-symbols-a1',['nm','--defined-only',ELF],cpu=20,timeout=30)
actual={l.split()[-1]:int(l.split()[0],16) for l in (R/'evidence/c23-inline-native-symbols-a1.stdout').read_text().splitlines() if len(l.split())==3}
S=json.loads((R/'source/inline-auth-native-symbols-c22-a2.json').read_text());S['ELF_sha']=N.sha(ELF)
for n,m in S['symbols'].items():
 assert m['mangled'] in actual,n
 m['C23_VMA']=actual[m['mangled']]
N.save(R/'source/inline-auth-native-symbols-c23-a1.json',S)
domain=S['symbols'][next(n for n in S['symbols'] if 'index_domain_bridge(' in n)]['mangled'];borrow=S['symbols'][next(n for n in S['symbols'] if 'typed_array_borrow_bridge<4ul, true, false>(' in n)]['mangled']
D=R/'source/inline-auth-gdb-c23-a1';D.mkdir(exist_ok=False);rows=[]
with N.lease():
 for case,slow_expected in [('head',0),('non-head',0),('deep-fallback',4),('collection-domain',0)]:
  w=R/'source/inline-array-auth-fixtures-c22-a1'/('i32-'+case+'.wasm');script=D/(case+'.gdb');script.write_text('set pagination off\nset confirm off\nset breakpoint pending off\nset $domains=0\nset $slow=0\nbreak \''+domain+'\'\ncommands\nsilent\nset $domains=$domains+1\ncontinue\nend\nbreak \''+borrow+'\'\ncommands\nsilent\nset $slow=$slow+1\ncontinue\nend\nrun\nprintf "AUTHCOUNTS %u %u\\n", $domains, $slow\n')
  N.persist(script);lab='inline-auth-gdb-c23-a1-'+case;env=dict(os.environ)
  for k in list(env):
   if k.startswith('WASM_PERF_NATIVE'):env.pop(k,None)
  N.run(lab,['gdb','-q','-nx','-batch','-x',script,'--args',ELF,'-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-exceptions','-WFE-function-references','--run',w],cpu=6,timeout=90,env=env)
  out=(R/'evidence'/(lab+'.stdout')).read_text();assert 'exited normally' in out,out[-2000:];line=next(l for l in out.splitlines() if l.startswith('AUTHCOUNTS '));count=list(map(int,line.split()[1:]));assert count==[1,slow_expected],(case,count,slow_expected)
  rows.append(dict(case=case,domains=count[0],borrow_fallback_calls=count[1],ELF_sha=S['ELF_sha'],WASM_sha=N.sha(w),receipt=lab));print('C23_GDB_PROOF',rows[-1],flush=True)
 N.save(R/'evidence/inline-auth-gdb-c23-a1.json',dict(rows=rows,scope='Real native breakpoints on precise domain/checkedborrow symbols, normal single Wasm start, no timing hooks/cache; exactly one domain call, bounded collision fallback; no-call claim limited to these exact cells.'))
