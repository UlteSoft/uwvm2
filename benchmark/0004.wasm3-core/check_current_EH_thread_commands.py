#!/usr/bin/env python3
"""Pure source/command checks using a real successful current compiler prefix."""
from pathlib import Path
import argparse, ast, hashlib, json, types
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--actual-commands',type=Path,required=True)
a=p.parse_args()
source=Path(__file__).with_name('prepare_current_EH_thread_commands.py')
raw=source.read_bytes();tree=ast.parse(raw)
assert not any(isinstance(n,ast.Name) and n.id in ('subprocess','Popen','exec_command') for n in ast.walk(tree))
m=types.ModuleType('_current_command_source');m.__file__=str(source);exec(compile(raw,str(source),'exec'),m.__dict__)
actual=dict(json.loads(a.actual_commands.read_bytes()));prefix=actual['runtime-compile'][:actual['runtime-compile'].index('-MD')]
tail=actual['main-link'][len(prefix)+3:-2]
rows=m.lists(prefix,tail);checks=0
def check(ok):
    global checks
    assert ok
    checks+=1
check({k:len(v) for k,v in rows.items()}=={'eh-cold':12,'thread-build':6,'thread-cold':6,'thread-plain':4})
for label,argv in rows['eh-cold']:
    name=label.split('-')[0]
    check(argv[:3]==['env','-u',m.CAPTURE])
    check('UWVM_TEST_CPUSET=0,2,4,6,16-31' in argv and 'PYTHONDONTWRITEBYTECODE=1' in argv)
    check(argv[7]==str(m.D/'main'))
    check(argv[8:12]==['-Rcc','jit','-Rcm','full'] and '-Raot' not in argv)
    check(argv[argv.index('-Rllvm-cache-path')+1]=='disable')
    check(argv[-1]==str(m.F/(name+'.wasm')))
    check(('-WFD-exceptions' in argv)==(name=='plain_normal'))
    check(('-WFE-exceptions' in argv)==(name!='plain_normal'))
for label,argv in rows['thread-build']:
    check('UWVM_TEST_CPUSET=0,2,4,6,16-31' in argv and 'PYTHONDONTWRITEBYTECODE=1' in argv)
    check(str(m.D/'main.o') not in argv)
    if label.endswith('-compile'):
        check('-DUWVM2TEST_RUNNER_USE_LLVM_JIT' in argv and '-I'+str(m.S/'test/0013.uwvm_int/strict') in argv)
        check(('-DUWVM_THREAD_BENCH_QUALIFY_CREATION' in argv)==label.startswith('thread-qualifier'))
    else:
        check(argv.count(str(m.D/'runtime.o'))==1 and argv.count(str(m.D/'host-api.o'))==1)
        check(('-Wl,--export-dynamic' in argv)==label.startswith('thread-qualifier'))
        check(tail==argv[argv.index(str(m.D/'host-api.o'))+1:argv.index('-Wl,--export-dynamic') if '-Wl,--export-dynamic' in argv else -2])
for label,argv in rows['thread-cold']+rows['thread-plain']:
    check(argv[:3]==['env','-u',m.CAPTURE])
    check('UWVM_TEST_CPUSET=0,2,4,6,16-31' in argv and 'PYTHONDONTWRITEBYTECODE=1' in argv)
    check(not any(v in argv for v in ('perf','vtune')))
for relative,pin in m.TESTS.items():
    data=(source.parents[2]/relative).read_bytes()
    check(len(data)==pin['bytes'] and hashlib.sha256(data).hexdigest()==pin['sha256'])
for bad in ([v for v in prefix if not v.startswith('LD_LIBRARY_PATH=')],prefix+['LD_LIBRARY_PATH=changed']):
    try:m.lists(bad,tail)
    except RuntimeError:check(True)
    else:check(False)
check('ROS_supported' in raw.decode() and "'ROS_supported':False" in raw.decode())
# Exact single-path exception accepts neither another .pyc nor wrong old/current
# bytes; all source/receipt/finalizer pins remain actual immutable proof.
pins={str(m.PYC):m.PYC_NEW,str(m.S/'tools/debug/dap_adapter.py'):m.ADAPTER_SOURCE,
      str(m.CACHE_PROOF):m.CACHE_PROOF_PIN,str(m.CACHE_FINALIZER):{'bytes':5767,'sha256':m.CACHE_FINALIZER_SHA}}
m.content_pin=lambda path:dict(pins[str(path)])
m.sha=lambda path:m.CACHE_FINALIZER_SHA
class ReceiptPath:
    def read_bytes(self):return b'{}'
real_receipt=m.CACHE_PROOF
m.CACHE_PROOF=ReceiptPath()
pins[str(m.CACHE_PROOF)]=m.CACHE_PROOF_PIN
check(m.checked_parent_input('inputs',str(m.PYC),m.PYC_OLD) is True)
for group,path,old in [('tools',str(m.PYC),m.PYC_OLD),('inputs',str(m.PYC),m.PYC_NEW),
                       ('inputs',str(m.PYC.with_name('other.pyc')),m.PYC_OLD)]:
    try:m.checked_parent_input(group,path,old)
    except (RuntimeError,KeyError):check(True)
    else:check(False)
pins[str(m.PYC)]={'bytes':m.PYC_NEW['bytes'],'sha256':'0'*64}
try:m.checked_parent_input('inputs',str(m.PYC),m.PYC_OLD)
except RuntimeError:check(True)
else:check(False)
m.CACHE_PROOF=real_receipt
print('PASS pure current EH/thread command checks',checks,'native_executed=false')
