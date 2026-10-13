from pathlib import Path
import sys,json,struct
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/typed-array-store-codegen-c21';proof=json.loads((D/'hardware-assembly-proof-c21-a1.json').read_text());rows=[];base=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin')
def syms(blob):
 off=struct.unpack_from('<Q',blob,40)[0];sz,num=struct.unpack_from('<HH',blob,58);hs=[struct.unpack_from('<IIQQQQIIQQ',blob,off+i*sz) for i in range(num)];out=[]
 for h in hs:
  if h[1]!=2:continue
  st=hs[h[6]];strings=blob[st[4]:st[4]+st[5]]
  for i in range(h[4],h[4]+h[5],h[9]):
   no,info,other,idx,val,size=struct.unpack_from('<IBBHQQ',blob,i)
   if info&15==2 and size:out.append(dict(name=strings[no:].split(b'\0',1)[0].decode(errors='replace'),value=val,size=size))
 return out
with N.lease():
 for v,pattern in [('typed-array-load','gc_aggregate_fixed_bridge'),('typed-array-access','typed_array_borrow_bridge')]:
  row=next(x for x in proof['rows'] if x['variant']==v and x['analysis']=='hotspots');P=R/'profiles'/row['profile'];cap=json.loads((P/'capture.json').read_text());elf=Path(cap['elf']);blob=elf.read_bytes();assert N.sha(elf)==cap['elf_sha'];hot=next(x for x in row['hotspots'] if pattern in x['Function']);rva=int(hot['Start Address'],16);sym=next(x for x in syms(blob) if x['value']==rva);phoff=struct.unpack_from('<Q',blob,32)[0];sz,n=struct.unpack_from('<HH',blob,54);phs=[struct.unpack_from('<IIQQQQQQ',blob,phoff+i*sz) for i in range(n)];maps=[l.split() for l in (P/'before-maps.txt').read_text().splitlines() if l.split()[-1]==str(elf) and 'x' in l.split()[1]];assert len(maps)==1;m=maps[0];lo=int(m[0].split('-')[0],16);offset=int(m[2],16);ph=next(x for x in phs if x[0]==1 and x[1]&1 and x[2]//4096*4096==offset);bias=lo-ph[3]//4096*4096;actual=bias+rva;raw=json.loads((P/'validation-a3/raw-record-validation.json').read_text());within=sum(n for a,n in raw['top_ips'] if actual<=int(a,16)<actual+sym['size']);lab='typed-store-native-bridge-c21-a2-'+v;N.run(lab,[base/'llvm-objdump','--disassemble','--no-show-raw-insn','--x86-asm-syntax=intel','--start-address='+str(rva),'--stop-address='+str(rva+sym['size']),elf],cpu=20,timeout=30);asm=D/(v+'-actual-native-bridge-a2.asm');asm.write_bytes((R/'evidence'/(lab+'.stdout')).read_bytes());N.persist(asm);hits=[dict(RX_sha=rx['sha'],lo=rx['lo'],pointer_occurrences=Path(rx['file']).read_bytes().count(struct.pack('<Q',actual))) for rx in cap['rx']];assert any(x['pointer_occurrences'] for x in hits);rows.append(dict(variant=v,symbol=sym,ELF_sha=cap['elf_sha'],load_bias=hex(bias),actual_address=hex(actual),top100_mixed_hardware_records_in_symbol=within,JIT_pointer_binding=hits,asm_sha=N.sha(asm),asm=str(asm)));print('BOUND_NATIVE_CALL',v,sym['size'],hex(actual),within,flush=True)
 N.save(D/'native-bridge-binding-c21-a2.json',dict(rows=rows,scope='Exact measured native ELF symbol+PT_LOAD/procmap PIEbias+actualJIT embedded pointer match. Old JIT calls aggregate bridge which calls array_set; new calls typed borrow then performsstore inJIT. Rawtop100IPs mixed eventrecords/skid, not callcounts or precise cycles.'))
