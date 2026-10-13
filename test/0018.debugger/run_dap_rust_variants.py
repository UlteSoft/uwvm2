#!/usr/bin/env python3
"""Actual compiler-produced Rust variants through Linux guest stops and DAP stdio.

Only run under the cgroup supervisor. Display metadata grants no new selectors,
memory references or mutations. Original VM wait and actual EOF are required.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,sys,tarfile,time
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
sys.dont_write_bytecode=True

def session(a,folder,root,profile):
 folder.mkdir(mode=0o700)
 dap=base.load(root/'tools/debug/dap_adapter.py','actual_variant_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 features=['--wasm-feature-enable-memory64','--wasm-feature-enable-table64'] if profile['address_width']==64 else []
 vm=['-m','run',*mode,*features,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable','--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 record=dict(passed=False,policy=a.policy,profile=profile['profile'],trees=[],array_selectors=[],array_rejections=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   first=initial.request('pause');live.require('error:' not in first and 'guest exited:' not in first,'actual first queued pause',first)
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700)
  client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source'])
  line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'OBJECT_VARIANT_DAP_STOP' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'actual producer source breakpoint',bp)
  client.request('continue',{'threadId':live.actual_location(client)['thread']})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'guest exited before source stop',stop)
  else:raise AssertionError('real Rust variant stop timeout')
  position=live.actual_location(client);stack=client.request('stackTrace',{'threadId':position['thread']})['body']
  frames=[f for f in stack['stackFrames'] if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_variant_probe' in f['name']]
  live.require(len(frames)==1,'actual Rust variant source frame',stack)
  frame=frames[0]
  scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
  scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables']
  record.update(position=position,frame=frame,variables=variables,breakpoint=bp[0])
  def readonly(row):
   live.require(row['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in row and 'evaluateName' not in row,'readonly copied variant display',row)
  def members(ref):
   rows=client.request('variables',{'variablesReference':ref})['body']['variables']
   for row in rows:readonly(row)
   return rows
  def tree(ref,entry):
   flat=[]
   def visit(reference,path,depth):
    live.require(depth<=32 and len(flat)<1024,'bounded original variant tree')
    rows=members(reference)
    for kwargs,want in (({'filter':'named'},[r for r in rows if not re.fullmatch(r'\[(0|[1-9][0-9]*)\]',r['name'])]),({'filter':'indexed'},[r for r in rows if re.fullmatch(r'\[(0|[1-9][0-9]*)\]',r['name'])]),({'start':1,'count':2},rows[1:3])):
     live.require(client.request('variables',dict(variablesReference=reference,**kwargs))['body']['variables']==want,'real variant paging')
    for row in rows:
     node={k:v for k,v in row.items() if k!='variablesReference'};node['path']=path+[row['name']];flat.append(node)
     if row['variablesReference']:visit(row['variablesReference'],node['path'],depth+1)
   visit(ref,[],0)
   live.require(any(r['path']==['seed'] and r['value']=='5' for r in flat),'real Rust seed',flat)
   live.require(any(r['path']==['next'] and re.fullmatch(r'guest:0x[0-9a-f]+',r['value']) and r['value']!='guest:0x0' for r in flat),'real Rust guest selfpointer',flat)
   by={tuple(r['path']):r for r in flat}
   live.require(len(by)==len(flat)==41,'complete original Rust variant layout',flat)
   live.require([r['path'][0] for r in flat if len(r['path'])==1]==['seed','negative','positive','empty','some','none','grid','next'],'real original Rust field order')
   for field,case,tag,default in (('negative','Negative','-3',False),('positive','Positive','7',False),('empty','Empty','11',False),('some','Some','13',True),('none','None','0',False)):
    prefix=(field,'<variant-part>')
    live.require(by[prefix]['value']=='variant-part' and by[prefix+('object',)]['value']==tag,'actual copied Rust discriminant',(field,flat))
    active=[r for r in flat if tuple(r['path'][:-1])==prefix and r['value'].startswith('active variant')]
    live.require(len(active)==1 and active[0]['name']==case and active[0]['value']==('active variant (default)' if default else 'active variant'),'only VM-selected branch',(field,active))
   for path,value in ((('negative','<variant-part>','Negative','Negative','signed'),'-3'),(('positive','<variant-part>','Positive','Positive','unsigned'),'7'),(('some','<variant-part>','Some','Some','__0','__0','__0'),'13')):
    live.require(by[path]['value']==value and by[path]['type'] in ('i32','u32'),'original Rust selected payload',(path,by[path]))
   for field,case in (('empty','Empty'),('none','None')):
    prefix=(field,'<variant-part>',case,case)
    live.require(prefix in by and not any(tuple(r['path'][:len(prefix)])==prefix and len(r['path'])>len(prefix) for r in flat),'empty variant has no invented payload',field)
   for i in range(2):
    for j in range(3):live.require(by[('grid',f'[{i}]',f'[{j}]')]['value']==str(i*3+j+1),'real Rust 2D array beside variants')
   record['trees'].append(dict(entry=entry,rows=len(flat),members=flat))
   return flat
  source_object=next(v for v in variables if v['name']=='object')
  if a.expect_opaque:
   live.require(source_object['variablesReference']==0,'before Rust variant Source object remains opaque',source_object)
   source_tree=None
  else:
   live.require(source_object['variablesReference']>0,'real Source Rust variant object expandable',source_object)
   source_tree=tree(source_object['variablesReference'],'Source variables')
  for context in ('watch','hover','variables'):
   aggregate=client.request('evaluate',{'expression':'object','context':context,'frameId':frame['id']})['body'];readonly(aggregate)
   if a.expect_opaque:
    live.require(aggregate['variablesReference']==0 and ', variant-part' in aggregate['result'] and ', active-variant' in aggregate['result'],'actual before variant packet loses complete tree',aggregate)
    record.setdefault('opaque',[]).append(aggregate)
   else:
    live.require(aggregate['variablesReference']>0,'actual aggregate after projection',aggregate)
    live.require(tree(aggregate['variablesReference'],context)==source_tree,'same actual copied tree at all DAP entries')
  if not a.expect_opaque:
   for context in ('watch','hover','variables'):
    for i in range(2):
     for j in range(3):
      expression=f'object.grid[{i}][{j}]'
      row=client.request('evaluate',{'expression':expression,'context':context,'frameId':frame['id']})['body'];readonly(row)
      live.require(row['result']==str(i*3+j+1) and row['variablesReference']==0,'actual multidimensional array selector',(expression,row))
      record['array_selectors'].append(dict(context=context,expression=expression,expected=i*3+j+1,response=row))
   for context in ('watch','hover','variables'):
    for expression in ('object.grid[2][0]','object.grid[0][3]','object.grid[-1][0]','object.grid[0][-1]'):
     fresh=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
     selected=next(f for f in fresh if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_variant_probe' in f['name'])
     denied=client.request('evaluate',{'expression':expression,'context':context,'frameId':selected['id']},success=False)
     record['array_rejections'].append(dict(context=context,expression=expression,response=denied))
   # Failed reads retire adapter references; obtain new live references before
   # the real step so stale-reference checks cannot pass due to an earlier error.
   fresh=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   frame=next(f for f in fresh if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_variant_probe' in f['name'])
   scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
   scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
   variables=client.request('variables',{'variablesReference':scope})['body']['variables']
   source_object=next(v for v in variables if v['name']=='object')
   live.require(source_object['variablesReference']>0,'fresh live object before actual step',source_object)
   # Obtain live descendant references after the deliberately failed reads.
   fields=members(source_object['variablesReference'])
   negative=next(r for r in fields if r['name']=='negative')
   group=members(negative['variablesReference'])[0]
   branch=next(r for r in members(group['variablesReference']) if r['name']=='Negative')
   live.require(group['variablesReference']>0 and branch['variablesReference']>0,'live copied variant references before actual step')
  # Queue a real instruction step and stale reads together. No test-side stop
  # identifier is fabricated, and no guest self-pointer is chased.
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  queries=[('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),
           ('evaluate',{'expression':'object.seed','context':'watch','frameId':frame['id']}),
           ('variables',{'variablesReference':scope}),
           ('scopes',{'frameId':frame['id']})]
  if not a.expect_opaque:queries.extend([('variables',{'variablesReference':source_object['variablesReference']}),('variables',{'variablesReference':group['variablesReference']}),('variables',{'variablesReference':branch['variablesReference']})])
  replies=[client.response(n) for n in client.queue(queries)]
  live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real step retires variant/group/object/frame references',replies)
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'real new stop after step')
  record['queued_retirement']=replies
  client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:
    live.require(b'guest exited: 0' in exited,'guest enum/array/selfpointer oracle 43',exited);break
  else:raise AssertionError('real Rust variant guest did not finish')
  server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker OS wait')
  record.update(passed=True,natural_guest_exit=exited.decode())
 except BaseException as error:
  record.update(error=repr(error));raise
 finally:
  try:
   record['cleanup']=server.close();raw=(folder/'broker.raw').read_text()
   record['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw
   live.require(record['queued_before_exec'],'original first-pause gate proof',raw)
   if record['passed']:
    live.require('TinyGo DAP original guest wait: 0' in raw,'actual original guest OS wait',raw)
    record['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(record,indent=2)+'\n')
 return record

def retain_completed_protocol(folder):
 """Archive only this completed session's protocol; verify every original byte.

 Successful live EOF/OS waits have already been checked. Keep session results
 and broker wait evidence unpacked; raw DAP stays recoverable in this archive.
 This keeps long qualifications below the unchanged shared disk floor.
 """
 paths=[p for p in (folder/'session-0').iterdir() if p.is_file()]
 pins={str(p.relative_to(folder)):{'bytes':p.stat().st_size,'sha256':live.sha(p)} for p in paths}
 archive=folder/'protocol.tar.xz'
 live.require(not archive.exists(),'new private protocol archive')
 with tarfile.open(archive,'w:xz',preset=1) as tar:
  for p in paths:tar.add(p,arcname=str(p.relative_to(folder)),recursive=False)
 with archive.open('rb') as f:os.fsync(f.fileno())
 with tarfile.open(archive,'r:xz') as tar:
  live.require({m.name for m in tar.getmembers()}==set(pins),'closed protocol archive')
  for member in tar:
   with tar.extractfile(member) as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
   live.require(member.size==pins[member.name]['bytes'] and digest==pins[member.name]['sha256'],'all raw protocol bytes recoverable',member.name)
 record=dict(archive=archive.name,bytes=archive.stat().st_size,sha256=live.sha(archive),original_files=pins,all_members_stream_verified=True,fsync_completed=True)
 (folder/'protocol-retention.json').write_text(json.dumps(record,indent=2)+'\n')
 for p in paths:
  live.require(live.sha(p)==pins[str(p.relative_to(folder))]['sha256'],'own raw unchanged before retirement',p)
  p.unlink()
 return record

def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('uwvm','build-receipt','producer-receipt','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--ros',action='store_true');ap.add_argument('--expect-opaque',action='store_true')
 ap.add_argument('--seconds',type=int,default=0);ap.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
 ap.add_argument('--profile',action='append')
 a=ap.parse_args();live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux runner')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 build=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text())
 live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and live.sha(a.uwvm)==build['binary_sha256'],'original full product')
 live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'original Rust producer receipt')
 profiles=[p for p in producer['wasms'] if not a.profile or p['profile'] in a.profile];live.require(bool(profiles),'actual producer profiles')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),a.uwvm,a.build_receipt,a.producer_receipt,root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh']
 for p in profiles:
  live.require(live.sha(Path(p['path']))==p['sha256'] and live.sha(Path(p['source']))==p['source_sha256'],'unchanged original compiler Wasm/source',p)
  paths += [Path(p['path']),Path(p['source'])]
 pins={str(p):live.sha(p) for p in paths};result=dict(passed=False,pins=pins,sessions=[],actual_ide_ui=False,full_language_parity=False)
 a.out.mkdir(mode=0o700);begin=time.monotonic()
 try:
  while True:
   p=profiles[len(result['sessions'])%len(profiles)]
   folder=a.out/f'session-{len(result["sessions"]):04}'
   x=session(a,folder,root,p)
   retain_completed_protocol(folder)
   result['sessions'].append(dict(passed=x['passed'],policy=x['policy'],profile=x['profile'],trees=len(x['trees']),tree_rows=sum(t['rows'] for t in x['trees'])))
   if len(result['sessions'])>=len(profiles) and time.monotonic()-begin>=a.seconds:break
   if a.seconds and len(result['sessions'])%len(profiles)==0:a.policy='unwind' if a.policy=='instruction' else 'instruction'
   live.require(len(result['sessions'])<1024,'bounded session count')
  result.update(passed=True,elapsed_seconds=time.monotonic()-begin)
 finally:
  result['pins_after']={p:live.sha(Path(p)) for p in pins}
  result['passed']=result['passed'] and result['pins_after']==pins
  (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 live.require(result['passed'],'actual Rust variants through DAP');print('PASS real Rust variant DAP',len(result['sessions']),'sessions')
 return 0
if __name__=='__main__':raise SystemExit(main())
