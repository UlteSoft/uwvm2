#!/usr/bin/env python3
"""Authentic Zig producer, copied primitive predicates and DAP lifetime; Linux cgroup only."""
from pathlib import Path
import argparse,json,subprocess,sys,time
import run_dap_tinygo_objects as base
from run_dap_enum_objects import retain_completed_protocol
live=base.live
sys.dont_write_bytecode=True
PREDICATES={
 'packet.enabled':True,'packet.disabled':False,'!packet.enabled':False,'!packet.disabled':True,
 'packet.enabled == true':True,'packet.disabled != false':False,
 'packet.seed == 3':True,'packet.seed != 3':False,'packet.seed < 4':True,
 'packet.seed <= 2':False,'packet.seed > 2':True,'packet.seed >= 4':False,
 'packet.enabled and !packet.disabled':True,'packet.disabled or packet.enabled':True,
 'packet.disabled and (1 / 0 > 0)':False,'packet.enabled or (1 / 0 > 0)':True,
 'false and (packet.seed / 0 > 0)':False,'true or (packet.seed / 0 > 0)':True,
 'false or true and false':False,'true or false and false':True,
 '(packet.seed == 3) and (packet.grid[1][2] == 15)':True,
 '(packet.seed != 3) or (packet.grid[1][2] == 15)':True,
 '!false':True,'!true':False,'true == false':False}
NUMBERS={'packet.seed':'3','packet.grid[0][0]':'10','packet.grid[1][2]':'15',
 'packet.grid[1][2] + packet.seed':'18','@as(i64, packet.seed)':'3',
 '@as(u8, 255)':'255','@as(i32, -1)':'-1'}
REFUSED=('packet.seed and true','false and packet.seed','false and missing','true or missing',
 'packet.enabled + 1','true == 1','!packet.seed','packet.enabled && true','packet.disabled || true',
 'true ? 1 : 0','packet.seed = 4','packet.seed++','packet.seed;continue',
 'packet.seed\ncontinue','packet.grid[2][0]','@as(u8, 256)','@as(i32, packet.enabled)',
 '@as(bool, packet.enabled)')
def finish_guest(client,server,thread):
 client.request('continue',{'threadId':thread})
 for _ in range(64):
  exited=client.evaluate('wait')
  if b'guest exited:' in exited:
   live.require(b'guest exited: 0' in exited,'Zig guest Boolean/array self checks',exited);break
 else:raise AssertionError('original Zig guest did not finish')
 server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker OS wait')
 return exited.decode()
def optimized_locations(client,source,record):
 """Qualify real sparse pieces; optimized-out bytes remain unavailable."""
 record['optimized_full_predicates_qualified']=False;record['wasm_steps']=[]
 client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
 def current():
  position=live.actual_location(client)
  rows=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
  frames=[f for f in rows if f.get('source',{}).get('path')==str(source) and 'predicate_probe' in f['name']]
  live.require(len(frames)<=1,'unambiguous optimized source frame',rows)
  if not frames:
   live.require(rows and rows[0].get('instructionPointerReference')==f"wasm:0:{position['function']}:{position['offset']}" and 'predicate_probe' in rows[0]['name'],'actual unmapped guest instruction, without a fabricated source frame',rows)
  return position,frames[0] if frames else None
 def view(frame):
  scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
  scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  values=client.request('variables',{'variablesReference':scope})['body']['variables']
  packet=next(v for v in values if v['name']=='packet')
  return scope,values,packet
 position,frame=current();live.require(frame is not None,'actual initial source breakpoint frame');scope,values,packet=view(frame)
 record['optimized_initial_variables']=values
 live.require(packet['variablesReference']==0 and 'no location active' in packet['value'],'compiler has no active packet location at the initial source row',packet)
 for _ in range(64):
  previous=position
  reply=client.evaluate('step wasm '+str(position['thread']))
  live.require(b'guest exited:' not in reply,'optimized packet location must become active before exit',reply)
  position,frame=current();live.require(position['stop']>previous['stop'],'actual Wasm step changes stop')
  if frame is None:
   record['wasm_steps'].append(dict(position=position,source_frame_available=False));continue
  scope,values,packet=view(frame);record['wasm_steps'].append(dict(position=position,frame=frame,variables=values))
  if packet['variablesReference']==0:continue
  fields=client.request('variables',{'variablesReference':packet['variablesReference']})['body']['variables']
  live.require([v['name'] for v in fields]==['seed','enabled','disabled','grid'],'actual sparse compiler member order',fields)
  if fields[0]['value']=='3':break
 else:raise AssertionError('actual optimized DWARF pieces never exposed the captured seed')
 for value in fields[1:3]:
  live.require(value['variablesReference']==0 and value['value'].startswith('unavailable'),'empty DW_OP_piece is unknown, not zero or inferred truth',value)
 grid=client.request('variables',{'variablesReference':fields[3]['variablesReference']})['body']['variables']
 live.require([v['name'] for v in grid]==['[0]','[1]'],'actual sparse two-dimensional array',grid)
 children=[]
 for row in grid:children+=client.request('variables',{'variablesReference':row['variablesReference']})['body']['variables']
 live.require(len(children)==6 and children[-1]['value']=='15' and all(v['value'].startswith('unavailable') for v in children[:-1]),'only compiler-provided constant array piece is readable',children)
 for context in ('watch','hover','variables'):
  for expression,want in (('seed','3'),('packet.seed','3'),('packet.grid[1][2]','15')):
   row=client.request('evaluate',{'expression':expression,'context':context,'frameId':frame['id']})['body']
   live.require(row['result']==want and row['type']=='i32' and row['variablesReference']==0,'actual optimized scalar and piece display',(expression,row))
   record['numbers'].append(dict(context=context,expression=expression,expected=want,response=row))
 record['optimized_sparse_tree']=dict(fields=fields,grid=grid,children=children)
 refs=(scope,packet['variablesReference'],fields[3]['variablesReference'],grid[0]['variablesReference'])
 requests=[('evaluate',{'expression':'step wasm '+str(position['thread']),'context':'repl'}),('scopes',{'frameId':frame['id']})]+[('variables',{'variablesReference':ref}) for ref in refs]
 replies=[client.response(n) for n in client.queue(requests)]
 live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real Wasm step retires optimized frame and sparse views',replies)
 record['queued_retirement']=replies
 after=live.actual_location(client);live.require(after['stop']>position['stop'],'actual optimized next stop')
 return after
def session(a,folder,root,profile):
 folder.mkdir(mode=0o700);dap=base.load(root/'tools/debug/dap_adapter.py','real_zig_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 feature=['--wasm-feature-enable-memory64','--wasm-feature-enable-table64'] if profile['address_width']==64 else []
 vm=['-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable',*feature,'--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 record=dict(passed=False,policy=a.policy,profile=profile['profile'],predicates=[],numbers=[],refusals=[])
 source=Path(profile['source']);line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'ZIG_PREDICATE_READY' in s)
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   first=initial.request('status')
   live.require('error:' not in first and 'prepared; no Wasm instruction executed' in first,'actual pre-execution debug launch',first)
   record['initial_pause']=first
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700)
  client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'actual Zig source breakpoint',bp)
  client.request('continue',{})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'original Zig guest exited before breakpoint',stop)
  else:raise AssertionError('actual Zig stop timeout')
  position=live.actual_location(client)
  def frame_now():
   rows=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   chosen=[f for f in rows if f.get('source',{}).get('path')==str(source) and f.get('line')==line and 'predicate_probe' in f['name']]
   live.require(len(chosen)==1,'original Zig source/inline frame',rows);return chosen[0]
  frame=frame_now();scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
  scope=next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
  variables=client.request('variables',{'variablesReference':scope})['body']['variables']
  record.update(position=position,frame=frame,variables=variables,breakpoint=bp[0])
  if profile['optimization']=='ReleaseSafe':
   after=optimized_locations(client,source,record)
   record.update(passed=True,natural_guest_exit=finish_guest(client,server,after['thread']))
   return record
  record['parameters']=[]
  for name,kind,want in (('seed','i32','3'),('enabled','bool','true'),('disabled','bool','false')):
   parameter=next(v for v in variables if v['name']==name)
   live.require(parameter['type']==kind and parameter['value']==want,'real Debug formal argument in Source variables',parameter)
   for context in ('watch','hover','variables'):
    row=client.request('evaluate',{'expression':name,'context':context,'frameId':frame['id']})['body']
    live.require(row['type']==kind and row['result']==want and row['variablesReference']==0,'real Debug formal argument display',(name,row))
    record['parameters'].append(dict(name=name,context=context,response=row))
  def readonly(row):live.require(row['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in row and 'evaluateName' not in row,'copied display only',row)
  def members(ref,want):
   rows=client.request('variables',{'variablesReference':ref})['body']['variables']
   live.require([r['name'] for r in rows]==want,'original compiler member order',rows)
   for row in rows:readonly(row)
   live.require(client.request('variables',{'variablesReference':ref,'start':1,'count':2})['body']['variables']==rows[1:3],'actual copied page')
   return rows
  packet=next(v for v in variables if v['name']=='packet');readonly(packet)
  live.require(packet['variablesReference']>0,'actual Zig packet object',packet)
  fields=members(packet['variablesReference'],['seed','enabled','disabled','grid']);by={r['name']:r for r in fields}
  for name,want in (('seed','3'),('enabled','true'),('disabled','false')):
   live.require(by[name]['value']==want and by[name]['variablesReference']==0,'actual Zig compiler leaf',(name,by[name]))
  live.require(by['enabled']['type']==by['disabled']['type']=='bool','actual Zig copied Boolean type',fields)
  grid=members(by['grid']['variablesReference'],['[0]','[1]']);inner_refs=[]
  for i,row in enumerate(grid):
   children=members(row['variablesReference'],['[0]','[1]','[2]']);inner_refs.append(row['variablesReference'])
   live.require([r['value'] for r in children]==[str(10+i*3+j) for j in range(3)],'original two-dimensional array',children)
  record['object_tree']=dict(fields=fields,grid=grid)
  for context in ('watch','hover','variables'):
   for expression,want in PREDICATES.items():
    row=client.request('evaluate',{'expression':expression,'context':context,'frameId':frame['id']})['body'];readonly(row)
    live.require(row['result']==('true' if want else 'false') and row['type']=='bool' and row['variablesReference']==0,'actual Zig primitive predicate type/truth',(expression,row,want))
    record['predicates'].append(dict(context=context,expression=expression,expected=want,response=row))
   for expression,want in NUMBERS.items():
    row=client.request('evaluate',{'expression':expression,'context':context,'frameId':frame['id']})['body'];readonly(row)
    live.require(row['result']==want and row['variablesReference']==0,'actual Zig finite numeric control',(expression,row,want))
    record['numbers'].append(dict(context=context,expression=expression,expected=want,response=row))
  for expression in REFUSED:
   fresh=frame_now();reply=client.request('evaluate',{'expression':expression,'context':'watch','frameId':fresh['id']},success=False)
   record['refusals'].append(dict(expression=expression,response=reply))
  frame=frame_now();scope=next(s['variablesReference'] for s in client.request('scopes',{'frameId':frame['id']})['body']['scopes'] if s['name']=='Source variables')
  packet=next(v for v in client.request('variables',{'variablesReference':scope})['body']['variables'] if v['name']=='packet')
  fields=members(packet['variablesReference'],['seed','enabled','disabled','grid']);gridref=next(v['variablesReference'] for v in fields if v['name']=='grid')
  inner=members(gridref,['[0]','[1]'])[0]['variablesReference']
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  requests=[('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),('evaluate',{'expression':'packet.enabled','context':'watch','frameId':frame['id']}),('scopes',{'frameId':frame['id']})]+[('variables',{'variablesReference':ref}) for ref in (scope,packet['variablesReference'],gridref,inner)]
  replies=[client.response(n) for n in client.queue(requests)]
  live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real step retires original Zig refs',replies)
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'new actual stop');record['queued_retirement']=replies
  record.update(passed=True,natural_guest_exit=finish_guest(client,server,after['thread']))
 except BaseException as e:record['error']=repr(e);raise
 finally:
  try:
   record['cleanup']=server.close();raw=(folder/'broker.raw').read_text()
   record['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw
   live.require(record['queued_before_exec'],'original launch gate evidence',raw)
   if record['passed']:live.require('TinyGo DAP original guest wait: 0' in raw,'original guest OS wait',raw);record['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(record,indent=2)+'\n')
 return record
def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('uwvm','build-receipt','producer-receipt','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--seconds',type=int,default=0);ap.add_argument('--ros',action='store_true');ap.add_argument('--sample',action='store_true');ap.add_argument('--debug-only',action='store_true',help='Explicit Debug subset; ReleaseSafe remains required by the default matrix');ap.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
 a=ap.parse_args();live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux genuine Zig driver')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 build=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text())
 live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and live.sha(a.uwvm)==build['binary_sha256'],'fresh pinned full R48 product')
 live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'authentic official Zig compiler product')
 profiles=producer['profiles'];live.require(len(profiles)==4 and len({p['profile'] for p in profiles})==4,'Debug/ReleaseSafe Wasm32/64 genuine producers')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),root/'test/0018.debugger/run_dap_enum_objects.py',a.uwvm,a.build_receipt,a.producer_receipt,root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh']
 paths += [Path(p[k]) for p in profiles for k in ('source','path')]
 for p in profiles:live.require(live.sha(Path(p['path']))==p['sha256'] and live.sha(Path(p['source']))==p['source_sha256'],'original compiler and source bytes',p)
 selected=[p for p in profiles if p['optimization']=='Debug'] if a.debug_only else profiles
 pins={str(p):live.sha(p) for p in paths};a.out.mkdir(mode=0o700);r=dict(passed=False,pins=pins,actual_ide_ui=False,full_language_parity=False,all_required_profiles_passed=False,all_launch_profiles_passed=False,optimized_full_predicates_qualified=False,scope='Debug full finite predicates and formal arguments; ReleaseSafe source break, real Wasm steps and sparse DW_OP_piece with explicit holes',explicit_debug_subset=a.debug_only,sessions=[])
 begin=time.monotonic()
 try:
  while True:
   for profile in selected[:1] if a.sample else selected:
    folder=a.out/f'session-{len(r["sessions"]):04}';x=session(a,folder,root,profile);retained=retain_completed_protocol(folder)
    r['sessions'].append(dict(passed=x['passed'],profile=x['profile'],policy=x['policy'],predicates=len(x['predicates']),parameters=len(x.get('parameters',[])),numbers=len(x['numbers']),refusals=len(x['refusals']),optimized_full_predicates_qualified=False,protocol_retention=retained))
   if time.monotonic()-begin>=a.seconds:break
   a.policy='unwind' if a.policy=='instruction' else 'instruction';live.require(len(r['sessions'])<1024,'bounded actual session count')
  r.update(passed=True,all_launch_profiles_passed=not a.debug_only and not a.sample,elapsed_seconds=time.monotonic()-begin)
 finally:
  r['pins_after']={p:live.sha(Path(p)) for p in pins};r['passed']=r['passed'] and r['pins']==r['pins_after'];(a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 live.require(r['passed'],'authentic Zig DAP');print('PASS real Zig DAP',len(r['sessions']),'sessions',flush=True)
 return 0
if __name__=='__main__':raise SystemExit(main())
