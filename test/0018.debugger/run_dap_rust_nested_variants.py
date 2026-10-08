#!/usr/bin/env python3
"""Authentic nested Rust variants; run only under the Linux cgroup supervisor."""
from pathlib import Path
import argparse,json,os,re,subprocess,sys,time
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
from run_dap_enum_objects import retain_completed_protocol
sys.dont_write_bytecode=True

EXPECTED={'negative':'Carry { payload: Negative { signed: -3 } }',
          'positive':'Carry { payload: Positive { unsigned: 7 } }', 'empty':'Empty',
          'some':'Wrapped { payload: Some { __0.__0.__0: 13 } }',
          'none':'Wrapped { payload: None }',
          'array':'Packet { items[0]: 17, items[1]: 19 }',
          'deep':'Top { payload: Carry { payload: Negative { signed: -3 } } }'}
BEFORE={'negative':'Carry','positive':'Carry','empty':'Empty','some':'Wrapped','none':'Wrapped',
        'array':'Packet { items.[0]: 17, items.[1]: 19 }','deep':'Top'}

def session(a,folder,root,profile):
 folder.mkdir(mode=0o700);dap=base.load(root/'tools/debug/dap_adapter.py','nested_variant_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 vm=['-m','run',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable','--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 record=dict(passed=False,profile=profile['profile'],policy=a.policy,trees=[],variant_evaluations=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:live.require('error:' not in initial.request('pause'),'original queued pause')
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700);client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source']);line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'OBJECT_NESTED_VARIANT_DAP_STOP' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints'];live.require(len(bp)==1 and bp[0]['verified'],'original nested source breakpoint',bp)
  client.request('continue',{'threadId':live.actual_location(client)['thread']})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'guest before actual nested stop',stop)
  else:raise AssertionError('nested source stop timeout')
  position=live.actual_location(client)
  def frame_now():
   rows=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   chosen=[f for f in rows if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'source_nested_variant_probe' in f['name']]
   live.require(len(chosen)==1,'original nested Rust source frame',rows);return chosen[0]
  frame=frame_now();scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes'];scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables'];object_row=next(v for v in variables if v['name']=='object')
  record.update(position=position,frame=frame,variables=variables,breakpoint=bp[0])
  def readonly(row):live.require(row['presentationHint']['attributes']==['readOnly'] and 'evaluateName' not in row and 'memoryReference' not in row,'copied readonly nested row',row)
  def members(ref):
   rows=client.request('variables',{'variablesReference':ref})['body']['variables']
   for row in rows:readonly(row)
   return rows
  def tree(ref,entry):
   flat=[]
   def visit(ref,path,depth):
    live.require(depth<=32 and len(flat)<1024,'bounded original tree')
    rows=members(ref)
    for kwargs,want in (({'filter':'named'},[r for r in rows if not re.fullmatch(r'\[(0|[1-9][0-9]*)\]',r['name'])]),({'filter':'indexed'},[r for r in rows if re.fullmatch(r'\[(0|[1-9][0-9]*)\]',r['name'])]),({'start':1,'count':2},rows[1:3])):
     live.require(client.request('variables',dict(variablesReference=ref,**kwargs))['body']['variables']==want,'original nested paging')
    for row in rows:
     n={k:v for k,v in row.items() if k!='variablesReference'};n['path']=path+[row['name']];flat.append(n)
     if row['variablesReference']:visit(row['variablesReference'],n['path'],depth+1)
   visit(ref,[],0);by={tuple(n['path']):n for n in flat}
   live.require(len(by)==len(flat)==76,'complete original nested Rust tree',flat)
   live.require([n['path'][0] for n in flat if len(n['path'])==1]==['seed',*EXPECTED,'next'],'original nested field order',flat)
   live.require(by[('seed',)]['value']=='5' and re.fullmatch(r'guest:0x[0-9a-f]+',by[('next',)]['value']) and by[('next',)]['value']!='guest:0x0','actual seed/self pointer')
   def case(path,tag,name,default=False):
    group=path+('<variant-part>',);live.require(by[group]['value']=='variant-part' and by[group+('object',)]['value']==tag,'VM selected original discriminant',(path,by))
    rows=[n for n in flat if tuple(n['path'][:-1])==group and n['value'].startswith('active variant')]
    live.require(len(rows)==1 and rows[0]['name']==name and rows[0]['value']==('active variant (default)' if default else 'active variant'),'VM selected unique active branch',(path,rows))
    return group+(name,name)
   for field,tag,name,value in (('negative','-3','Negative','-3'),('positive','7','Positive','7')):
    outer=case((field,),'2','Carry');inner=case(outer+('payload',),tag,name)
    leaf='signed' if field=='negative' else 'unsigned';live.require(by[inner+(leaf,)]['value']==value,'real nested signed/unsigned payload')
   case(('empty',),'7','Empty')
   for field,tag,name in (('some','13','Some'),('none','0','None')):
    outer=case((field,),'3','Wrapped');inner=case(outer+('payload',),tag,name,field=='some')
    if field=='some':live.require(by[inner+('__0','__0','__0')]['value']=='13','actual nested NonZero payload')
   array=case(('array',),'5','Packet')
   for i,v in enumerate(('17','19')):live.require(by[array+('items',f'[{i}]')]['value']==v,'actual variant array')
   top=case(('deep',),'1','Top');middle=case(top+('payload',),'2','Carry');inner=case(middle+('payload',),'-3','Negative');live.require(by[inner+('signed',)]['value']=='-3','actual three nested cases')
   for field,want in (BEFORE if a.expect_case_only else EXPECTED).items():live.require(by[(field,)]['value']==want,'real outer summary',(field,by[(field,)],want))
   record['trees'].append(dict(entry=entry,rows=len(flat),members=flat));return flat
  live.require(object_row['variablesReference']>0,'actual nested object expandable',object_row);first_tree=tree(object_row['variablesReference'],'Source variables')
  for context in ('watch','hover','variables'):
   row=client.request('evaluate',{'expression':'object','context':context,'frameId':frame['id']})['body'];readonly(row);live.require(tree(row['variablesReference'],context)==first_tree,'same copied nested tree')
   for field,want in (BEFORE if a.expect_case_only else EXPECTED).items():
    row=client.request('evaluate',{'expression':'object.'+field,'context':context,'frameId':frame['id']})['body'];readonly(row)
    live.require(row['result']==want and row['variablesReference']>0,'actual nested owner evaluation',(field,row,want));record['variant_evaluations'].append(dict(context=context,field=field,response=row))
  # Refresh genuine references, then queue a real step with stale reads.
  frame=frame_now();scope=next(s['variablesReference'] for s in client.request('scopes',{'frameId':frame['id']})['body']['scopes'] if s['name']=='Source variables')
  rootref=next(v['variablesReference'] for v in client.request('variables',{'variablesReference':scope})['body']['variables'] if v['name']=='object')
  negative=next(v for v in members(rootref) if v['name']=='negative');outergroup=members(negative['variablesReference'])[0];branch=next(v for v in members(outergroup['variablesReference']) if v['name']=='Carry')
  wrapper=members(branch['variablesReference'])[0];payload=members(wrapper['variablesReference'])[0];innergroup=members(payload['variablesReference'])[0];innerbranch=next(v for v in members(innergroup['variablesReference']) if v['name']=='Negative')
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  queries=[('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),('evaluate',{'expression':'object.negative','context':'watch','frameId':frame['id']}),('scopes',{'frameId':frame['id']})]+[('variables',{'variablesReference':ref}) for ref in (scope,rootref,outergroup['variablesReference'],innergroup['variablesReference'],innerbranch['variablesReference'])]
  replies=[client.response(n) for n in client.queue(queries)];live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real step retires nested descendant refs',replies);record['queued_retirement']=replies
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'actual new stop');client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:live.require(b'guest exited: 0' in exited,'guest nested payload self check 55',exited);break
  else:raise AssertionError('nested guest did not finish')
  server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker OS wait');record.update(passed=True,natural_guest_exit=exited.decode())
 except BaseException as e:record['error']=repr(e);raise
 finally:
  try:
   record['cleanup']=server.close();raw=(folder/'broker.raw').read_text();record['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw;live.require(record['queued_before_exec'],'actual original first-pause gate',raw)
   if record['passed']:live.require('TinyGo DAP original guest wait: 0' in raw,'original guest wait',raw);record['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(record,indent=2)+'\n')
 return record

def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('uwvm','build-receipt','producer-receipt','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--ros',action='store_true');p.add_argument('--expect-case-only',action='store_true');p.add_argument('--seconds',type=int,default=0);p.add_argument('--policy',choices=('instruction','unwind'),default='instruction');a=p.parse_args()
 live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux nested runner');subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 b=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text());live.require(b['passed'] and b['all_product_TUs_fresh'] and b['inputs_before_equals_after'] and live.sha(a.uwvm)==b['binary_sha256'],'original R41 full product');live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'official original nested Rust producer')
 profiles=producer['wasms'];live.require(len(profiles)==2,'DWARF4/5 profiles')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),root/'test/0018.debugger/run_dap_enum_objects.py',a.uwvm,a.build_receipt,a.producer_receipt,root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh']
 for profile in profiles:
  live.require(live.sha(Path(profile['path']))==profile['sha256'] and live.sha(Path(profile['source']))==profile['source_sha256'],'original Wasm/source bytes',profile);paths += [Path(profile['path']),Path(profile['source'])]
 pins={str(p):live.sha(p) for p in paths};result=dict(passed=False,pins=pins,sessions=[],actual_ide_ui=False,full_language_parity=False);a.out.mkdir(mode=0o700);begin=time.monotonic()
 try:
  while True:
   profile=profiles[len(result['sessions'])%len(profiles)];folder=a.out/f'session-{len(result["sessions"]):04}';r=session(a,folder,root,profile);retain_completed_protocol(folder)
   result['sessions'].append(dict(passed=r['passed'],policy=r['policy'],profile=r['profile'],trees=len(r['trees']),tree_rows=sum(t['rows'] for t in r['trees']),variant_evaluations=len(r['variant_evaluations'])))
   if len(result['sessions'])>=len(profiles) and time.monotonic()-begin>=a.seconds:break
   if a.seconds and len(result['sessions'])%2==0:a.policy='unwind' if a.policy=='instruction' else 'instruction'
   live.require(len(result['sessions'])<1024,'bounded sessions')
  result.update(passed=True,elapsed_seconds=time.monotonic()-begin)
 finally:
  result['pins_after']={p:live.sha(Path(p)) for p in pins};result['passed']=result['passed'] and result['pins_after']==pins;(a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 live.require(result['passed'],'actual nested Rust DAP');print('PASS real nested Rust DAP',len(result['sessions']),'sessions')
 return 0
if __name__=='__main__':raise SystemExit(main())
