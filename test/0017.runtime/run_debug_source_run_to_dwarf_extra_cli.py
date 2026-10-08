from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys
ap=argparse.ArgumentParser()
for key in ('source-root','build-record','out','wasm','source','dwarfdump','validator'):ap.add_argument('--'+key,type=Path,required=True)
ap.add_argument('--language',choices=('zig','tinygo','objc'),required=True)
ap.add_argument('--policy',choices=('instruction','unwind'),required=True);ap.add_argument('--ros',action='store_true');a=ap.parse_args()
root=a.source_root.resolve(strict=True);sys.path.insert(0,str(root/'test/0017.runtime'))
import run_debug_source_step_cli as step
import run_debug_language_experience_cli as language
import run_debug_source_tinygo_cli as tinygo
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(exist_ok=False)
build=json.loads(a.build_record.read_text());assert build['passed'] and build['all_inputs_after_unchanged'] and build['new_objects_only'] and len(build['rows'])==4
assert Path(build['source_root']).resolve()==root
pins=dict(build['inputs']);pins.update({str(p):sha(p) for p in [a.build_record,a.wasm,a.source,a.dwarfdump,a.validator,Path(__file__),Path(step.__file__),Path(step.metadata_cli.__file__),Path(language.__file__),Path(tinygo.__file__)]})
for p,h in pins.items():assert sha(p)==h,p
binary=Path(build['binary']);assert sha(binary)==build['binary_sha256']
d=dict(passed=False,language=a.language,source_id=build['source_id'],product_sha256=sha(binary),producer_scope='retained original compiler artifact; no fresh producer compilation claimed',inputs=pins,rows=[],commands=[],cgroup=Path('/proc/self/cgroup').read_text())
def publish():(a.out/'results.json').write_text(json.dumps(d,indent=2)+'\n')
def run(argv,name):
 p=a.out/(name+'.log')
 with p.open('wb') as f:r=subprocess.run(list(map(str,argv)),stdout=f,stderr=subprocess.STDOUT,timeout=180)
 d['commands'].append(dict(argv=list(map(str,argv)),returncode=r.returncode,log_sha256=sha(p)));publish();assert r.returncode==0,p.read_text()[-1500:];return p.read_text()
def marker(text):
 matches=[i for i,line in enumerate(a.source.read_text().splitlines(),1) if text in line];assert len(matches)==1,(text,matches);return matches[0]
try:
 run([a.validator,'validate',a.wasm],'validate');run([a.dwarfdump,'--verify',a.wasm],'dwarf-verify');oracle=run([a.dwarfdump,'--debug-line',a.wasm],'debug-line')
 rows=language.fixture_statement_rows(oracle,a.source);sequences=step.line_sequences(oracle);expressions=step.code_expressions(a.wasm)
 if a.language=='zig':origin_name,leaf_name=b'probe_outer',b'probe_leaf';before_line,after_line,leaf_line=map(marker,['PROBE_CALL','PROBE_AFTER','PROBE_LEAF_VALUES'])
 elif a.language=='tinygo':origin_name,leaf_name=b'main.probeOuter',b'main.consume';before_line,after_line,leaf_line=map(marker,['CALL_VALUES','OUTER_VALUES','LEAF_VALUES'])
 else:origin_name,leaf_name=b'_start',b'_i_LanguageProbe__add_';before_line,after_line,leaf_line=map(marker,['int result =','if(result != 11)','OBJC_METHOD_READY'])
 assert all(any(r['line']==line for r in rows) for line in (before_line,after_line,leaf_line)),('required actual own-file statement missing',before_line,after_line,leaf_line)
 origin=(step.metadata_cli.function(a.wasm,origin_name.decode())[0] if a.language=='zig' else tinygo.named_function(a.wasm,origin_name)[0]);leaf=(step.metadata_cli.function(a.wasm,leaf_name.decode())[0] if a.language=='zig' else tinygo.named_function(a.wasm,leaf_name)[0])
 for mode in ('until','advance'):
  argv=[str(binary),'-Rdbg',*(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']),'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm)]
  s=step.Session(argv,a.out/(mode+'.log'),expressions,sequences,a.source);row=dict(passed=False,policy=mode,argv=argv,actions=s.actions,positions=s.positions);d['rows'].append(row);publish()
  try:
   s.begin(origin);old=language.own_seek(s,lambda p:p['function']==origin and p['line']==before_line and p['is_statement'],'authentic callsite')
   if a.language=='tinygo':
    missing=marker('AFTER_VALUES')
    assert not any(r['line']==missing for r in rows),'this negative route must use an actually omitted statement'
    bad=s.console.send(f'until {s.thread} {old["stop_id"]} {old["file"]}:{missing}')
    assert b'source target has no emitted statement in the permitted module/frame' in bad,bad
    retained=s.position();assert retained['stop_id']==old['stop_id'] and retained['offset']==old['offset']
    row['omitted_compiler_statement_refusal']=dict(line=missing,reply=bad.decode(),same_authentic_stop=True)
   target=after_line if mode=='until' else leaf_line
   reply=s.console.send(f'{mode} {s.thread} {old["stop_id"]-1} {old["file"]}:{target}');assert reply.startswith(b'error: '),reply
   retained=s.position();assert retained['stop_id']==old['stop_id'] and retained['offset']==old['offset']
   reply=s.send(f'{mode} {target}');assert b'stopped: selected participant step' in reply and b'error:' not in reply,reply
   new=s.position();assert new['stop_id']>old['stop_id'] and new['line']==target and new['is_statement'],(old,new)
   if mode=='until':assert new['function']==origin and new['physical_functions']==old['physical_functions'],(old,new)
   else:assert new['function']==leaf and len(new['physical_functions'])==len(old['physical_functions'])+1,(old,new)
   row.update(origin=old,destination=new,actual_target=target);s.finish_guest();row['passed']=True
  finally:
   s.console.finish();row.update(quit_returncode=s.console.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in s.console.transcript)
   assert row['quit_returncode']==0 and row['managed_shutdown_complete'];publish()
 assert all(sha(p)==h for p,h in pins.items());d.update(passed=True,inputs_after_unchanged=True)
except BaseException as e:d['error']=repr(e);raise
finally:publish()
print('PASS retained '+a.language+' actual until/advance compiler statement and physical frame checks',flush=True)
