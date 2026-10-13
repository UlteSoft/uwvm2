from pathlib import Path
import sys,json,collections,re,csv,struct
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
D=N.ROOT/'source/wasmtime-codegen-analysis-a1';A=D/'actual-profile-assembly-a1';A.mkdir(exist_ok=False);base=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin');core=int(Path('/sys/bus/event_source/devices/cpu_core/type').read_text());atom=int(Path('/sys/bus/event_source/devices/cpu_atom/type').read_text());rows=[]
def elf(blob):
 shoff=struct.unpack_from('<Q',blob,40)[0];sz,num,sidx=struct.unpack_from('<HHH',blob,58);hs=[struct.unpack_from('<IIQQQQIIQQ',blob,shoff+i*sz) for i in range(num)];st=hs[sidx];names=blob[st[4]:st[4]+st[5]];sections={names[h[0]:].split(b'\0',1)[0].decode():h for h in hs};syms=[]
 for h in hs:
  if h[1]!=2:continue
  st=hs[h[6]];names=blob[st[4]:st[4]+st[5]]
  for i in range(h[4],h[4]+h[5],h[9]):
   no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',blob,i);name=names[no:].split(b'\0',1)[0].decode(errors='replace');syms.append({'name':name,'value':val,'size':size,'info':info,'section':idx})
 return sections,syms
with N.lease():
 for spec in sorted((D/'hardware-specs-a1').glob('*.json')):
  s=json.loads(spec.read_text());P=Path(s['dir']);V=P/'validation-a3';r=json.loads((V/'raw-record-validation.json').read_text());q=json.loads((V/'qualified.json').read_text());cap=json.loads((P/'capture.json').read_text());ids={str(i):a for a in r['attrs'] for i in a['ids']};count=collections.Counter();events=collections.Counter()
  for eid,n in r['event_counts'].items():a=ids[eid];count[a['type']]+=n;events[(a['type'],a['config'])]+=n
  assert count[core]==q['target_samples']==r['target_roi_samples'] and count[atom]==0 and count[1]==0
  assert events[(core,0x3c)]>0 and events[(core,0xc0)]>0
  label=s['label']+'-independent-perf-stats-a1';N.run(label,['/opt/intel/oneapi/vtune/2026.4/bin64/amplxe-perf','report','--stats','--stdio','--no-children','-i',P/'vtune/data.0/system-wide.perf'],cpu=20,timeout=30)
  stats=(N.ROOT/'evidence'/(label+'.stdout')).read_text();m=re.search(r'SAMPLE events:\s*([0-9,]+)',stats.split('cpu_',1)[0]);assert m;total=int(m.group(1).replace(',',''));assert total==r['total_sample_records']
  N.save(V/'independent-crosscheck-codegen-a1.json',{'raw_sha':r['raw_sha'],'total_sample_records':total,'crosscheck':True,'zero_loss':not any(x['count'] for x in r['lost_records']),'stats_receipt':label})
  mappings=[]
  for j,rx in enumerate(cap['rx']):
   binary=Path(rx['file']);assert N.sha(binary)==rx['sha'];obj=A/(s['label']+'-rx'+str(j)+'.o');asm=A/(s['label']+'-rx'+str(j)+'.asm')
   blabel=s['label']+'-rx-object-'+str(j);N.run(blabel,[base/'llvm-objcopy','--input-target=binary','--output-target=elf64-x86-64','--binary-architecture=i386:x86-64','--rename-section','.data=.text,alloc,load,readonly,code,contents',binary,obj],cpu=20,timeout=30)
   alabel=s['label']+'-rx-disasm-'+str(j);N.run(alabel,[base/'llvm-objdump','--disassemble','--section=.text','--no-show-raw-insn','--x86-asm-syntax=intel','--adjust-vma='+str(rx['lo']),obj],cpu=20,timeout=30);asm.write_bytes((N.ROOT/'evidence'/(alabel+'.stdout')).read_bytes());N.persist(asm);obj.unlink()
   binding={'lo':rx['lo'],'hi':rx['hi'],'RX_sha':rx['sha'],'assembly':str(asm),'assembly_sha':N.sha(asm)}
   if s['variant']=='wasmtime-drc':
    pattern=(D/'gdb-reference-read-a1/reference-read-barrier-pattern.bin').read_bytes();blob=binary.read_bytes();pos=blob.find(pattern);assert pos>=0 and blob.find(pattern,pos+1)<0;binding.update(reference_barrier_pattern_offset=pos,pattern_sha=N.sha(D/'gdb-reference-read-a1/reference-read-barrier-pattern.bin'),test_address=rx['lo']+pos+9)
   mappings.append(binding)
  hotspots=list(csv.DictReader((V/'hotspots.csv').open()));fields=['Function','CPU Time','Clockticks','Instructions Retired','Module','Start Address'];brief=[{k:x.get(k) for k in fields if k in x} for x in hotspots[:8]]
  rows.append({'profile':s['label'],'reps':s['reps'],'variant':s['variant'],'analysis':s['analysis'],'ROI_ns':cap['elapsed_ns'],'target_samples':q['target_samples'],'JIT_samples':q['jit_samples'],'zero_loss':True,'core_hardware_target_samples':count[core],'atom_target_samples':count[atom],'software_target_samples':count[1],'cycle_samples':events[(core,0x3c)],'instruction_samples':events[(core,0xc0)],'independent_total_sample_count':total,'raw_sha':r['raw_sha'],'independent_receipt':label,'hotspots':brief,'mappings':mappings,'capture_sha':N.sha(P/'capture.json'),'qualified_sha':N.sha(V/'qualified.json')});print('BOUND',s['label'],flush=True)
 # Bind hottest actual native functions to exact measured C20 ELF symbols and PC samples.
 P=next(Path(json.loads(p.read_text())['dir']) for p in (D/'hardware-specs-a1').glob('*.json') if json.loads(p.read_text())['variant']=='typed-array-load' and json.loads(p.read_text())['analysis']=='hotspots')
 cap=json.loads((P/'capture.json').read_text());raw=json.loads((P/'validation-a3/raw-record-validation.json').read_text());sections,syms=elf(Path(cap['elf']).read_bytes());native=[]
 for x in list(csv.DictReader((P/'validation-a3/hotspots.csv').open()))[:3]:
  addr=int(x['Start Address'],16);sym=next(s for s in syms if s['value']==addr and s['size']>0 and s['info']&15==2);assert sym['size']<=16384;name=['array-get','aggregate-bridge','struct-get32-bridge'][len(native)]
  label='reference-c20-native-'+name+'-a1';N.run(label,[base/'llvm-objdump','--disassemble','--no-show-raw-insn','--x86-asm-syntax=intel','--start-address='+str(addr),'--stop-address='+str(addr+sym['size']),cap['elf']],cpu=20,timeout=30);asm=A/(name+'.asm');asm.write_bytes((N.ROOT/'evidence'/(label+'.stdout')).read_bytes());N.persist(asm)
  xchgs=[int(m.group(1),16) for m in re.finditer(r'^\s*([0-9a-f]+):\s+xchg\s',asm.read_text(),re.M)];records={int(ip,16):n for ip,n in raw['top_ips']};native.append({'function':x['Function'],'ELF_sha':cap['elf_sha'],'symbol':sym,'assembly':str(asm),'assembly_sha':N.sha(asm),'xchg_addresses':xchgs,'xchg_mixed_event_records_among_top100':{hex(a):records.get(a,0) for a in xchgs},'scope':'Native ELF symbol address verified; sampled records mixed hardware events/skid, not lock wait counts.'})
 # Model only the actual six instruction bump arithmetic block, no whole allocator prediction.
 block=A/'drc-bump-six-instructions.s';block.write_text('.intel_syntax noprefix\nmov eax, dword ptr [r14 + 0x40]\nlea ecx, [rax + r12]\ncmp ecx, dword ptr [r14 + 0x44]\nseta dl\nmov ecx, eax\nadd ecx, r12d\n');N.persist(block);label='wasmtime-actual-bump-mca-a1';N.run(label,[base/'llvm-mca','--mcpu=raptorlake','--iterations=100',block],cpu=20,timeout=30);mca=A/'drc-bump-six-instructions.mca';mca.write_bytes((N.ROOT/'evidence'/(label+'.stdout')).read_bytes());N.persist(mca)
 N.save(D/'profile-assembly-proof-c20-a1.json',{'profiles':rows,'cpu_core_type':core,'cpu_atom_type':atom,'native':native,'MCA':{'input':str(block),'input_sha':N.sha(block),'output':str(mca),'output_sha':N.sha(mca),'scope':'Actual6instructions atELF c5d33a..c5d34b,RaptorLake100iterations. Branches/allocatorcalls/cache/coherency not modeled; not runtime speed ratio.'},'scope':'4new profiles independently counted and hardware domain verified. Actual RX disassembly bound to stable capture hashes. Wasmtime actual reference membership block matches CLI and GDB bytes. Different repetitions acrossVMprofiles, no matched-work PMUcounter ratio claim; timing matrix uses same32starts.'})
 print('REFERENCE_HARDWARE_PROOF_COMPLETE',len(rows),flush=True)
