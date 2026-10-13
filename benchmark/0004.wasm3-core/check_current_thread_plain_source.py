#!/usr/bin/env python3
"""Pure source/shape checks only; no native fixtures or /proc observations."""
import ast
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import types

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('thread_source',HERE/'run_current_thread_plain.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
checks=0
def check(value):
    global checks
    assert value;checks+=1
def rejected(function):
    try:function()
    except RuntimeError:return True
    return False

ast.parse((HERE/'run_current_thread_plain.py').read_text());check(True)
for filename,expected in (
 ('run_managed_general_measurement.py',m.MANAGED_SHA),('run_general_gc_R5_nativeTLS.py',m.R5_SHA),
 ('run_current_general_gc.py',m.SAMPLER_SHA),('run_current_host_pcore_hw_counting.py',m.HOST_SHA),
 ('run_current_pcore_hw_counting.py',m.HW_SHA),('run_current_pcore_diagnostic.py',m.BASE_SHA)):
    check(m.digest(HERE/filename)==expected)
check(m.process_cpu_allowed({'cpu':'0'},{'cpus':'0'}))
check(not m.process_cpu_allowed({'cpu':'0'},{'cpus':'0,2'}))
check(m.process_cpu_allowed({'cpu':'0,2'},{'cpus':'0'}))
check(m.process_cpu_allowed({'cpu':'0,2'},{'cpus':'0,2'}))
check(not m.process_cpu_allowed({'cpu':'0,2'},{'cpus':'2'}))
for bad in ('2','6','0,2,4','16','0-3'):check(not m.process_cpu_allowed({'cpu':'0,2'},{'cpus':bad}))
check(m.task_cpu_allowed('0,2',100,101,'2'))
check(m.task_cpu_allowed('0,2',100,101,'0'))
check(not m.task_cpu_allowed('0,2',100,100,'2'))
check(not m.task_cpu_allowed('0',100,101,'2'))
check(not m.task_cpu_allowed('0,2',100,101,'6'))
check(rejected(lambda:m.checked_replace('aa','a','b')))
check(rejected(lambda:m.checked_replace('aa','x','b')))
check(m.checked_replace('abc','b','x')=='axc')

managed=m.load('source_managed',HERE/'run_managed_general_measurement.py',m.MANAGED_SHA)
host=m.load('source_host',HERE/'run_current_host_pcore_hw_counting.py',m.HOST_SHA)
base=m.load('source_base',HERE/'run_current_pcore_diagnostic.py',m.BASE_SHA)
sampler=m.load('source_plain',HERE/'run_current_general_gc.py',m.SAMPLER_SHA)
hw=m.load('source_hw',HERE/'run_current_pcore_hw_counting.py',m.HW_SHA)
r5=m.load('source_r5',HERE/'run_general_gc_R5_nativeTLS.py',m.R5_SHA)
check(issubclass(m.thread_guard_class(managed,host,base,sampler,Path('/unused'),{}),host.HostGuard))
check(callable(m.spawn_view(hw,r5,'0',[]).spawn_stopped))
check(callable(m.spawn_view(hw,r5,'0,2',[]).spawn_stopped))
check(rejected(lambda:m.spawn_view(hw,r5,'2',[])))
check(base.BOOT.count('{0}')==1)
# Evaluate the actual main-expression with an existing telemetry key: its
# namespace override must preserve the other fields without duplicate kwargs.
tree=ast.parse((HERE/'run_current_thread_plain.py').read_text())
main_node=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='main')
assignment=next(node for node in main_node.body if isinstance(node,ast.Assign) and
                any(isinstance(target,ast.Name) and target.id=='base' for target in node.targets))
sentinel=object();adaptor=types.SimpleNamespace(telemetry=sentinel)
view=eval(compile(ast.Expression(assignment.value),'<actual namespace expression>','eval'),
          dict(types=types,original=base,adapted=adaptor))
check(view.telemetry is sentinel and view.BOOT==base.BOOT)
actual_helper=Path('/tmp/uwvm-current-thread-r4-plain-source-data-20261003-r1/closure.py')
if actual_helper.exists():
    source=actual_helper.read_text();check(m.digest(actual_helper)==m.HELPER_SHA)
    check(source.count('mode=sys.argv[1]')==2)
    check(source.count('\nmode=sys.argv[1]\n')==1)
    ast.parse(source.split('\nmode=sys.argv[1]\n',1)[0]);check(True)

keys=('wall_ns','thread_constructor_sum_ns','join_sum_ns','start_latency_ns','admission_ns','guest_interval_ns','release_ns')
synthetic=[]
for path in ('std_native','vm_full_entry','vm_raw_entry'):
 for workers in (1,4):
  for sample in range(9):
   for threaded in (False,True):
    row=dict(path=path,workers=workers,sample=sample,threaded=threaded,rounds=1024,
      checksum=sum(m.native_checksum(i) for i in range(workers))*1024,native_creations=0,**dict.fromkeys(keys,1))
    if not threaded:row.update(thread_constructor_sum_ns=0,join_sum_ns=0)
    synthetic.append(row)
marker='PASS real VM thread entry/checksum/markers; shared memory + atomic.fence; creation counter not instrumented'
with tempfile.TemporaryDirectory(prefix='thread-pure-source-') as temp:
 p=Path(temp)/'synthetic-only.log'
 def receipt(rows,kind='creation',trace='unwind',code=0,ending=marker):
    p.write_text('\n'.join(json.dumps(r) for r in rows)+'\n'+ending+'\n')
    return m.semantic(dict(kind=kind,trace=trace),p,code)
 check(receipt(synthetic)['passed'])
 for field,value in (('checksum',0),('native_creations',1),('rounds',1),('wall_ns',True),('wall_ns',-1),('workers',True)):
    bad=copy.deepcopy(synthetic);bad[0][field]=value;check(not receipt(bad)['passed'])
 check(not receipt(synthetic[:-1])['passed'])
 check(not receipt(synthetic+[synthetic[0]])['passed'])
 check(not receipt(synthetic,code=1)['passed'])
 check(not receipt(synthetic,ending=marker.replace('not instrumented','verified'))['passed'])
 parked=[dict(policy='unwind',sample=sample,round=r,wake_ns=1,notify_call_ns=0,qualification_ns=1,
    qualification_probes=1,empty_notify_retries=0,waiter_tid=sample+100,parked_task_state='S',notify_return=1,parked_wchan='futex_wait')
    for sample in range(9) for r in range(128)]
 ending='PASS Linux parked guest wait32 -> guest notify; stat S/futex wchan before notify=1; 9 samples x 128 wakeups, 2 warmups'
 check(receipt(parked,'parked',ending=ending)['passed'])
 for field,value in (('policy','instruction'),('parked_wchan','running'),('parked_task_state','R'),('notify_return',0),('waiter_tid',0),('wake_ns',True)):
    bad=copy.deepcopy(parked);bad[0][field]=value;check(not receipt(bad,'parked',ending=ending)['passed'])
 check(not receipt(parked[:-1],'parked',ending=ending)['passed'])
 items=m.commands(Path(temp))
 check(len(items)==4)
 check([r['cpu'] for r in items]==['0','0,2','0','0,2'])
 check(all(str(m.D) in r['argv'][3] for r in items))
 check(all(r['argv'][:3]==['taskset','-c',r['cpu']] for r in items))
 check(len({r['label'] for r in items})==4)
 check(all(r['argv'][4]==r['trace'] for r in items))
print(json.dumps(dict(pure_source_checks=checks,native_executed=False,performance_qualified=False)))
