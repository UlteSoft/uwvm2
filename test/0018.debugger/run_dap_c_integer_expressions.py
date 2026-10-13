#!/usr/bin/env python3
"""Finite C-family copied integer expressions at authentic Wasm32/64 stops."""
from pathlib import Path
import argparse,json,os,re,subprocess,sys,time
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
from run_dap_enum_objects import retain_completed_protocol
sys.dont_write_bytecode=True

LEAVES={('pair','__0'):'-9',('pair','__1'):'42',
 ('tuple_struct','__0'):'-13',('tuple_struct','__1'):'55',
 ('nested','__0','__0'):'-7',('nested','__0','__1'):'21',
 ('nested','__1','__0'):'34',('nested','__1','__1'):'-11',
 ('arrays','[0]','__0'):'-3',('arrays','[0]','__1'):'8',
 ('arrays','[1]','__0'):'-5',('arrays','[1]','__1'):'13'}
ARITHMETIC={'object.pair.0 + (object.pair.1 as i32)':'33',
 'object.tuple_struct.0 + (object.tuple_struct.1 as i32)':'42',
 'object.nested.0.0 + (object.nested.0.1 as i32) + (object.nested.1.0 as i32) + object.nested.1.1':'37',
 'object.arrays[0].0 + (object.arrays[0].1 as i32) + object.arrays[1].0 + (object.arrays[1].1 as i32)':'13'}
UNSIGNED_BOUNDARIES={'object.pair.__1 + 2147483606u':'2147483648',
 'object.pair.__1 - 43u':'4294967295','object.pair.__1 - 42u':'0'}
# Exact primitive spellings come from the backend's copied_type_name, never a typedef.
INTEGER_BOUNDARIES=(
 ('(signed char)(object.pair.__0 - 119)','-128','signed char'),
 ('(signed char)(object.pair.__0 + 136)','127','signed char'),
 ('(unsigned char)(object.pair.__1 + 213u)','255','unsigned char'),
 ('(unsigned char)(object.pair.__1 - 42u)','0','unsigned char'),
 ('(short)(object.pair.__0 - 32759)','-32768','short'),
 ('(short)(object.pair.__0 + 32776)','32767','short'),
 ('(unsigned short)(object.pair.__1 + 65493u)','65535','unsigned short'),
 ('(unsigned short)(object.pair.__1 - 42u)','0','unsigned short'),
 ('(long)(object.pair.__0 - 2147483639)','-2147483648','long'),
 ('(long)(object.pair.__0 + 2147483656ll)','2147483647','long'),
 ('(unsigned long)(object.pair.__1 + 4294967253ull)','4294967295','unsigned long'),
 ('(unsigned long)(object.pair.__1 - 42u)','0','unsigned long'),
 ('(long long)(object.pair.__0 - 9223372036854775799ll)','-9223372036854775808','long long'),
 ('(long long)(object.pair.__0 + 9223372036854775816ull)','9223372036854775807','long long'),
 ('(unsigned long long)(object.pair.__1 + 18446744073709551573ull)','18446744073709551615','unsigned long long'),
 ('(unsigned long long)(object.pair.__1 - 42u)','0','unsigned long long'))
LONG64_BOUNDARIES=(
 ('(long)(object.pair.__0 - 9223372036854775799ll)','-9223372036854775808','long'),
 ('(long)(object.pair.__0 + 9223372036854775816ull)','9223372036854775807','long'),
 ('(unsigned long)(object.pair.__1 + 18446744073709551573ull)','18446744073709551615','unsigned long'))
REJECTED=('object.pair.2','object.pair.00','object.nested.0.2','object.nested.2.0',
 'object.arrays[-1].0','object.arrays[2].1','object.pair.0.missing','object.next.pair.0',
 'object.pair.0;continue','object.arrays[seed].0','object.pair.0=1','object.pair.0++')
def spelling(path,alias):
 value='object'
 for name in path:
  value+=name if name.startswith('[') else '.'+(name[2:] if alias and name.startswith('__') else name)
 return value

def session(a,folder,root,profile):
 folder.mkdir(mode=0o700);dap=base.load(root/'tools/debug/dap_adapter.py','tuple_dap')
 mode=[] if a.ros else ['-Rcc','jit','-Rcm','full']
 feature=['--wasm-feature-enable-memory64'] if profile['address_width']==64 else []
 vm=['-m','run',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable',*feature,'--run',profile['path']]
 server=live.BrokerSession(a.uwvm,Path(__file__).resolve(),root/'tools/debug/dap_adapter.py',vm,folder)
 rust=profile['language']=='rust'
 record=dict(passed=False,profile=profile['profile'],language=profile['language'],policy=a.policy,trees=[],tuple_evaluations=[],subobjects=[])
 try:
  initial=dap.UnixBroker(str(server.directory))
  try:live.require('error:' not in initial.request('pause'),'original queued pause')
  finally:initial.close()
  cf=folder/'session-0';cf.mkdir(mode=0o700);client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source');server.clients.append(client)
  source=Path(profile['source']);line=next(i for i,s in enumerate(source.read_text().splitlines(),1) if 'OBJECT_TUPLE_DAP_STOP' in s)
  bp=client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[{'line':line}]})['body']['breakpoints'];live.require(len(bp)==1 and bp[0]['verified'],'original tuple source breakpoint',bp)
  client.request('continue',{'threadId':live.actual_location(client)['thread']})
  for _ in range(64):
   stop=client.evaluate('wait')
   if b'stopped: breakpoint' in stop:break
   live.require(b'guest exited:' not in stop,'guest before actual tuple stop',stop)
  else:raise AssertionError('tuple source stop timeout')
  position=live.actual_location(client)
  def frame_now():
   rows=client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
   chosen=[f for f in rows if f.get('source',{}).get('path')==str(source) and f.get('line')==line and profile['probe'] in f['name']]
   live.require(len(chosen)==1,'original tuple Rust source frame',rows);return chosen[0]
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
   live.require(len(by)==len(flat)==22,'complete original Rust tuple tree',flat)
   live.require([n['path'][0] for n in flat if len(n['path'])==1]==['seed','pair','tuple_struct','nested','arrays','next'],'original tuple field order',flat)
   live.require(by[('seed',)]['value']=='5' and re.fullmatch(r'guest:0x[0-9a-f]+',by[('next',)]['value']) and by[('next',)]['value']!='guest:0x0','actual tuple seed/self pointer')
   for path,want in LEAVES.items():live.require(by[path]['value']==want and by[path]['type'] in ('i32','u32','int','unsigned int'),'actual signed/unsigned tuple leaf',(path,by[path],want))
   record['trees'].append(dict(entry=entry,rows=len(flat),members=flat));return flat
  live.require(object_row['variablesReference']>0,'actual tuple object expandable',object_row);first_tree=tree(object_row['variablesReference'],'Source variables')
  for context in ('watch','hover','variables'):
   row=client.request('evaluate',{'expression':'object','context':context,'frameId':frame['id']})['body'];readonly(row);live.require(tree(row['variablesReference'],context)==first_tree,'same copied tuple tree')
   for path,want in LEAVES.items():
    for alias in ((False,True) if rust else (False,)):
     expr=spelling(path,alias);row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row)
     live.require(row['result']==want and row['variablesReference']==0 and row['type'] in ('i32','u32','int','unsigned int'),'actual Rust tuple numeric and producer alias',(expr,row,want));record['tuple_evaluations'].append(dict(context=context,expression=expr,response=row))
   for expr,want in ({'(object.pair).0':'-9','(object.nested.0).1':'21','(*object.next).pair.0':'-9'} if rust else {'(object.pair).__0':'-9','(object.nested.__0).__1':'21','(*object.next).pair.__0':'-9'}).items():
    row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row);live.require(row['result']==want and row['variablesReference']==0,'real parenthesized/guest-pointer tuple selector',(expr,row,want));record['tuple_evaluations'].append(dict(context=context,expression=expr,response=row))
   for path in (('pair',),('tuple_struct',),('nested','__0'),('nested','__1'),('arrays','[0]'),('arrays','[1]')):
    for alias in ((False,True) if rust else (False,)):
     expr=spelling(path,alias);row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row);live.require(row['variablesReference']>0,'real tuple subobject expandable',(expr,row));children=members(row['variablesReference'])
     want=[(p[-1],v) for p,v in LEAVES.items() if p[:-1]==path];live.require([(n['name'],n['value']) for n in children]==want,'actual tuple subobject leaf order',(expr,children,want))
     live.require(client.request('variables',{'variablesReference':row['variablesReference'],'filter':'named','start':1,'count':1})['body']['variables']==children[1:],'actual tuple subobject named page');live.require(client.request('variables',{'variablesReference':row['variablesReference'],'filter':'indexed'})['body']['variables']==[],'tuple fields retain original named category');record['subobjects'].append(dict(context=context,expression=expr,response=row,children=children))
   for original,want in ARITHMETIC.items():
    expr=original if rust else original.replace(' as i32','')
    if not rust:
     for path in sorted(LEAVES,key=len,reverse=True):expr=expr.replace(spelling(path,True),spelling(path,False))
    row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row);live.require(row['result']==want and row['variablesReference']==0,'actual Rust tuple casts/arithmetic versus original guest oracle',(expr,row,want));record.setdefault('arithmetic',[]).append(dict(context=context,expression=expr,response=row))
   if not rust:
    for expr,want in UNSIGNED_BOUNDARIES.items():
     row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row)
     live.require(row['result']==want and row['type']=='unsigned int' and row['variablesReference']==0,'actual unsigned-int boundary versus original C/C++ guest oracle',(expr,row,want));record.setdefault('unsigned_boundaries',[]).append(dict(context=context,expression=expr,response=row))
   for expr,want,kind in INTEGER_BOUNDARIES+(LONG64_BOUNDARIES if profile['address_width']==64 else ()):
    row=client.request('evaluate',{'expression':expr,'context':context,'frameId':frame['id']})['body'];readonly(row)
    live.require(row['result']==want and row['type']==kind and row['variablesReference']==0,'finite integer boundary versus original compiler guest oracle',(expr,row,want,kind));record.setdefault('integer_boundaries',[]).append(dict(context=context,expression=expr,response=row))
  refused=list(REJECTED)
  if not rust:refused += [spelling(path,True) for path in LEAVES]
  for index,expr in enumerate(refused):
   frame=frame_now();n=client.queue([('evaluate',{'expression':expr,'context':('watch','hover','variables')[index%3],'frameId':frame['id']})])[0];reply=client.response(n)
   live.require(not reply['success'],'unsafe/absent/out-of-range tuple selector refused',(expr,reply));record.setdefault('refusals',[]).append(dict(expression=expr,response=reply))
  # Refresh genuine references, then queue a real step with stale reads.
  frame=frame_now();scope=next(s['variablesReference'] for s in client.request('scopes',{'frameId':frame['id']})['body']['scopes'] if s['name']=='Source variables')
  rootref=next(v['variablesReference'] for v in client.request('variables',{'variablesReference':scope})['body']['variables'] if v['name']=='object')
  pair=next(v for v in members(rootref) if v['name']=='pair');nested=next(v for v in members(rootref) if v['name']=='nested');inner=members(nested['variablesReference'])[0]
  array=next(v for v in members(rootref) if v['name']=='arrays');element=members(array['variablesReference'])[0]
  client.request('setBreakpoints',{'source':{'path':str(source)},'breakpoints':[]})
  queries=[('stepIn',{'threadId':position['thread'],'granularity':'instruction'}),('evaluate',{'expression':'object.nested.0.0' if rust else 'object.nested.__0.__0','context':'watch','frameId':frame['id']}),('scopes',{'frameId':frame['id']})]+[('variables',{'variablesReference':ref}) for ref in (scope,rootref,pair['variablesReference'],inner['variablesReference'],element['variablesReference'])]
  replies=[client.response(n) for n in client.queue(queries)];live.require(replies[0]['success'] and all(not r['success'] for r in replies[1:]),'real step retires tuple descendant refs',replies);record['queued_retirement']=replies
  after=live.actual_location(client);live.require(after['stop']>position['stop'],'actual new stop');client.request('continue',{'threadId':after['thread']})
  for _ in range(64):
   exited=client.evaluate('wait')
   if b'guest exited:' in exited:live.require(b'guest exited: 0' in exited,'guest tuple payload self check 130',exited);break
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
 p.add_argument('--ros',action='store_true');p.add_argument('--control',action='store_true');p.add_argument('--seconds',type=int,default=0);p.add_argument('--policy',choices=('instruction','unwind'),default='instruction');a=p.parse_args()
 live.require(sys.platform=='linux' and 0<=a.seconds<=900,'bounded Linux tuple runner');subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 b=json.loads(a.build_receipt.read_text());producer=json.loads(a.producer_receipt.read_text());live.require(b['passed'] and b['all_product_TUs_fresh'] and b['inputs_before_equals_after'] and live.sha(a.uwvm)==b['binary_sha256'],'original R41 full product');live.require(producer['passed'] and producer['inputs_before_equals_after'] and producer['wasm_bytes_not_rewritten'],'official original tuple Rust producer')
 profiles=[p for p in producer['wasms'] if (p['language']!='rust')==a.control];live.require(a.control and len(profiles)==16,'original Rust DWARF4/5 or C/C++ control profiles')
 paths=[Path(__file__),Path(base.__file__),Path(live.__file__),Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__),root/'test/0018.debugger/run_dap_enum_objects.py',a.uwvm,a.build_receipt,a.producer_receipt,root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',root/'tools/ci/require_wasm3_test_cgroup.sh']
 for profile in profiles:
  live.require(live.sha(Path(profile['path']))==profile['sha256'] and live.sha(Path(profile['source']))==profile['source_sha256'],'original Wasm/source bytes',profile);paths += [Path(profile['path']),Path(profile['source'])]
 pins={str(p):live.sha(p) for p in paths};result=dict(passed=False,pins=pins,sessions=[],actual_ide_ui=False,full_language_parity=False);a.out.mkdir(mode=0o700);begin=time.monotonic()
 try:
  while True:
   profile=profiles[len(result['sessions'])%len(profiles)];folder=a.out/f'session-{len(result["sessions"]):04}';r=session(a,folder,root,profile);retain_completed_protocol(folder)
   result['sessions'].append(dict(passed=r['passed'],policy=r['policy'],profile=r['profile'],trees=len(r['trees']),tree_rows=sum(t['rows'] for t in r['trees']),language=r['language'],tuple_evaluations=len(r['tuple_evaluations']),subobjects=len(r['subobjects']),arithmetic=len(r.get('arithmetic',[])),unsigned_boundaries=len(r.get('unsigned_boundaries',[])),integer_boundaries=len(r.get('integer_boundaries',[])),refusals=len(r.get('refusals',[]))))
   if len(result['sessions'])>=len(profiles) and time.monotonic()-begin>=a.seconds:break
   if a.seconds and len(result['sessions'])%len(profiles)==0:a.policy='unwind' if a.policy=='instruction' else 'instruction'
   live.require(len(result['sessions'])<1024,'bounded sessions')
  result.update(passed=True,elapsed_seconds=time.monotonic()-begin)
 finally:
  result['pins_after']={p:live.sha(Path(p)) for p in pins};result['passed']=result['passed'] and result['pins_after']==pins;(a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 live.require(result['passed'],'actual Rust tuple DAP');print('PASS real Rust tuple DAP',len(result['sessions']),'sessions')
 return 0
if __name__=='__main__':raise SystemExit(main())
