from pathlib import Path
import sys,json,re
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
N.check();D=R/'source/typed-array-inline-auth-codegen-c23-a3';M=D/'static32-mca-a2';M.mkdir(exist_ok=False)
proof=json.loads((D/'hardware-assembly-proof-write-c23-a3.json').read_text())
ranges={
 'typed-array-access-inline':[(0x133,0x15e),(0x1d0,0x1e6),(0x211,0x237),(0x240,0x26d),(0x11f,0x133)],
 'typed-array-access-inline-static32':[(0x124,0x170),(0x1f0,0x217),(0x220,0x235),(0x110,0x124)],
 'wasmtime-drc':[(0x9f,0x125)]}
rows=[]
for row in proof['rows']:
 if row['analysis']!='hotspots':continue
 v=row['variant'];a=row['asm'][0];p=Path(a['file']);assert N.sha(p)==a['sha'];lo=a['lo'];ins=[]
 parsed=[]
 for l in p.read_text().splitlines():
  m=re.match(r'([0-9a-f]+):\s+(.*)',l)
  if m:parsed.append((int(m[1],16),re.sub(r'\s*<[^>]+>','',m[2]).strip()))
 for first,end in ranges[v]:
  for addr,inst in parsed:
   if first<=addr-lo<end and not inst.startswith('nop'):
    ins.append(dict(address=hex(addr),offset=hex(addr-lo),actual=inst))
 assert ins and all(not x['actual'].startswith('call') for x in ins)
 source='.intel_syntax noprefix\n.text\n'
 for x in ins:
  s=x['actual']
  if re.match(r'j\w+\s+0x[0-9a-f]+$',s):s=s.split()[0]+' .Lselected_path_end'
  source+=s+'\n'
 source+='.Lselected_path_end:\n'
 f=M/(v+'.s');f.write_text(source);N.persist(f)
 rec=N.run('actual-static32-mca-c23-a2-'+v,[str(Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llvm-mca')),'-mtriple=x86_64-pc-linux-gnu','-mcpu=alderlake','-iterations=100','-instruction-info','-resource-pressure','-dispatch-stats',str(f)],cpu=20,timeout=60)
 assert rec['exit']==0 and rec['guard'] is None,rec
 text=(R/'evidence'/('actual-static32-mca-c23-a2-'+v+'.stdout')).read_text()
 metrics={k:re.search(r'^'+re.escape(k)+r':\s*(.+)$',text,re.M).group(1) for k in ['Iterations','Instructions','Total Cycles','Total uOps','Dispatch Width','uOps Per Cycle','IPC','Block RThroughput']}
 rows.append(dict(variant=v,profile=row['profile'],actual_asm=a,selected_ranges_offsets=ranges[v],selected_instructions=ins,input=str(f),input_sha=N.sha(f),receipt='actual-static32-mca-c23-a2-'+v,metrics=metrics))
 print(v,metrics,flush=True)
N.save(M/'result.json',dict(rows=rows,proof_sha=N.sha(D/'hardware-assembly-proof-write-c23-a3.json'),CPU_model='alderlake P-core scheduling approximation for Raptor Cove',scope='Selected head-success loop trace. Branch targets are rewritten to terminal label without deleting branch instructions; LLVM-MCA executes the selected linear stream. No allocation, bridge, collision-chain, trap, contention, cache-coherence or whole-VM prediction. All native measurements use fixed counters; MCA is explanatory only.'))
