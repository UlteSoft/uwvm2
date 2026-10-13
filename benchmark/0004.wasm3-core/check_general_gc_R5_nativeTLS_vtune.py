#!/usr/bin/env python3
"""Pure Python command/environment contracts; no native launch."""
import ast
import copy
from pathlib import Path
import types

p=Path(__file__).with_name('prepare_general_gc_R5_nativeTLS_vtune.py')
ast.parse(p.read_text())
m=types.ModuleType('_command_only_vtune')
m.__file__=str(p)
exec(compile(p.read_text(),str(p),'exec'),m.__dict__)
d=m.driver()
s,c=d.dependencies()
n=0
def check(value):
    global n
    if not value:raise AssertionError('contract failed')
    n+=1
def rejects(callback):
    global n
    try:callback()
    except RuntimeError:n+=1
    else:raise AssertionError('invalid recipe accepted')
plan={'LD_LIBRARY_PATH':c.LD_PATH,'product':{'path':str(c.PRODUCT),'sha256':c.PRODUCT_SHA},
      'source_id':c.SID,'commands':[],'fixtures':{}}
for cell in m.CELLS:
    family,phase,count=cell.rsplit('-',2)
    argv=['taskset','-c','0',*c.argv_for(family,phase,int(count),'unwind')[5:]]
    plan['commands'].append({'profile':d.PROFILE,'fixture':cell,'argv':argv})
    plan['fixtures'][cell]={'path':argv[-1],'sha256':'a'*64}
oldenv={'LD_PRELOAD':'secret-injected-loader','UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT':'/capture',
        'VTUNE_CONFIG':'secret-config','LD_LIBRARY_PATH':c.LD_PATH,'RAYON_NUM_THREADS':'9'}
saved=copy.deepcopy(oldenv)
r=m.recipe(plan,m.DEFAULT_RESULT,{'path':m.VTUNE,'sha256':'b'*64,'bytes':123},oldenv)
check(oldenv==saved)
check(len(r['commands'])==8 and len(r['targets'])==4 and len(r['report_queries'])==20)
check(r['execute_ready'] is False and r['actual_current_VTune_collection_passed'] is False)
check(all(t['actual_PID_TID'] is None and t['actual_guest_environment'] is None for t in r['targets']))
check(r['controller_environment']['RAYON_NUM_THREADS']=='1')
check(r['controller_environment']['HOME'].startswith(str(m.DEFAULT_RESULT)))
check(set(r['controller_environment'])=={'HOME','TMPDIR','USER','LOGNAME','PATH','LANG','LC_ALL','LD_LIBRARY_PATH','RAYON_NUM_THREADS'})
check(all(v not in str(r['inherited_environment_inventory']) for v in ('secret-injected-loader','secret-config','/capture')))
for target in r['targets']:
    argv=next(v[1] for v in r['commands'] if v[0]==target['label'])
    check(argv[:4]==['/usr/bin/taskset','-c','16',m.VTUNE])
    check(argv[argv.index('--')+1:]==target['guest_argv'] and '-cpu-mask=0' not in argv)
    check('sampling-mode=sw' not in argv)
    check('sampling-mode=hw' in argv if target['collector']=='hotspots' else 'pmu-collection-mode=summary' in argv)
queries=[a for _,a in r['report_queries'] if a[-1] in ('-group-by=?','-filter=?')]
check(len(queries)==16)
check(all(a[-1].startswith('-') and a[-1] in ('-group-by=?','-filter=?') for a in queries))
check(not any(a[-1] in ('group-by=?','filter=?') for _,a in r['report_queries']))
check(all(a[-1][1:] not in ('-group-by=?','-filter=?') for a in queries))
rejects(lambda:m.profiler_argv('software',Path('/fake'),['taskset','-c','0','/fake']))
rejects(lambda:m.profiler_argv('hotspots',Path('/fake'),['taskset','-c','1','/fake','arg']))
bad=copy.deepcopy(plan);bad['commands'].pop()
rejects(lambda:m.recipe(bad,m.DEFAULT_RESULT,{},{}))
bad=copy.deepcopy(plan);bad['commands'].append(copy.deepcopy(bad['commands'][0]))
rejects(lambda:m.recipe(bad,m.DEFAULT_RESULT,{},{}))
check(m.PLAN_SHA=='c6e08fa3595470094b6c97db55c3ddd20e45440862e014a4003be4c03ba63f35')
check(m.DRIVER_SHA==d.digest(p.with_name('run_general_gc_R5_nativeTLS.py')))
print(f'VTune command-only contracts: {n} pure Python checks; no native/SSH execution')
