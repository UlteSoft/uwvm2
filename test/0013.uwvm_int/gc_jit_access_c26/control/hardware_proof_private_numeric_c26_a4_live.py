from pathlib import Path
import sys,json,collections,re,csv,time,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/private-numeric-codegen-c26-a3';A=D/'assembly';A.mkdir(exist_ok=False,parents=True);base=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin');core=int(Path('/sys/bus/event_source/devices/cpu_core/type').read_text());atom=int(Path('/sys/bus/event_source/devices/cpu_atom/type').read_text());rows=[]
class ReadonlyProofLease:
 def __enter__(self):
  self.f=open(N.BASE/'control/native-c26-hardware-proof.lock','a+');fcntl.flock(self.f,fcntl.LOCK_EX|fcntl.LOCK_NB);N.check();return self
 def __exit__(self,*a):fcntl.flock(self.f,fcntl.LOCK_UN);self.f.close()
def qualified_entries():
 seen=set();deadline=time.monotonic()+2100
 while len(seen)<22:
  assert time.monotonic()<deadline;N.check()
  for p in sorted((R/'source/hardware-specs-private-numeric-c26-a3').glob('*.json')):
   if str(p) in seen:continue
   sp=json.loads(p.read_text());q=Path(sp['dir'])/'validation-a3/qualified.json'
   if not q.exists():continue
   seen.add(str(p));yield dict(spec=str(p),spec_sha=N.sha(p),qualified=str(q),qualified_sha=N.sha(q))
  if len(seen)<22:time.sleep(1)
with ReadonlyProofLease():
 for entry in qualified_entries():
  p=Path(entry['spec']);assert N.sha(p)==entry['spec_sha']
  s=json.loads(p.read_text());P=Path(s['dir']);V=P/'validation-a3';r=json.loads((V/'raw-record-validation.json').read_text());q=json.loads((V/'qualified.json').read_text());cap=json.loads((P/'capture.json').read_text());ids={str(i):a for a in r['attrs'] for i in a['ids']};counts=collections.Counter();events=collections.Counter()
  for eid,n in r['event_counts'].items():a=ids[eid];counts[a['type']]+=n;events[(a['type'],a['config'])]+=n
  assert counts[core]==q['target_samples']==r['target_roi_samples'] and counts[atom]==0 and counts[1]==0;assert events[(core,0x3c)]>0 and events[(core,0xc0)]>0
  label=s['label']+'-independent-stats-c26';N.run(label,['/opt/intel/oneapi/vtune/2026.4/bin64/amplxe-perf','report','--stats','--stdio','--no-children','-i',P/'vtune/data.0/system-wide.perf'],cpu=20,timeout=30)
  stats=(R/'evidence'/(label+'.stdout')).read_text();m=re.search(r'SAMPLE events:\s*([0-9,]+)',stats.split('cpu_',1)[0]);assert m;total=int(m[1].replace(',',''));assert total==r['total_sample_records'] and not any(x['count'] for x in r['lost_records']);N.save(V/'independent-domain-proof-c26.json',dict(total=total,core_target=counts[core],atom_target=counts[atom],software_target=counts[1],raw_sha=r['raw_sha'],receipt=label))
  asmfiles=[]
  for i,rx in enumerate(cap['rx']):
   binary=Path(rx['file']);assert N.sha(binary)==rx['sha'];obj=A/(s['label']+'-'+str(i)+'.o');asm=obj.with_suffix('.asm');lab=s['label']+'-obj-'+str(i);N.run(lab,[base/'llvm-objcopy','--input-target=binary','--output-target=elf64-x86-64','--binary-architecture=i386:x86-64','--rename-section','.data=.text,alloc,load,readonly,code,contents',binary,obj],cpu=20,timeout=30);lab=s['label']+'-asm-'+str(i);N.run(lab,[base/'llvm-objdump','--disassemble','--section=.text','--no-show-raw-insn','--x86-asm-syntax=intel','--adjust-vma='+str(rx['lo']),obj],cpu=20,timeout=30);asm.write_bytes((R/'evidence'/(lab+'.stdout')).read_bytes());N.persist(asm);obj.unlink();asmfiles.append(dict(file=str(asm),sha=N.sha(asm),RX_sha=rx['sha'],lo=rx['lo'],hi=rx['hi']))
  hs=list(csv.DictReader((V/'hotspots.csv').open()));rows.append(dict(case=s['case'],variant=s['variant'],analysis=s['analysis'],profile=s['label'],reps=s['reps'],core_target_samples=counts[core],atom_target_samples=counts[atom],software_target_samples=counts[1],zero_loss=True,JIT_samples=q['jit_samples'],total_sample_records=total,cycle_samples=events[(core,0x3c)],instruction_samples=events[(core,0xc0)],hotspots=hs[:8],object_flag_XCHG=sum(sum(1 for line in Path(a['file']).read_text().lower().splitlines() if 'xchg' in line and 'byte ptr' in line and ('+ 32]' in line or '+ 0x20]' in line)) for a in asmfiles),asm=asmfiles,capture_sha=N.sha(P/'capture.json'),qualified_sha=N.sha(V/'qualified.json')));print('C21_HARDWARE_BOUND',s['label'],flush=True)
 N.save(D/'hardware-assembly-proof-c26-a3.json',dict(rows=rows,core_type=core,atom_type=atom,scope='22 actual compiled C23/C25/C26a3/WT postJIT hardware Hotspots and detailed uarch captures, zeroPERFloss independent stats, PMUtypeevent domainjoin provescore2user hardware target counts; same actual RX stablebeforeafter. Separately calibratedreps, not raw total counters ratio. Exact assembly files bound to actual captured executable bytes.'))
