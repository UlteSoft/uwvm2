#!/usr/bin/env python3
"""Actual original TinyGo array-object DAP; only the designated Linux cgroup."""
from pathlib import Path
import argparse,json,os,sys,time
import run_dap_tinygo_objects as base
live=base.live
sys.dont_write_bytecode=True
def session(a,folder,root):
 folder.mkdir(mode=0o700);dap=base.load(root/'tools/debug/dap_adapter.py','real_array_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 vm=['-m','run',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable','--run',str(a.wasm)]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder);record=dict(passed=False,policy=a.policy,objects=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   first=initial.request('pause');live.require('error:' not in first and 'guest exited:' not in first,'first queued pause',first);record['first_pause']=first
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700)
  client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  line=next(i for i,s in enumerate(a.source.read_text().splitlines(),1) if 'GO_ARRAY_READY' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(a.source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'real array source breakpoint',bp);record['breakpoint']=bp[0]
  client.request('continue',{'threadId':live.actual_location(client)['thread']})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'guest exited before array breakpoint',stop)
  else:raise AssertionError('actual array stop timeout')
  position=live.actual_location(client);stack=client.request('stackTrace',{'threadId':position['thread']})['body']
  frames=[f for f in stack['stackFrames'] if f.get('source',{}).get('path')==str(a.source) and f.get('line')==line and 'arrayProbe' in f['name']]
  live.require(len(frames)==1,'actual original arrayProbe source frame',stack)
  frame=frames[0];scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
  scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables']
  record.update(position=position,frame=frame,variables=variables)
  record.update(values=[],trees=[],texts=[],rejections=[])
  oracle={'arrayLen':4,'outerLen':2,'innerLen':3,'zeroLen':0,'zeroSizedLen':6}
  for name,want in oracle.items():
   values=[v for v in variables if v['name']==name]
   live.require(len(values)==1 and values[0]['value']==f'i32={want}' and values[0]['variablesReference']==0,'actual compiler argument',values)
  record['compiler_oracles']=oracle
  def readonly(value):
   live.require(value['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in value and 'evaluateName' not in value,'copied display only',value)
  def members(reference,expected,named=False):
   rows=client.request('variables',{'variablesReference':reference})['body']['variables']
   live.require([r['name'] for r in rows]==expected,'actual original field/index order',rows)
   for r in rows:readonly(r)
   live.require(client.request('variables',{'variablesReference':reference,'filter':'named' if named else 'indexed'})['body']['variables']==rows,'actual member classification')
   live.require(client.request('variables',{'variablesReference':reference,'filter':'indexed' if named else 'named'})['body']['variables']==[],'no invented member class')
   live.require(client.request('variables',{'variablesReference':reference,'start':1,'count':2})['body']['variables']==rows[1:3],'snapshot page')
   live.require(client.request('variables',{'variablesReference':reference,'start':1024})['body']['variables']==[],'snapshot empty page')
   return rows
  def semantic(rows):return [{k:v for k,v in r.items() if k!='variablesReference'} for r in rows]
  def holder(reference,label):
   fields=members(reference,['Array','Named','Matrix','Zero','ZeroSized','Pointer','NilPointer','ZeroPointer','Message'],True)
   by={r['name']:r for r in fields};observed=[fields]
   for name,want in (('Array',[1,2,3,4]),('Named',[5,6,7,8])):
    row=by[name];live.require(row['indexedVariables']==4 and row['namedVariables']==0,'actual fixed array count',row)
    values=members(row['variablesReference'],[f'[{i}]' for i in range(4)]);observed.append(values)
    live.require([int(r['value']) for r in values]==want and all(r['variablesReference']==0 for r in values),'original numeric array values',values)
   live.require(by['Named']['type']=='main.NamedArray','original named array type',by['Named'])
   matrix=members(by['Matrix']['variablesReference'],['[0]','[1]']);observed.append(matrix)
   for i,row in enumerate(matrix):
    live.require(row['indexedVariables']==3 and row['namedVariables']==0,'matrix inner count',row)
    values=members(row['variablesReference'],['[0]','[1]','[2]']);observed.append(values)
    live.require([int(r['value']) for r in values]==[i*3+1,i*3+2,i*3+3],'original matrix values',values)
   live.require(by['Zero']['variablesReference']==0,'zero-length array has no invented element',by['Zero'])
   zero=members(by['ZeroSized']['variablesReference'],[f'[{i}]' for i in range(6)]);observed.append(zero)
   live.require(all(r['type']=='struct' and r['value']=='struct' and r['variablesReference']==0 for r in zero),'six actual zero-sized elements',zero)
   message=members(by['Message']['variablesReference'],['ptr','len'],True);observed.append(message)
   live.require(message[0]['value'].startswith('guest:0x') and message[0]['variablesReference']==0 and message[1]['value']=='7' and message[1]['variablesReference']==0,'actual UTF-8 string carriers',message)
   for name in ('Pointer','NilPointer','ZeroPointer'):
    live.require(by[name]['variablesReference']==0 and by[name]['value'].startswith('guest:0x'),'copied member pointer stays leaf',by[name])
   live.require(by['NilPointer']['value']==by['ZeroPointer']['value']=='guest:0x0' and by['Pointer']['value']!='guest:0x0','actual nil/nonnil member pointers',fields)
   tree={'entry':label,'fields':fields,'rows':sum(len(r) for r in observed),'semantic':list(map(semantic,observed))}
   record['trees'].append(tree);return tree,message
  box=next(v for v in variables if v['name']=='box');live.require(box['variablesReference']>0 and box['type']=='pointer','actual Source variables root',box)
  readonly(box);source_tree,message=holder(box['variablesReference'],'Source variables')
  record['source_scope']={'root':box,'members':source_tree['fields']}
  for name in ('nilBox','nilArray'):
   nil=next(v for v in variables if v['name']==name);readonly(nil)
   live.require(nil['value']=='guest:0x0' and nil['variablesReference']==0,'actual Source nil root',nil)
  for context in ('watch','hover','variables'):
   aggregate=client.request('evaluate',{'expression':'*box','context':context,'frameId':frame['id']})['body'];readonly(aggregate)
   live.require(aggregate['result']=='main.ArrayHolder' and aggregate['type']=='main.ArrayHolder' and aggregate['namedVariables']==9,'actual holder evaluation',aggregate)
   tree,_=holder(aggregate['variablesReference'],context)
   live.require(tree['semantic']==source_tree['semantic'],'same copied array tree in Source and evaluation',tree)
   text=client.request('evaluate',{'expression':'box.Message','context':context,'frameId':frame['id']})['body'];readonly(text)
   live.require(text['result']=='"a\\xce\\xbb\\xf0\\x9f\\x99\\x82"' and text['type']=='string' and text['variablesReference']>0,'native-style escaped copied text',text)
   carriers=members(text['variablesReference'],['ptr','len'],True)
   live.require(semantic(carriers)==semantic(message),'text uses original copied ptr/len only',carriers)
   record['texts'].append({'context':context,'response':text,'members':carriers})
  cases=[('len(box.Array)',4),('cap(box.Array)',4),('len(box.Named)',4),('cap(box.Named)',4),
    ('len(box.Matrix)',2),('cap(box.Matrix)',2),('len(box.Matrix[0])',3),('cap(box.Matrix[1])',3),
    ('len(box.Zero)',0),('cap(box.Zero)',0),('len(box.ZeroSized)',6),('cap(box.ZeroSized)',6),
    ('len(box.Pointer)',4),('cap(box.Pointer)',4),('len(box.NilPointer)',4),('cap(box.NilPointer)',4),
    ('len(nilArray)',4),('cap(nilArray)',4),('len(*nilArray)',4),('cap(*nilArray)',4),
    ('len(nilBox.Array)',4),('cap(nilBox.NilPointer)',4),('len(nilBox.Zero)',0),('len(*box.NilPointer)',4),
    ('len(box.ZeroPointer)',0),('cap(box.ZeroPointer)',0),('len(*box.ZeroPointer)',0),('cap(*box.ZeroPointer)',0),
    ('len(box.Array)-1',3),('len(box.Array)+cap(box.Matrix)',6)]
  for context in ('watch','hover','variables'):
   for expression,want in cases:
    result=client.request('evaluate',{'expression':expression,'context':context,'frameId':frame['id']})['body'];readonly(result)
    live.require(result['result']==str(want) and result['variablesReference']==0,'actual array builtin versus original compiler oracle',(expression,result))
    record['values'].append({'expression':expression,'context':context,'expected':want,'response':result})
  for expression in ('cap(box.Message)','len(box.Array[0])','len(box.Matrix[2])','len(box.Array,box.Matrix)','cap(len(box.Array))','len(box.Array=1)','continue','box.Message\ncontinue'):
   fresh=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames'][0]
   reply=client.request('evaluate',{'expression':expression,'context':'watch','frameId':fresh['id']},success=False)
   record['rejections'].append({'expression':expression,'message':reply.get('message')})
  fresh=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames'][0]
  fresh_scopes=client.request('scopes',{'frameId':fresh['id']})['body']['scopes']
  fresh_scope=next(r['variablesReference'] for r in fresh_scopes if r['name']=='Source variables')
  fresh_vars=client.request('variables',{'variablesReference':fresh_scope})['body']['variables']
  lazy=next(r['variablesReference'] for r in fresh_vars if r['name']=='box')
  matrix=client.request('evaluate',{'expression':'box.Matrix','context':'watch','frameId':fresh['id']})['body']['variablesReference']
  inner=client.request('variables',{'variablesReference':matrix})['body']['variables'][0]['variablesReference']
  text=client.request('evaluate',{'expression':'box.Message','context':'watch','frameId':fresh['id']})['body']['variablesReference']
  live.require(all(r>0 for r in (fresh_scope,lazy,matrix,inner,text)) and len({fresh_scope,lazy,matrix,inner,text})==5,'live distinct references before real step')
  client.request('setBreakpoints',{'source':{'path':str(a.source)},'breakpoints':[]})
  queued=client.queue([('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),
    ('evaluate',{'expression':'len(box.Array)','context':'watch','frameId':fresh['id']}),
    *[('variables',{'variablesReference':r}) for r in (fresh_scope,lazy,matrix,inner,text)],
    ('scopes',{'frameId':fresh['id']})])
  replies=[client.response(n) for n in queued]
  live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'actual step retires lazy/nested/text/frame references',replies)
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'actual new stop')
  record['queued_retirement']=replies
  client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:
    live.require(b'guest exited: 0' in exited,'original array self-check',exited);break
  else:raise AssertionError('actual array fixture did not finish')
  server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker wait')
  record.update(passed=True,natural_guest_exit=exited.decode())
 except BaseException as error:
  record.update(error=repr(error));raise
 finally:
  try:
   record['cleanup']=server.close();raw=(folder/'broker.raw').read_text()
   record['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw
   live.require(record['queued_before_exec'],'original launch-gate evidence',raw)
   if record['passed']:live.require('TinyGo DAP original guest wait: 0' in raw,'original guest OS wait',raw);record['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(record,indent=2)+'\n')
 return record
def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('uwvm','build-receipt','wasm','source','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--seconds',type=int,default=0);ap.add_argument('--ros',action='store_true');ap.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
 a=ap.parse_args();live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux only')
 import subprocess
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 build=json.loads(a.build_receipt.read_text());live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and live.sha(a.uwvm)==build['binary_sha256'],'original R35 full product')
 a.out.mkdir(mode=0o700)
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),a.uwvm,a.build_receipt,a.wasm,a.source,root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh']
 pins={str(p):live.sha(p) for p in paths};r=dict(passed=False,pins=pins,actual_ide_ui=False,full_language_parity=False,sessions=[])
 begin=time.monotonic()
 try:
  while True:
   x=session(a,a.out/f'session-{len(r["sessions"]):04}',root)
   r['sessions'].append({'passed':x['passed'],'policy':x['policy'],'comparisons':len(x['values']),'trees':len(x['trees']),'tree_rows':sum(t['rows'] for t in x['trees']),'texts':len(x['texts'])})
   if time.monotonic()-begin>=a.seconds:break
   a.policy='unwind' if a.policy=='instruction' else 'instruction'
   live.require(len(r['sessions'])<128,'bounded session count')
  r.update(passed=True,elapsed_seconds=time.monotonic()-begin)
 finally:
  r['pins_after']={p:live.sha(Path(p)) for p in pins};r['passed']=r['passed'] and r['pins']==r['pins_after']
  (a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 live.require(r['passed'],'actual array-object DAP');print('PASS real array-object DAP',len(r['sessions']),'sessions',sum(r['comparisons'] for r in r['sessions']),'values')
 return 0
if __name__=='__main__':raise SystemExit(main())
