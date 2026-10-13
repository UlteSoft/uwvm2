#!/usr/bin/env python3
"""Actual C-family primitive parameter display and lifetime; Linux cgroup only."""
from pathlib import Path
import argparse,json,math,re,struct,subprocess,sys,time
import run_dap_tinygo_objects as base
import run_dap_current_broker as live
from run_dap_enum_objects import retain_completed_protocol
sys.dont_write_bytecode=True
def expected_cases(profile):
 rows = {}
 for kind, width in (('i',8),('u',8),('i',16),('u',16),('i',32),('u',32),('i',64),('u',64),('i',profile['address_width']),('u',profile['address_width'])):
  prefix = ('s' if kind=='i' else 'u') + ('long' if len(rows)>=16 else str(width))
  if kind=='i':
   rows[prefix+'_min']=dict(width=width,kind=kind,value=str(-(1<<(width-1))))
   rows[prefix+'_max']=dict(width=width,kind=kind,value=str((1<<(width-1))-1))
  else:
   rows[prefix+'_zero']=dict(width=width,kind=kind,value='0')
   rows[prefix+'_max']=dict(width=width,kind=kind,value=str((1<<width)-1))
 rows['enabled']=dict(width=1,kind='bool',value='true')
 rows['disabled']=dict(width=1,kind='bool',value='false')
 return rows
DWARF_TOOL=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llvm-dwarfdump')
def check_display(row,expected):
 live.require(row.get('type')==expected['producer_type'] and row['variablesReference']==0 and row['presentationHint']['attributes']==['readOnly'],'copied primitive type and readonly leaf',row)
 live.require('memoryReference' not in row and 'evaluateName' not in row,'display does not grant an address or expression',row)
 spelling=row.get('result',row.get('value'))
 live.require(spelling==expected['value'],'actual native primitive spelling including full unsigned 64-bit value',(row,expected))
def session(a,folder,root,profile):
 folder.mkdir(mode=0o700);dap=base.load(root/'tools/debug/dap_adapter.py','primitive_parameter_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 features=['--wasm-feature-enable-memory64'] if profile['address_width']==64 else []
 vm=['-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable',*features,'--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 r=dict(passed=False,profile=profile['profile'],language=profile['language'],optimization=profile['optimization'],policy=a.policy,evaluations=[],raw_parameters=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   first=initial.request('status');live.require('prepared; no Wasm instruction executed' in first,'actual prepared debug launch',first);r['prepared']=first
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700);client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source']);line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'SOURCE_PRIMITIVE_DAP_STOP' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'actual primitive source breakpoint',bp);client.request('continue',{})
  for _ in range(64):
   stopped=client.evaluate('wait')
   if b'stopped: breakpoint' in stopped:break
   live.require(b'guest exited:' not in stopped,'guest must reach its real primitive stop',stopped)
  else:raise AssertionError('actual primitive source stop timeout')
  position=live.actual_location(client);rows=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
  frames=[f for f in rows if f.get('source',{}).get('path')==str(source) and f.get('line')==line and profile['probe'] in f['name']]
  live.require(len(frames)==1,'original primitive producer frame',rows);frame=frames[0]
  raw=client.evaluate(f"locals source {position['thread']} {position['stop']} 0").decode()
  r.update(position=position,frame=frame,raw_source_values=raw,breakpoint=bp[0])
  expected=expected_cases(profile)
  for name,entry in expected.items():
   carrier=(entry['kind']+str(entry['width'])+'='+entry['value']) if entry['kind']!='bool' else 'bool='+entry['value']
   match=re.search(r'^source parameter '+name+r' type=([^\n]+) = ('+re.escape(carrier)+r')$',raw,re.M)
   live.require(match is not None,'actual console carrier from original stopped primitive parameter',(name,raw))
   entry['producer_type']=match[1]
   r['raw_parameters'].append(dict(name=name,width=entry['width'],carrier=match[2],value=entry['value'],type=match[1]))
  scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes'];scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables'];r['variables']=variables
  by={v['name']:v for v in variables};live.require(set(expected)<=set(by),'all compiler-provided primitive parameters',variables)
  if not a.object_before:
   for name,entry in expected.items():check_display(by[name],entry)
  live.require(client.request('variables',{'variablesReference':scope,'start':3,'count':4})['body']['variables']==variables[3:7],'actual source variable page')
  for context in ('watch','hover','variables'):
   for name,entry in expected.items():
    row=client.request('evaluate',{'expression':name,'context':context,'frameId':frame['id']})['body'];check_display(row,entry)
    r['evaluations'].append(dict(name=name,context=context,response=row,expected_value=entry['value']))
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  queries=[('evaluate',{'expression':'step wasm '+str(position['thread']),'context':'repl'}),('evaluate',{'expression':'s32_min','context':'watch','frameId':frame['id']}),('scopes',{'frameId':frame['id']}),('variables',{'variablesReference':scope})]
  replies=[client.response(n) for n in client.queue(queries)]
  live.require(replies[0]['success'] and all(not row['success'] for row in replies[1:]),'actual Wasm step retires primitive frame and source variables',replies);r['queued_retirement']=replies
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'actual new stop after real step')
  client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:
    live.require(b'guest exited: 0' in exited,'original guest integer and Boolean boundary self checks',exited);break
  else:raise AssertionError('original primitive guest did not finish')
  server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker OS wait')
  r.update(passed=True,natural_guest_exit=exited.decode())
 except BaseException as error:r['error']=repr(error);raise
 finally:
  try:
   r['cleanup']=server.close();raw=(folder/'broker.raw').read_text()
   r['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw
   live.require(r['queued_before_exec'],'original launch gate evidence',raw)
   if r['passed']:
    live.require('TinyGo DAP original guest wait: 0' in raw,'original guest OS wait',raw);r['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 return r
def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 p=argparse.ArgumentParser()
 for name in ('uwvm','build-receipt','producer-receipt','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--only-profile');p.add_argument('--ros',action='store_true');p.add_argument('--sample',action='store_true');p.add_argument('--object-before',action='store_true');p.add_argument('--seconds',type=int,default=0);p.add_argument('--policy',choices=('instruction','unwind'),default='instruction');a=p.parse_args()
 live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux genuine primitive driver')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 build=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text())
 live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and live.sha(a.uwvm)==build['binary_sha256'],'verified fresh full product')
 live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'actual validated compiler product')
 profiles=producer['wasms'];live.require(len(profiles)==32 and len({x['profile'] for x in profiles})==32,'four C-family frontends, two widths, DWARF4/5, O0/O1')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),root/'test/0018.debugger/run_dap_enum_objects.py',root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh',a.uwvm,a.build_receipt,a.producer_receipt,DWARF_TOOL]
 for profile in profiles:
  live.require(live.sha(Path(profile['path']))==profile['sha256'] and live.sha(Path(profile['source']))==profile['source_sha256'],'original Wasm and source bytes',profile)
  paths+=[Path(profile['path']),Path(profile['source'])]
 pins={str(p):live.sha(p) for p in paths};a.out.mkdir(mode=0o700);r=dict(passed=False,pins=pins,sessions=[],dwarf_verify=[],actual_ide_ui=False,full_language_parity=False)
 begin=time.monotonic()
 try:
  for profile in profiles:
   log=a.out/(profile['profile']+'-dwarf-verify.log')
   with log.open('wb') as output:done=subprocess.run([str(DWARF_TOOL),'--verify',profile['path']],stdout=output,stderr=subprocess.STDOUT,timeout=30)
   live.require(done.returncode==0,'genuine DWARF verifier',log.read_text()[-1000:])
   r['dwarf_verify'].append(dict(profile=profile['profile'],returncode=done.returncode,log_sha256=live.sha(log)))
  selected=[profile for profile in profiles if profile['profile']==a.only_profile] if a.only_profile else profiles[:1] if a.sample else profiles
  live.require(bool(selected),'selected genuine producer profile exists')
  while True:
   for profile in selected:
    folder=a.out/f'session-{len(r["sessions"]):04}';x=session(a,folder,root,profile);retained=retain_completed_protocol(folder)
    r['sessions'].append(dict(passed=x['passed'],profile=x['profile'],policy=x['policy'],parameters=len(x['raw_parameters']),evaluations=len(x['evaluations']),protocol_retention=retained))
    if a.seconds:time.sleep(0.25)  # Bound retained output and yield to other cgroup users.
   if time.monotonic()-begin>=a.seconds:break
   a.policy='unwind' if a.policy=='instruction' else 'instruction';live.require(len(r['sessions'])<1024,'bounded actual session count')
  r.update(passed=True,elapsed_seconds=time.monotonic()-begin,all_profiles_passed=not a.sample and not a.only_profile)
 finally:
  r['pins_after']={p:live.sha(Path(p)) for p in pins};r['passed']=r['passed'] and r['pins']==r['pins_after'];(a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 live.require(r['passed'],'genuine primitive parameter DAP');print('PASS actual primitive parameters',len(r['sessions']),'sessions',flush=True)
 return 0
if __name__=='__main__':raise SystemExit(main())
