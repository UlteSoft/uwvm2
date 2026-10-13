#!/usr/bin/env python3
"""Pure Python Node-plan/semantic/env/unchanged-guard checks; never invoke Node."""
from pathlib import Path
import ast, copy, hashlib, json, sys, tempfile, types
HERE=Path(__file__).parent
sys.path.insert(0,str(HERE))
import generate_general_gc as oracle
raw=(HERE/'run_node_general_measurement.py').read_bytes()
module=types.ModuleType('_node_source');module.__file__=str(HERE/'run_node_general_measurement.py')
exec(compile(raw,module.__file__,'exec'),module.__dict__)
parent=(HERE/'run_managed_general_measurement.py').read_text()
def segment(source,name):
    tree=ast.parse(source);node=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name)
    return ast.get_source_segment(source,node)
checks=0
def check(ok):
    global checks
    assert ok;checks+=1
for name in ('guard_class','tid_snapshot','sdk_tree'):
    check(segment(raw.decode(),name)==segment(parent,name))
check(hashlib.sha256(parent.encode()).hexdigest()==module.PARENT_SOURCE_SHA)
check(hashlib.sha256((HERE/'managed_general/node/GeneralGc.mjs').read_bytes()).hexdigest()==module.PORT_PINS['managed_general/node/GeneralGc.mjs'])
# Sanitation captures actual inherited knobs without printing sensitive values.
child,receipt=module.sanitized_environment(
    {'NODE_OPTIONS':'--require /bad','NODE_V8_COVERAGE':'/bad','NODE_COMPILE_CACHE':'/bad',
     'UV_THREADPOOL_SIZE':'999','NPM_CONFIG_NODE_OPTIONS':'--jitless','V8_FLAGS':'--bad',
     'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT':'/bad','LD_PRELOAD':'/bad','_JAVA_OPTIONS':'-bad',
     'COMPlus_gcServer':'1','PATH':'/actual'},
    {'NODE_OPTIONS':'','NODE_DISABLE_COMPILE_CACHE':'1','PYTHONDONTWRITEBYTECODE':'1'},'/actual/lib')
check(child=={'PATH':'/actual','NODE_OPTIONS':'','NODE_DISABLE_COMPILE_CACHE':'1',
              'PYTHONDONTWRITEBYTECODE':'1','LD_LIBRARY_PATH':'/actual/lib'})
check(len(receipt['removed_names'])==10)
check(receipt['effective_relevant']['PYTHONDONTWRITEBYTECODE']=='1')
for bad in ({},{'NODE_OPTIONS':'--single-threaded','NODE_DISABLE_COMPILE_CACHE':'1','PYTHONDONTWRITEBYTECODE':'1'}):
    try:module.sanitized_environment({},bad,'')
    except RuntimeError:check(True)
    else:check(False)
version={'node':'v26.10.0','v8':'14.6.202.34-node.actual','platform':'linux','arch':'x64'}
def result(row):
    expected=row['expected']
    data={'runtime':'node','node_version':version['node'],'v8_version':version['v8'],
      'platform':'linux','arch':'x64','actual_exec_argv':module.NODE_FLAGS,
      'family':row['family'],'phase':row['phase'],'iterations':row['iterations'],
      'warmup_iterations':250000,'warmup_rounds':row['warmup_rounds'],'execution_ns':123456,
      'root_groups':1024,'table_root_slots':2048 if row['family']=='reference-array' else 1024,
      'planned_syntax_allocations':expected['guest_planned_allocations'],'collector_roi':False,
      'gc_reclaimed_objects':None,'gc_collection_count':None,'physical_object_count_known':False,
      'numeric_array_representation':'Uint32Array + managed ArrayBuffer/backing storage',
      'actual_heap_size_limit':1073741824}
    data.update({k:expected[k] for k in ('step_checksum_u32','root_checksum_u32','last_lcg_u32','return_checksum_u32')})
    return data
with tempfile.TemporaryDirectory(prefix='uwvm-node-source-check-') as temporary:
    path=Path(temporary)/'actual.jsonl'
    for family in oracle.FAMILIES:
        for phase in oracle.PHASES:
            row={'family':family,'phase':phase,'iterations':65536,'warmup_rounds':0,
                 'expected':oracle.oracle(family,phase,65536),'actual_version':version,'argv':['actual']}
            actual=result(row);path.write_text(json.dumps(actual)+'\n')
            answer=module.semantic(row,path,0)
            check(answer['passed'] and not answer['collector_qualified'] and not answer['single_tid_hw_counter_qualified'])
            for key in ('step_checksum_u32','root_checksum_u32','last_lcg_u32','return_checksum_u32','planned_syntax_allocations'):
                wrong=copy.deepcopy(actual);wrong[key]^=1;path.write_text(json.dumps(wrong)+'\n')
                check(not module.semantic(row,path,0)['passed'])
    row={'family':'reference-array','phase':'allocate','iterations':2000000,'warmup_rounds':8,
         'expected':oracle.oracle('reference-array','allocate',2000000),'actual_version':version,'argv':['actual']}
    actual=result(row);path.write_text(json.dumps(actual)+'\n');check(module.semantic(row,path,0)['passed'])
    for key,value in [('execution_ns',0),('execution_ns',True),('execution_ns',float('inf')),
                      ('actual_exec_argv',['--single-threaded']),('collector_roi',True),
                      ('gc_collection_count',1),('node_version','v24.0.0'),
                      ('warmup_rounds',0),('iterations',True),('actual_heap_size_limit',0)]:
        wrong=copy.deepcopy(actual);wrong[key]=value;path.write_text(json.dumps(wrong)+'\n')
        check(not module.semantic(row,path,0)['passed'])
    path.write_text(json.dumps(actual)+'\n'+json.dumps(actual)+'\n');check(not module.semantic(row,path,0)['passed'])
    path.write_text(json.dumps(actual)+'\n');check(not module.semantic(row,path,1)['passed'])
    for missing in ('gc_collection_count','gc_reclaimed_objects'):
        wrong=copy.deepcopy(actual);del wrong[missing];path.write_text(json.dumps(wrong)+'\n')
        check(not module.semantic(row,path,0)['passed'])
    row['argv'].append('--gc-telemetry')
    actual['memory_snapshot_before']={k:1 for k in ('rss','heapTotal','heapUsed','external','arrayBuffers')}
    actual['memory_snapshot_after']=dict(actual['memory_snapshot_before'])
    actual['memory_snapshot_is_allocated_byte_counter']=False
    path.write_text('GC_TELEMETRY_START\nGC_TELEMETRY_END\n'+json.dumps(actual)+'\n')
    check(module.semantic(row,path,0)['passed'])
    actual['memory_snapshot_is_allocated_byte_counter']=True
    path.write_text('GC_TELEMETRY_START\nGC_TELEMETRY_END\n'+json.dumps(actual)+'\n')
    check(not module.semantic(row,path,0)['passed'])
    actual['memory_snapshot_is_allocated_byte_counter']=False;actual['memory_snapshot_after']['rss']=-1
    path.write_text('GC_TELEMETRY_START\nGC_TELEMETRY_END\n'+json.dumps(actual)+'\n')
    check(not module.semantic(row,path,0)['passed'])
# Shape/source audit is not JS parsing or runtime proof.
producer=ast.parse((HERE/'prepare_node_general_plan.py').read_bytes())
check(not any(isinstance(n,ast.Name) and n.id in ('subprocess','Popen') for n in ast.walk(producer)))
check('--single-threaded' not in (HERE/'prepare_node_general_plan.py').read_text())
print('PASS pure Node source/semantic checks',checks,'node_executed=false')
