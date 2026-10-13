import sys,pathlib,json,struct
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
D=N.ROOT/'source/wasmtime-codegen-analysis-a1';j=json.loads((D/'profile-assembly-proof-c20-a1.json').read_text());p=next(x for x in j['profiles'] if x['variant']=='typed-array-load' and x['analysis']=='hotspots');P=N.ROOT/'profiles'/p['profile'];cap=json.loads((P/'capture.json').read_text());elf=pathlib.Path(cap['elf']);assert N.sha(elf)==cap['elf_sha'];ed=elf.read_bytes();off=struct.unpack_from('<Q',ed,32)[0];sz,num=struct.unpack_from('<HH',ed,54);ph=[struct.unpack_from('<IIQQQQQQ',ed,off+i*sz) for i in range(num)];load=[x for x in ph if x[0]==1 and x[2]==0];assert len(load)==1
maps=(P/'before-maps.txt').read_text().splitlines();zero=[int(l.split()[0].split('-')[0],16) for l in maps if len(l.split())>=6 and l.split()[-1]==str(elf) and int(l.split()[2],16)==0];assert len(zero)==1;bias=zero[0]-load[0][3];raw=json.loads((P/'validation-a3/raw-record-validation.json').read_text());pcs={int(ip,16):n for ip,n in raw['top_ips']}
for x in j['native']:
 a=x['symbol']['value']+bias;b=a+x['symbol']['size'];assert any(int(l.split()[0].split('-')[0],16)<=a and b<=int(l.split()[0].split('-')[1],16) and 'x' in l.split()[1] and l.split()[-1]==str(elf) for l in maps)
 x['load_bias']=bias;x['runtime_start']=a;x['runtime_end']=b;x['xchg_runtime_addresses']=[a+bias for a in x['xchg_addresses']];x['xchg_mixed_event_records_among_top100']={hex(a):pcs.get(a,0) for a in x['xchg_runtime_addresses']};x['top100_mixed_event_records_in_symbol']=sum(n for pc,n in pcs.items() if a<=pc<b);x['scope']='PIE ELF symbol/file virtual address mapped through actual LOAD segment and before-maps load bias; ELF SHA unchanged. Top100 mixed event records may skid; not waiting counts or full event histogram.'
 assert x['top100_mixed_event_records_in_symbol']>0
j['supersedes']='profile-assembly-proof-c20-a1 native PC lookup fields used ELF virtual addresses without PIE load bias; replaced here. Other matrix/GDB/PMU proofs unchanged.'
p=D/'profile-assembly-proof-c20-a2.json'
if p.exists():assert json.loads(p.read_text())==j
else:N.save(p,j)
print('ACTUAL_PIE_NATIVE_BINDING_REPRODUCED',bias,len(j['native']))
