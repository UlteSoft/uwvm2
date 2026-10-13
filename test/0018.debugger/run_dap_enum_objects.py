#!/usr/bin/env python3
"""Actual compiler-produced enums through Linux guest stops and DAP stdio.

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
 dap=base.load(root/'tools/debug/dap_adapter.py','actual_enum_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 features=['--wasm-feature-enable-memory64','--wasm-feature-enable-table64'] if profile['address_width']==64 else []
 vm=['-m','run',*mode,*features,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable','--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 record=dict(passed=False,policy=a.policy,profile=profile['profile'],trees=[],enums=[],array_selectors=[],array_rejections=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   first=initial.request('pause');live.require('error:' not in first and 'guest exited:' not in first,'actual first queued pause',first)
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700)
  client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source'])
  line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'OBJECT_ENUM_DAP_STOP' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'actual producer source breakpoint',bp)
  client.request('continue',{'threadId':live.actual_location(client)['thread']})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'guest exited before source stop',stop)
  else:raise AssertionError('real enum stop timeout')
  position=live.actual_location(client);stack=client.request('stackTrace',{'threadId':position['thread']})['body']
  frames=[f for f in stack['stackFrames'] if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_enum_probe' in f['name']]
  live.require(len(frames)==1,'actual enum source frame',stack)
  frame=frames[0]
  scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
  scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables']
  record.update(position=position,frame=frame,variables=variables,breakpoint=bp[0])
  enum_expected={'shade':'3 (warm)','negative_shade':'-1 (negative)','unnamed_shade':'13'}
  if profile['cpp']:enum_expected.update(small='-3 (negative)',wide='18446744073709551615 (maximum)')
  def readonly(row):
   live.require(row['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in row and 'evaluateName' not in row,'readonly copied display',row)
  def members(ref):
   rows=client.request('variables',{'variablesReference':ref})['body']['variables']
   for row in rows:readonly(row)
   return rows
  def semantic(rows):return [{k:v for k,v in row.items() if k!='variablesReference'} for row in rows]
  def tree(ref,entry):
   fields=members(ref)
   names=['value','shade','negative_shade','unnamed_shade']+(['small','wide'] if profile['cpp'] else [])+['grid','next']
   live.require([r['name'] for r in fields]==names,'actual enum-containing field order',fields)
   by={r['name']:r for r in fields}
   live.require(by['value']['value']=='5' and by['value']['variablesReference']==0,'guest seed copied',fields)
   for name,want in enum_expected.items():
    live.require(by[name]['value']==want and by[name]['variablesReference']==0,'actual enum leaf',(name,by[name]))
   live.require(re.fullmatch(r'guest:0x[0-9a-f]+',by['next']['value']) and by['next']['value']!='guest:0x0' and by['next']['variablesReference']==0,'copied self-pointer leaf',by['next'])
   grid=members(by['grid']['variablesReference'])
   live.require([r['name'] for r in grid]==['[0]','[1]'],'actual grid row order',grid)
   allrows=[fields,grid]
   for i,row in enumerate(grid):
    elems=members(row['variablesReference']);allrows.append(elems)
    live.require([e['name'] for e in elems]==['[0]','[1]','[2]'] and [e['value'] for e in elems]==[str(i*3+j+1) for j in range(3)],'original guest array beside enum',elems)
    live.require(client.request('variables',{'variablesReference':row['variablesReference'],'filter':'indexed','start':1,'count':1})['body']['variables']==elems[1:2],'actual array page')
   for kwargs,want in (({'filter':'named'},fields),({'filter':'indexed'},[]),({'start':1,'count':2},fields[1:3]),({'start':1024},[])):
    live.require(client.request('variables',dict(variablesReference=ref,**kwargs))['body']['variables']==want,'actual enum object page')
   record['trees'].append(dict(entry=entry,rows=sum(map(len,allrows)),members=fields))
   return list(map(semantic,allrows))
  source_object=next(v for v in variables if v['name']=='object')
  if a.expect_opaque:
   live.require(source_object['variablesReference']==0,'before enum object source scope remains opaque',source_object)
   source_tree=None
  else:
   live.require(source_object['variablesReference']>0,'real Source enum object expandable',source_object)
   source_tree=tree(source_object['variablesReference'],'Source variables')
  for context in ('watch','hover','variables'):
   aggregate=client.request('evaluate',{'expression':'object','context':context,'frameId':frame['id']})['body'];readonly(aggregate)
   if a.expect_opaque:
    live.require(aggregate['variablesReference']==0 and 'value=3 (warm)' in aggregate['result'] and 'value=-1 (negative)' in aggregate['result'],'real old enum packet loses complete object tree',aggregate)
    record.setdefault('opaque',[]).append(aggregate)
   else:
    live.require(aggregate['variablesReference']>0,'actual aggregate after projection',aggregate)
    live.require(tree(aggregate['variablesReference'],context)==source_tree,'same actual copied tree at all DAP entries')
   for name,want in enum_expected.items():
    row=client.request('evaluate',{'expression':'object.'+name,'context':context,'frameId':frame['id']})['body'];readonly(row)
    live.require(row['variablesReference']==0,'real scalar has no children',row)
    if a.expect_opaque and '(' in want:
     live.require('value='+want in row['result'] and row['result'].startswith('source-value stop='),'original opaque enum leaf',row)
    else:live.require(row['result']==want,'real explicit enum leaf',(name,row))
    record['enums'].append(dict(context=context,name=name,expected=want,response=row))
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
     selected=next(f for f in fresh if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_enum_probe' in f['name'])
     denied=client.request('evaluate',{'expression':expression,'context':context,'frameId':selected['id']},success=False)
     record['array_rejections'].append(dict(context=context,expression=expression,response=denied))
   # Failed reads retire adapter references; obtain new live references before
   # the real step so stale-reference checks cannot pass due to an earlier error.
   fresh=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   frame=next(f for f in fresh if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_enum_probe' in f['name'])
   scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
   scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
   variables=client.request('variables',{'variablesReference':scope})['body']['variables']
   source_object=next(v for v in variables if v['name']=='object')
   live.require(source_object['variablesReference']>0,'fresh live object before actual step',source_object)
  # Queue a real instruction step and stale reads together. No test-side stop
  # identifier is fabricated, and no guest self-pointer is chased.
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  queries=[('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),
           ('evaluate',{'expression':'object.shade','context':'watch','frameId':frame['id']}),
           ('variables',{'variablesReference':scope}),
           ('scopes',{'frameId':frame['id']})]
  if not a.expect_opaque:queries.append(('variables',{'variablesReference':source_object['variablesReference']}))
  replies=[client.response(n) for n in client.queue(queries)]
  live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real step retires enum/frame references',replies)
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'real new stop after step')
  record['queued_retirement']=replies
  client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:
    live.require(b'guest exited: 0' in exited,'guest enum/array/selfpointer oracle 41',exited);break
  else:raise AssertionError('real enum guest did not finish')
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
 live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'original enum producer receipt')
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
   result['sessions'].append(dict(passed=x['passed'],policy=x['policy'],profile=x['profile'],enum_evaluations=len(x['enums']),trees=len(x['trees']),tree_rows=sum(t['rows'] for t in x['trees'])))
   if len(result['sessions'])>=len(profiles) and time.monotonic()-begin>=a.seconds:break
   if a.seconds and len(result['sessions'])%len(profiles)==0:a.policy='unwind' if a.policy=='instruction' else 'instruction'
   live.require(len(result['sessions'])<1024,'bounded session count')
  result.update(passed=True,elapsed_seconds=time.monotonic()-begin)
 finally:
  result['pins_after']={p:live.sha(Path(p)) for p in pins}
  result['passed']=result['passed'] and result['pins_after']==pins
  (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 live.require(result['passed'],'actual enums through DAP');print('PASS real enum DAP',len(result['sessions']),'sessions')
 return 0
if __name__=='__main__':raise SystemExit(main())
