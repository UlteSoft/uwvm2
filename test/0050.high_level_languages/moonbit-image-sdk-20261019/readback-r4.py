"""Independent data readback; does not execute compilers, targets or collectors."""
from pathlib import Path
import hashlib,json,os,resource,importlib.util,struct,fcntl,time
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1');O=V/'root-language-moonbit-image-20261019-a9';S=O/'source-r3'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
C=Path('/sys/fs/cgroup'+G);assert (C.stat().st_dev,C.stat().st_ino)==(29,19075)and(C/'memory.max').read_text().strip()=='68719476736'and(C/'memory.swap.max').read_text().strip()=='0'
def events():return {k:int(v)for k,v in(l.split()for l in(C/'memory.events').read_text().splitlines())}
before=events()
def H(p):
 p=Path(p);h=hashlib.sha256();assert not p.is_symlink()
 with p.open('rb')as f:
  a=os.fstat(f.fileno())
  while b:=f.read(65536):h.update(b)
  z=os.fstat(f.fileno())
 assert (a.st_dev,a.st_ino,a.st_size,a.st_mtime_ns,a.st_ctime_ns,a.st_nlink)==(z.st_dev,z.st_ino,z.st_size,z.st_mtime_ns,z.st_ctime_ns,z.st_nlink)
 return {'path':str(p),'bytes':a.st_size,'sha256':h.hexdigest()}
def check(row):
 actual=H(row['path']);assert actual['bytes']==row['bytes']and actual['sha256']==row['sha256'],row['path']
 return actual
def J(name):return json.loads((O/name).read_bytes())
def logs(row):
 assert row.get('passed',row.get('direct_child_terminal',False))and row.get('exit',row.get('exit_code'))==row.get('expected_exit',0)
 if 'ownership'in row:assert row['complete_output']and row['ownership']['closed']and not row['ownership']['signals']and not row['ownership']['retirement_errors']
 elif 'remaining_owned_group'in row:assert not row['remaining_owned_group']and not row['cleanup_signals']and row['error']is None
 for stream in ('stdout','stderr'):
  if isinstance(row.get(stream),dict):check(row[stream])
  else:
   p=O/'debug-r3'/row['label']/(stream+'.log');r=H(p);assert r['bytes']==row[stream+'_bytes']and r['sha256']==row[stream+'_sha256']
retirement=J('owned-duplicate-retirement.json');assert retirement['files_retired']==6116 and retirement['logical_bytes_retired']==276873700
check(retirement['archive'])
for root in retirement['roots']:assert not Path(root['path']).exists()
for r in J('source-inputs-r3.json')['members']:assert H(S/r['path'])['sha256']==r['sha256']
for r in J('source-current-guides.json')['members']:assert H(O/'source-current-guides'/r['path'])['sha256']==r['sha256']
module_count=0;models=0;image_models=0
for folder,count in [('models-default-r4',88),('models-owned-r4',92)]:
 report=J(folder+'/report.json');assert report['passed']and report['original_inputs_preserved']and report['actual_products_preserved']
 assert report['model_tests_per_target']=={'wasm':count,'wasm-gc':count}and report['ros_import_calls']==0
 assert len(report['modules'])==(8 if count==88 else 10)
 for m in report['modules']:
  check(m['wasm']);check(m['test_info']);check(m['manifest'])
  e=m['execution'];assert e['passed']and e['ros_import_calls']==0 and e['native_ros_delivery_cases']==0 and e['uwvm_execution_cases']==0
  assert e['model_tests']==len(e['results']);models+=e['model_tests'];module_count+=1
  for r in e['results']:assert r['passed'] is True
  info=json.loads(Path(m['test_info']['path']).read_bytes());expected=[(file,row['index'],row['name'])for file,rows in info['tests'].items()for row in rows]
  assert [(r['file'],r['index'],r['name'])for r in e['results']]==expected
  if '/wire/'in m['wasm']['path']:
   info=json.loads(Path(m['test_info']['path']).read_bytes());rows=info['tests']['wire_test.mbt'];assert len(rows)==48
   assert sum(r['name'].startswith('Image ')for r in rows)==22;image_models+=22
 for r in report['commands']:logs(r)
assert (models,image_models,module_count)==(360,88,18)
spec=importlib.util.spec_from_file_location('independent_readback_import_reader',S/'sdk/tools/check_wasm_imports.py');ck=importlib.util.module_from_spec(spec);spec.loader.exec_module(ck)
matrix=J('matrix-result-r3.json');assert matrix['status']=='ACTUAL_MOONBIT60_PRODUCTS120_DEFAULT_BASELINE_EXECUTIONS_PASSED'and len(matrix['artifacts'])==60
for a in matrix['artifacts']:
 check(a)
 if a['app']=='07_image_codec':assert len(ck.inspect(Path(a['path']).read_bytes()))==1
 else:
  report=ck.check(Path(a['path']).read_bytes(),a['app'][-1],('wasi_snapshot_preview1',),True);assert report['ROS_functions_checked']==(38 if a['app'].endswith('1')else 43)
 assert a['layout']['memory']==[[256,1024]]
for r in matrix['commands']+matrix['reused_build_commands']:logs(r)
ordinary=[r for r in matrix['commands']if r['label'].startswith('ordinary-')];assert len(ordinary)==120
for r in ordinary:assert not Path(r['stderr']['path']).read_bytes()and Path(r['stdout']['path']).read_bytes().endswith(b'\n')
extended=J('extended-result-r4.json');assert extended['status']=='ACTUAL_MOONBIT_IMAGE8_UWVM_LLVM4_WASMTIME_FUNCTIONAL_CASES_PASSED_WITH_EXPLICIT_GC_FEATURE'
closed=extended['reused_positive_commands']+extended['commands']
for r in closed:logs(r)
assert len([r for r in closed if r['label'].startswith(('ordinary-all-','stable-all-','ros-all-','ros-none-'))])==8
assert len([r for r in closed if r['label'].startswith('wasmtime-')and r['label']!='wasmtime-version'])==4
for r in closed:
 if r['label'].startswith(('ordinary-all-','stable-all-','ros-all-','ros-none-'))or(r['label'].startswith('wasmtime-')and r['label']!='wasmtime-version'):
  assert Path(r['stdout']['path']).read_bytes()==b'PASS_MOONBIT_IMAGE_CODEC\n'and not Path(r['stderr']['path']).read_bytes()
mapped=J('mapped-result-r2.json');assert mapped['status']=='ACTUAL_ABSOLUTE_SOURCE_MAP_AND_INDEPENDENT82_NATIVE_CODEC_CHECKS_VERIFIED'
for r in mapped['commands']:logs(r)
for m in mapped['mapped_products']:
 check(m['wasm']);check(m['map']);value=json.loads(Path(m['map']['path']).read_bytes());assert value['version']==3 and all(s.startswith('/')for s in value['sources'])
def code(path):
 r=ck.Reader(Path(path).read_bytes());r.take(8)
 while r.position<len(r.data):
  s=r.byte();b=bytes(r.take(r.uint()))
  if s==10:return b
 raise AssertionError('code absent')
assert code(O/'mapped-products-r1/image-codec.wasm')==code(O/'mapped-products-r2/image-codec.wasm')
native=mapped['native_closed_positive_reuse'];logs(native['command'])
frame=b'camera/optical';pixels=bytes.fromhex('ff000000ff00aabb0000ffffffffccdd')
wire=struct.pack('<III',0,1,2)+struct.pack('<I',len(frame))+frame+struct.pack('<II',2,2)+struct.pack('<I',4)+b'rgb8'+b'\0'+struct.pack('<II',8,len(pixels))+pixels
assert len(wire)==71 and native['expected_checks']==82
assert Path(native['command']['stdout']['path']).read_bytes()==wire.hex().encode()+b'\nPASS_NATIVE_IMAGE_CONTRACT checks=82\n'
probe=J('probe-result-r1.json');check(probe['native_product']);check(probe['native_source']);check(probe['native_compiler'])
debug=J('debug-r3-result.json');assert debug['status']=='ACTUAL_MOONBIT_THREE_REPOSITORY_SOURCE_MAP_WORKFLOWS_PASSED'and len(debug['sessions'])==3
for row in debug['commands']:logs(row)
for s,row in zip(debug['sessions'],debug['commands']):
 raw=(O/'debug-r3'/row['label']/'stdout.log').read_text()
 assert raw.startswith(''.join(a['reply']for a in s['actions']))
 assert [a['byte']for a in row['credits']]==[a['command']+'\n'for a in s['actions']if a['command']is not None]+['quit\n']
 assert s['finite_source_map_workflow']and not s['finite_full_value_workflow']and not s['value_available']
 actions=s['actions'];assert actions[0]['command']is None
 prints=[a for a in actions if a['command']and a['command'].startswith('print ')];assert len(prints)==3 and prints[0]['reply']==prints[1]['reply']and 'error:'in prints[2]['reply']
 assert 'source debug information'in prints[0]['reply']or'source value unavailable'in prints[0]['reply']
 assert any('Program exited normally.'in a['reply']for a in actions)
for p,r in debug['preserved_inputs'].items():assert H(p)['sha256']==r['sha256']
for r in J('tool-restoration-r2.json')['rows']:
 p=O/'moon-home-r2'/Path(r['relative']).relative_to('input-producer');assert H(p)['sha256']==r['sha256']
for row in J('result-r4.json')['tools']:check(row)
for row in matrix['runtimes']:check(row)
for pid in [4103460,4104705,4107235,4110346,4113121,4114866,4117389,4120422,4124305,4126060,4128226,4130961,4135541,4147300,4149115,4162913]:
 p=Path('/proc')/str(pid)
 if p.exists():assert (p/'stat').read_text().split(')')[-1].strip().startswith('Z '),'own controller still active'
lock=Path('/tmp/uwvm-hll-benchmark.lock');fd=os.open(lock,os.O_RDWR|os.O_NOFOLLOW);st=os.fstat(fd);busy=False
try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
except BlockingIOError:busy=True
else:fcntl.flock(fd,fcntl.LOCK_UN)
os.close(fd)
raw=int((C/'memory.current').read_text());admission={'status':'NOT_ADMITTED_ZERO_PERFORMANCE_TARGETS_AND_COLLECTORS','raw_memory_current':raw,'raw_memory_required_below':60<<30,'existing_benchmark_lock_busy':busy,'lock_device':st.st_dev,'lock_inode':st.st_ino,'performance_children_started':0,'hardware_collections':0,'changed_lock_cgroup_kernel_or_global_cache':False}
admission['eligible_when_observed']=not busy and raw<60<<30
if admission['eligible_when_observed']:admission['status']='ADMISSION_OPPORTUNITY_OBSERVED_NO_PERFORMANCE_SUPERVISOR_OR_COLLECTION_LAUNCHED'
(O/'performance-admission-r3.json').write_text(json.dumps(admission,indent=2)+'\n')
after=events();assert all(after[k]==0 for k in ('oom','oom_kill','oom_group_kill'))
report={'status':'INDEPENDENT_MOONBIT_IMAGE_FINITE_MATRIX_READBACK_PASSED','source_files':len(J('source-inputs-r3.json')['members']),'model_executions':models,'image_model_executions_including_selection_repeats':image_models,'distinct_image_methods_per_backend':22,'model_modules':module_count,'products':60,'ordinary_cases':120,'LLVM_functional_cases':8,'Wasmtime_functional_cases':4,'native_Host_codec_checks':82,'source_map_debug_workflows':3,'MoonBit_lexical_object_values_qualified':False,'new_native_ROS_Image_deliveries':0,'new_hardware_perf_QEMU_cache_credit':False,'cgroup':G,'before_events':before,'after_events':after,'shared_max_counter_attribution':'Shared cumulative counter; no exclusive attribution','performance_admission':admission,'reader_sha256':H(__file__)['sha256']}
(O/'readback-r4.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
