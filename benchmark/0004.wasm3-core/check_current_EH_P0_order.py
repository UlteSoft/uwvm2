#!/usr/bin/env python3
"""Pure fixed EH P0 ordering/actual-cold-data parser checks; no native launch."""
from pathlib import Path
import ast,copy,hashlib,json,sys,types
HERE=Path(__file__).parent
m=types.ModuleType('eh_order_source');m.__file__=str(HERE/'prepare_current_EH_P0_order.py')
raw=Path(m.__file__).read_bytes();exec(compile(raw,m.__file__,'exec'),m.__dict__)
p=m.parent();prefix=['env','LD_LIBRARY_PATH=/actual/canonical/lib','actual-clang']
rows={label.replace('_','-'):argv for label,argv in p.lists(prefix,['@actual-rsp'])['eh-cold']}
product={'product':{'path':str(p.D/'main')}}
observed={label:'native' if 'native-unwind' in label or label.startswith('plain-normal') else 'r2-phase' for label in rows}
checks=0
def check(ok):
    global checks
    assert ok;checks+=1
ordered=m.order(rows,product,p.EH,observed)
check(len(ordered['plain'])==16 and len(ordered['pure_hw'])==4 and len(ordered['vtune'])==3)
check([row['label'] for row in ordered['plain'][:12]]==m.ORDER)
check([row['label'] for row in ordered['plain'][12:]]==list(reversed([label for label in m.ORDER if label.startswith('eh-throws-')])))
for family,items in ordered.items():
    for row in items:
        check(row['argv'][:3]==['taskset','-c','0'])
        check(row['argv'][3:]==rows[row['label']][7:] and row['formal_acceptance'] is False)
for mutate in ('missing','extra','argv-exe','cpu-env','capture','trace','label','dispatch'):
    bad=copy.deepcopy(rows);seen=copy.deepcopy(observed);key=m.ORDER[0]
    if mutate=='missing':del bad[key]
    elif mutate=='extra':bad['extra']=bad[key]
    elif mutate=='argv-exe':bad[key][7]='/wrong'
    elif mutate=='cpu-env':bad[key][5]='UWVM_TEST_CPUSET=0-31'
    elif mutate=='capture':bad[key][2]='different'
    elif mutate=='trace':bad[key][bad[key].index('-Rllvm-call-stack')+1]='lazy'
    elif mutate=='label':bad['eh-throws-unwind-auto-extra']=bad.pop(key)
    elif mutate=='dispatch':seen[key]='r2-phase'
    try:m.order(bad,product,p.EH,seen)
    except (RuntimeError,KeyError):check(True)
    else:check(False)
check(not any(isinstance(n,ast.Name) and n.id in ('subprocess','Popen','spawn_stopped') for n in ast.walk(ast.parse(raw))))
# Saved actual fixed source-data bytes are read only when explicitly requested.
if len(sys.argv)>1:
    actual=Path(sys.argv[1])
    for name,pin in m.PINS.items():check(hashlib.sha256((actual/name).read_bytes()).hexdigest()==pin)
    after=json.loads((actual/'cold-after.json').read_bytes());summary=json.loads((actual/'summary.json').read_bytes())
    source=dict(json.loads((actual/'commands.json').read_bytes()))
    selected={key:source[key] for key in m.ORDER};seen={row['label']:row['dispatch_plan'] for row in summary['cells']}
    actualordered=m.order(selected,after['product'],p.EH,seen)
    check(after['passed'] is True and summary['passed'] is True and len(after['actual_cell_proofs'])==12)
    cold_plan=json.loads((actual/'plan.json').read_bytes())
    check(m.historical_init_only(summary,cold_plan))
    for wrong in (True,False,0,10155,'10154',None):
        altered=copy.deepcopy(summary);altered['after_init_only']=wrong
        check(not m.historical_init_only(altered,cold_plan))
    for wrong in ([10154,21356],[10155,21355],[],None):
        altered=copy.deepcopy(cold_plan);altered['current_init']=wrong
        check(not m.historical_init_only(summary,altered))
    check(all(after['actual_cell_proofs'][label]['actual_argv']==argv for label,argv in selected.items()))
    check(len(actualordered['plain'])==16)
print('PASS pure EH fixed P0 order checks',checks,'native_executed=false')
