from pathlib import Path
import sys,re,json
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/private-numeric-codegen-c26-a3/mca-actual-readwrite-a1';D.mkdir(exist_ok=False);rows=[]
for variant in ['typed-array-access-inline-static32','typed-array-access-known-length','private-numeric-array-lock-numeric-struct','wasmtime-drc']:
 p=R/'source/private-numeric-codegen-c26-a3/assembly'/('private-numeric-c26-a3-i32-write-read-'+variant+'-hotspots-0.asm')
 ins=[]
 for l in p.read_text().splitlines():
  m=re.match(r'([0-9a-f]+):\s+(.*)',l)
  if m:ins.append((int(m[1],16),m[2].split(' <')[0].split(' #')[0].strip()))
 idx={a:i for i,(a,t) in enumerate(ins)};base=ins[0][0];back=[]
 for a,t in ins:
  m=re.match(r'(j\w+)\s+0x([0-9a-f]+)',t)
  if m and 80<a-int(m[2],16):back.append((a,int(m[2],16)))
 end,start=min(back);pc=start;selected=[];branches=[]
 for step in range(500):
  a,t=ins[idx[pc]];assert not t.startswith(('call','ud2','ret')),('cold path selected',variant,hex(a),t)
  selected.append(dict(address=hex(a),offset=hex(a-base),original=t));m=re.match(r'(j\w+)\s+0x([0-9a-f]+)',t)
  if m:
   op,target=m[1],int(m[2],16);prev=selected[-2]['original'] if len(selected)>1 else ''
   take=op=='jmp' or (op=='je' and ('cmp\tqword ptr [rax + 0x8]' in prev)) or (op=='jne' and prev.startswith('cmp\tbyte ptr [rsp')) or (a==end)
   branches.append(dict(ip=hex(a),taken=take,reason='head token equality, private valid fact, uncontended lock, normal loop back edge; others checked-success fallthrough'))
   selected[-1]['mca']=op+' .Llinear_path_end'
   if take and target<=start:break
   pc=target if take else ins[idx[pc]+1][0]
  else:
   # llvm-mca Intel parser supports segment nop syntax when omitted from inert padding.
   selected[-1]['mca']=t;pc=ins[idx[pc]+1][0]
 else:raise AssertionError('unterminated selected trace')
 assert len(selected)>30 and sum('dword ptr' in x['original'] and x['original'].startswith(('mov\t','add\t')) for x in selected)>=2
 emitted=[x for x in selected if not x['original'].startswith('nop')];asm=D/(variant+'.s');asm.write_text('.intel_syntax noprefix\n.text\n'+''.join(x['mca']+'\n' for x in emitted)+'.Llinear_path_end:\n');N.persist(asm)
 lab='actual-readwrite-mca-c26-a1-'+variant;N.run(lab,['/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llvm-mca','-mtriple=x86_64-linux-gnu','-mcpu=alderlake','-iterations=100','-all-stats',asm],cpu=20,timeout=60)
 out=(R/'evidence'/(lab+'.stdout')).read_text();metrics={k:v for k,v in re.findall(r'^(Iterations|Instructions|Total Cycles|Total uOps|Dispatch Width|uOps Per Cycle|IPC|Block RThroughput):\s+([^\n]+)',out,re.M)}
 rows.append(dict(variant=variant,assembly_source=str(p),assembly_sha=N.sha(p),linear_trace_sha=N.sha(asm),metrics=metrics,selected=selected,branches=branches,emitted_count=len(emitted),receipt=lab));print('MCA_ACTUAL_PASS',variant,metrics,flush=True)
N.save(D/'result.json',dict(rows=rows,scope='Selected actual warmed RX instructions on head-bucket checked-success array read/write path, preserving instruction sequence and dependency registers. Branch operands rewritten only to a common inert label for linear MCA scheduling. Inert NOP padding excluded; failure/collision/trap/allocator paths excluded. Alder Lake P-core scheduling model approximates Raptor Cove; MCA does not model cache misses, lock coherence or branch flow, so output is NOT native performance or a replacement for fixed user PMU cycles. x86 only, no cross-architecture performance claim.',documentation='https://llvm.org/docs/CommandGuide/llvm-mca.html'))
