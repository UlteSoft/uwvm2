import pathlib,sys,json,struct,collections,time,os,csv
from compression.zstd import ZstdDecompressor
R=pathlib.Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=pathlib.Path(sys.argv[1]);cap=json.loads((D/'capture.json').read_text());V='/opt/intel/oneapi/vtune/2026.4/bin64/vtune'
assert not cap.get('configuration_only')
A=D/'validation-a3';A.mkdir(exist_ok=False)
files=list((D/'vtune').rglob('*.perf'))+list((D/'vtune').rglob('perf.data'));assert len(files)==1,[str(p) for p in files]
f=files[0];assert f.stat().st_size<128<<20
raw=f.read_bytes();h=struct.unpack_from('<13Q',raw);assert h[0]==0x32454c4946524550
stride,ao,asz,do,ds=h[2:7];attrs=[];ids={}
for off in range(ao,ao+asz,stride):
 typ,size=struct.unpack_from('<II',raw,off);mask=struct.unpack_from('<Q',raw,off+24)[0];flags=struct.unpack_from('<Q',raw,off+40)[0];clock=struct.unpack_from('<i',raw,off+92)[0]
 io,isz=struct.unpack_from('<QQ',raw,off+stride-16)
 a={'type':typ,'size':size,'config':struct.unpack_from('<Q',raw,off+8)[0],'period':struct.unpack_from('<Q',raw,off+16)[0],'mask':mask,'flags':flags,'clockid':clock,'ids':[],'exclude_kernel':bool(flags&(1<<5)),'callchain':bool(mask&(1<<5)),'raw_stack':bool(mask&(1<<13))}
 for k in range(io,io+isz,8):
  event_id=struct.unpack_from('<Q',raw,k)[0];ids[event_id]=a;a['ids'].append(event_id)
 attrs.append(a)
common=sum(1<<b for b in [16,0,1,2,3,6,9,7,8]);assert len(set(a['mask']&common for a in attrs))==1
mask=attrs[0]['mask'];assert all(mask&(1<<b) for b in [0,1,2,7])
assert all(a['exclude_kernel'] and not a['callchain'] and not a['raw_stack'] for a in attrs)
counts=collections.Counter();modes=collections.Counter();non_user=[];ips=collections.Counter();events=collections.Counter();lost_records=[];total=foreign=outside=target=zero=jit=0;times=[]
roi_clocks={1:'monotonic_ns',4:'raw_ns'};declared={a['clockid'] for a in attrs if a['flags']&(1<<25)}
assert len(declared)==1 and next(iter(declared)) in roi_clocks,('unknown perf clock',declared,attrs)
key=roi_clocks[next(iter(declared))];lo=cap['roi_start'][key];hi=cap['roi_end'][key]
def record(b):
 global total,foreign,outside,target,zero,jit
 typ,misc,n=struct.unpack_from('<IHH',b);counts[typ]+=1
 if typ in (2,13):
  loss=struct.unpack_from('<Q',b,16 if typ==2 else 8)[0]
  lost_records.append({'type':typ,'count':loss});return
 if typ!=9:return
 total+=1;p=8;row={}
 for bit,name,fmt in [(16,'identifier','Q'),(0,'ip','Q'),(1,'pidtid','II'),(2,'time','Q'),(3,'addr','Q'),(6,'id','Q'),(9,'stream','Q'),(7,'cpu','II'),(8,'period','Q')]:
  if mask&(1<<bit):
   v=struct.unpack_from('<'+fmt,b,p);p+=struct.calcsize('<'+fmt);row[name]=v
 pid,tid=row['pidtid'];cpu=row['cpu'][0];t=row['time'][0];ip=row['ip'][0]
 event_id=row.get('identifier',row.get('id',(0,)))[0];assert event_id in ids
 if not lo<=t<=hi:outside+=1;return
 if pid!=cap['target']['pid']:foreign+=1;return
 assert cpu==2 and tid==cap['target']['pid'],('unexpected target CPU/TID',row)
 modes[str(misc&7)]+=1
 if (misc&7)!=2:
  non_user.append({'misc':misc,'ip':hex(ip),'event_id':event_id,'type':ids[event_id]['type'],'config':hex(ids[event_id]['config'])});return
 target+=1;zero+=ip==0;ips[ip]+=1;events[str(event_id)]+=1;times.append(t)
 if any(x['lo']<=ip<x['hi'] for x in cap['rx']):jit+=1
pending=bytearray();dec=ZstdDecompressor()
def uncompressed(data):
 pending.extend(data)
 while len(pending)>=8:
  typ,misc,n=struct.unpack_from('<IHH',pending);assert n>=8
  if len(pending)<n:return
  b=bytes(pending[:n]);del pending[:n];record(b)
pos=do;end=do+ds
while pos<end:
 typ,misc,n=struct.unpack_from('<IHH',raw,pos);assert n>=8 and pos+n<=end
 b=raw[pos:pos+n];pos+=n
 if typ==81:
  out=dec.decompress(b[8:]);uncompressed(out)
  while not dec.needs_input and not dec.eof:uncompressed(dec.decompress(b''))
  if dec.eof:
   extra=dec.unused_data;dec=ZstdDecompressor()
   if extra:uncompressed(dec.decompress(extra))
 else:record(b)
j={'raw_sha':N.sha(f),'attrs':attrs,'records':dict(counts),'total_sample_records':total,'target_roi_samples':target,'foreign_roi_excluded':foreign,'outside_roi_excluded':outside,'lost_records':lost_records,'zero_ip':zero,'target_cpu':2,'target_pid':cap['target']['pid'],'clock':key,'jit_samples':jit,'top_ips':[[hex(ip),n] for ip,n in ips.most_common(100)],'top_jit_ips':[[hex(ip),n] for ip,n in sorted(((ip,n) for ip,n in ips.items() if any(r['lo']<=ip<r['hi'] for r in cap['rx'])),key=lambda x:x[1],reverse=True)[:100]],'event_counts':dict(events),'parser_sha':N.sha(__file__),'pending_bytes':len(pending),'observed_target_modes':dict(modes),'non_user_samples_excluded':non_user}
N.save(A/'raw-record-validation.json',j)
assert not pending and not any(x['count'] for x in lost_records) and target>=100 and zero==0,(len(pending),lost_records,target,zero)
for report in ['summary','hotspots']:
 label=cap['label']+'-'+report+'-report-a3'
 N.run(label,[V,'-report',report,'-r',D/'vtune','-filter','process-id='+str(cap['target']['pid']),'-format','csv','-csv-delimiter','comma','-report-output',A/(report+'.csv')],cpu=20,timeout=90)
 assert (A/(report+'.csv')).stat().st_size>0;N.persist(A/(report+'.csv'))
N.save(A/'qualified.json',{'hardware_runtime_qualified':True,'dense_JIT_instruction_metrics_qualified':jit>=20,'native_ROI_ns':cap['elapsed_ns'],'target_samples':target,'jit_samples':jit,'zero_loss':True,'raw_sha':N.sha(f),'ELF_sha':cap['elf_sha'],'WASM_sha':cap['wasm_sha'],'boot':N.BOOT,'cgroup':N.CG,'filter':'exact target PID/TID, recorded RAW clock ROI, user mode, P-core 2','source_binding':N.sha(D/'capture.json'),'non_user_samples_excluded':len(non_user),'scope':'user-mode target ROI only; kernel-mode PCs explicitly excluded despite attr exclude_kernel; independent perf stats pending'})
print('RAW_VALIDATED',cap['label'],'target',target,'jit',jit,'lost',lost_records,flush=True)
