#!/usr/bin/env python3
"""Compiler-original saved-caller scalars through genuine Linux broker/DAP."""
from pathlib import Path
import argparse,json,subprocess,sys,time
import run_dap_tinygo_objects as base
import run_dap_current_broker as live
from run_dap_enum_objects import retain_completed_protocol
sys.dont_write_bytecode=True
DWARF_TOOL=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llvm-dwarfdump')


def session(a,folder,root,profile):
 folder.mkdir(mode=0o700)
 dap=base.load(root/'tools/debug/dap_adapter.py','caller_scalar_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 features=['--wasm-feature-enable-memory64'] if profile['address_width']==64 else []
 vm=['-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable',*features,'--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 r=dict(passed=False,profile=profile['profile'],optimization=profile['optimization'],policy=a.policy,evaluations=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:
   prepared=initial.request('status')
   live.require('prepared; no Wasm instruction executed' in prepared,'genuine prepared launch',prepared)
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700)
  client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source']);line=next(n for n,text in enumerate(source.read_text().splitlines(),1) if 'SOURCE_CALLER_DAP_STOP' in text)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints']
  live.require(len(bp)==1 and bp[0]['verified'],'compiler-original callee source breakpoint',bp)
  client.request('continue',{})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'actual callee stop required',stop)
  else:raise AssertionError('callee source breakpoint timeout')
  expected={'retained':'73','enabled':'true','fraction':'1.25','wider':'-2.5'}
  def inspect_frame(position,before=False,caller=True):
   stack=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   matches=[f for f in stack if f['name'].startswith('source_caller ') and f.get('moduleId')=='0']
   live.require(len(matches)==1,'one authenticated saved physical caller',stack)
   frame=matches[0] if caller else stack[0]
   wants=expected if caller else {'retained':'-99','enabled':'false','fraction':'9.5','wider':'7.25'}
   if caller:live.require(frame['line']==0 and frame['column']==0 and 'source' not in frame and 'instructionPointerReference' not in frame,'saved caller is not a guessed callee/native PC',frame)
   else:live.require(frame['name']=='source_primitive_probe','actual callee physical scope',frame)
   scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
   references=[s['variablesReference'] for s in scopes if s['name']=='Source variables']
   live.require(len(references)==1,'actual saved caller qualified Source variables',scopes)
   ref=references[0]
   variables=client.request('variables',{'variablesReference':ref})['body']['variables']
   selected={v['name']:v for v in variables if v['name'] in wants}
   live.require(set(selected)==set(wants),'actual saved caller variable inventory',variables)
   for context in ('watch','hover','variables'):
    for name,want in wants.items():
     reply=client.request('evaluate',{'expression':name,'context':context,'frameId':frame['id']})['body']
     live.require(reply['result']==want and reply['variablesReference']==0 and reply['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in reply,'genuine selected saved-frame scalar',(name,context,reply))
     live.require(reply.get('type','').startswith('volatile '),'Watch preserves original qualified producer type',(name,reply))
     if not before:live.require(selected[name]['type']==reply['type'],'Source variables and Watch/hover preserve the same qualified type',(name,selected[name],reply))
     r['evaluations'].append(dict(name=name,context=context,response=reply))
   for name,want in wants.items():
    row=selected[name]
    live.require(row['variablesReference']==0 and row['presentationHint']['attributes']==['readOnly'] and 'memoryReference' not in row,'source scalar is copied read-only',row)
    if before and caller:live.require('unavailable' in row['value'],'before Source scope differs from genuine Watch',(name,row))
    else:live.require(row['value']==want,'Source scope agrees with actual saved-frame Watch',(name,row))
   live.require(client.request('variables',{'variablesReference':ref,'start':1,'count':2})['body']['variables']==variables[1:3],'saved caller scope page order')
   return dict(position=position,frame=frame,scopes=scopes,variables=variables),frame,ref
  position=live.actual_location(client)
  current,_,_=inspect_frame(position,a.expect_missing,False);r['initial_current_frame']=current
  snapshot,frame,ref=inspect_frame(position,a.expect_missing)
  r['initial_saved_caller']=snapshot
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  replies=[client.response(n) for n in client.queue([
   ('evaluate',{'expression':'step wasm '+str(position['thread']),'context':'repl'}),
   ('evaluate',{'expression':'retained','context':'watch','frameId':frame['id']}),
   ('scopes',{'frameId':frame['id']}),
   ('variables',{'variablesReference':ref})])]
  live.require(replies[0]['success'] and all(not v['success'] for v in replies[1:]),'actual instruction step retires saved caller references',replies)
  r['queued_retirement']=replies
  after=live.actual_location(client)
  live.require(after['stop']>position['stop'] and after['function']==position['function'],'genuine callee instruction step',after)
  snapshot,_,_=inspect_frame(after,a.expect_missing);r['fresh_saved_caller']=snapshot
  current,_,_=inspect_frame(after,a.expect_missing,False);r['fresh_current_frame']=current
  client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:
    live.require(b'guest exited: 0' in exited,'caller scalars unchanged in actual original guest self checks',exited);break
  else:raise AssertionError('original guest terminal timeout')
  server.child.wait(timeout=15);live.require(server.child.returncode==0,'original broker wait')
  r.update(passed=True,natural_guest_exit=exited.decode())
 except BaseException as error:r['error']=repr(error);raise
 finally:
  try:
   r['cleanup']=server.close();raw=(folder/'broker.raw').read_text();r['queued_before_exec']='TinyGo DAP gate: authenticated first command queued before VM exec' in raw
   live.require(r['queued_before_exec'],'original prepared broker gate',raw)
   if r['passed']:
    live.require('TinyGo DAP original guest wait: 0' in raw,'actual original guest OS wait',raw);r['original_guest_wait_returncode']=0
  finally:(folder/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 return r


def main():
 root=Path(__file__).resolve().parents[2]
 if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):return base.main()
 p=argparse.ArgumentParser()
 for name in ('uwvm','build-receipt','producer-receipt','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--ros',action='store_true');p.add_argument('--only-profile');p.add_argument('--expect-missing',action='store_true')
 p.add_argument('--seconds',type=int,default=0);p.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
 a=p.parse_args();live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux cgroup driver')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 build=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text())
 live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and live.sha(a.uwvm)==build['binary_sha256'],'closed fresh full product')
 live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'] and len(producer['wasms'])==32,'genuine compiler-original producer profiles')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),root/'test/0018.debugger/run_dap_enum_objects.py',root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh',a.uwvm,a.build_receipt,a.producer_receipt,DWARF_TOOL]
 for v in producer['wasms']:
  live.require(live.sha(Path(v['path']))==v['sha256'] and live.sha(Path(v['source']))==v['source_sha256'],'original producer bytes',v)
  paths.extend([Path(v['path']),Path(v['source'])])
 pins={str(f):live.sha(f) for f in paths};a.out.mkdir(mode=0o700)
 r=dict(passed=False,pins=pins,sessions=[],dwarf_verify=[],actual_ide_ui=False,full_language_parity=False)
 begin=time.monotonic()
 try:
  for v in producer['wasms']:
   log=a.out/(v['profile']+'-dwarf-verify.log')
   with log.open('wb') as output:done=subprocess.run([str(DWARF_TOOL),'--verify',v['path']],stdout=output,stderr=subprocess.STDOUT,timeout=30)
   live.require(done.returncode==0,'original DWARF verifier',log.read_text()[-1000:])
   r['dwarf_verify'].append(dict(profile=v['profile'],returncode=done.returncode,log_sha256=live.sha(log)))
  profiles=[v for v in producer['wasms'] if v['profile']==a.only_profile] if a.only_profile else producer['wasms']
  live.require(bool(profiles),'selected original producer profile')
  while True:
   for v in profiles:
    folder=a.out/f'session-{len(r["sessions"]):04}';x=session(a,folder,root,v);retained=retain_completed_protocol(folder)
    r['sessions'].append(dict(passed=x['passed'],profile=x['profile'],policy=x['policy'],evaluations=len(x['evaluations']),protocol_retention=retained))
    if a.seconds:time.sleep(0.25)
   if time.monotonic()-begin>=a.seconds:break
   a.policy='unwind' if a.policy=='instruction' else 'instruction';live.require(len(r['sessions'])<1024,'bounded session count')
  r.update(passed=True,elapsed_seconds=time.monotonic()-begin,all_profiles_passed=not a.only_profile)
 finally:
  r['pins_after']={f:live.sha(Path(f)) for f in pins};r['passed']=r['passed'] and r['pins']==r['pins_after'];(a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
 live.require(r['passed'],'actual caller scalars Source/Watch agreement')
 print('PASS genuine saved caller scalars',len(r['sessions']),'sessions',flush=True)
 return 0


if __name__=='__main__':raise SystemExit(main())
