#!/usr/bin/env python3
"""Pure Python R5 binding/explicit-env contracts. Never launches a process."""
import ast
import copy
from pathlib import Path
import signal as real_signal
import types

path = Path(__file__).with_name('run_general_gc_R5_nativeTLS.py')
ast.parse(path.read_text())
ns = types.ModuleType('_r5_binding_contract')
ns.__file__ = str(path)
exec(compile(path.read_text(), str(path), 'exec'), ns.__dict__)
sampler, cold = ns.dependencies()
checks = 0


def check(ok):
    global checks
    if not ok:
        raise AssertionError('contract check failed')
    checks += 1


def rejects(callback):
    global checks
    try:
        callback()
    except (RuntimeError, KeyError, MemoryError):
        checks += 1
    else:
        raise AssertionError('invalid contract accepted')


fixtures = {}
commands = []
for family in cold.FAMILIES:
    for phase in cold.PHASES:
        for n in cold.ITERATIONS:
            k = {'mutable-struct': 1, 'reference-cycle': 2, 'numeric-array': 1, 'reference-array': 3}[family]
            name = f'{family}-{phase}-{n}'
            definition = {'schema': 'uwvm-general-gc-fixture-v1', 'family': family, 'phase': phase,
                          'iterations': n, 'root_ring': 1024, 'compact_numeric_eligible': False,
                          'wasm_sha256': 'a' * 64,
                          'expected': {'guest_planned_allocations': k * (n if phase == 'allocate' else 1024),
                                       'final_root_group_count': 1024, 'final_reachable_objects': 1024 * k,
                                       'return_checksum_u32': 1234, 'collection_count': None,
                                       'reclaimed_count': None}}
            fixture = cold.OLD / 'fixtures' / (name + '.wasm')
            fixtures[name] = {'path': str(fixture), 'sha256': 'a' * 64, 'definition': definition}
            commands.append({'fixture': name, 'profile': ns.PROFILE,
                             'argv': ['taskset', '-c', '0', *cold.argv_for(family, phase, n, 'unwind')[5:]]})
            commands.append({'fixture': name, 'profile': ns.WT_PROFILE,
                             'argv': ['taskset', '-c', '0', '/tmp/wasmtime', '-C',
                                      'cache=n,collector=copying', '-W',
                                      'all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y',
                                      str(fixture)]})
plan = {'schema': ns.SCHEMA, 'execute_ready': True, 'source_id': cold.SID,
        'source': str(cold.SOURCE), 'product': {'sha256': cold.PRODUCT_SHA, 'bytes': cold.PRODUCT_BYTES},
        'native_TLS_all_TUs': True, 'experiments': 'all omitted', 'formal_acceptance': False,
        'temperature_policy': 'observation_only', 'LD_LIBRARY_PATH': cold.LD_PATH,
        'fixtures': fixtures, 'commands': commands, 'files': [{}] * 2800,
        'wasmtime': {'path': '/tmp/wasmtime'}}
ns.validate_plan(plan, sampler, cold)
check(len(plan['fixtures']) == 16 and len(plan['commands']) == 32)
for key, value in (('schema', 'old-S6e-schema'), ('execute_ready', False), ('source_id', sampler.SOURCE_ID),
                   ('native_TLS_all_TUs', False), ('experiments', 'six enabled'),
                   ('formal_acceptance', True), ('temperature_policy', 'reject90'),
                   ('source', '/tmp/borrowed-source'), ('LD_LIBRARY_PATH', '/tmp/other')):
    wrong = copy.deepcopy(plan)
    wrong[key] = value
    rejects(lambda wrong=wrong: ns.validate_plan(wrong, sampler, cold))
wrong = copy.deepcopy(plan)
wrong['product']['sha256'] = sampler.PRODUCT_SHA
rejects(lambda: ns.validate_plan(wrong, sampler, cold))
wrong = copy.deepcopy(plan)
wrong['commands'][-1] = wrong['commands'][0]
rejects(lambda: ns.validate_plan(wrong, sampler, cold))
for old, new in (('-Rclog', '-Rct'), ('unwind', 'instruction'), ('disable', 'enable'),
                 ('-WFE-gc', '-WFE-simd'), ('--run', '--invoke')):
    wrong = copy.deepcopy(plan)
    wrong['commands'][0]['argv'][wrong['commands'][0]['argv'].index(old)] = new
    rejects(lambda wrong=wrong: ns.validate_plan(wrong, sampler, cold))
check(ns.LONG_SUMMARY_SHA == '9da5e9d032c0aa551888883af3e6403b6aa16196cb5f853d74f13f368f9d0580')
check(ns.HOST_SHA == sampler.HOST_SHA and ns.HW_SHA == sampler.HW_SHA and ns.BASE_SHA == sampler.BASE_SHA)
check(ns.SAMPLER_SHA == '91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8')

# All process/proc/signal operations below are Python fakes. No native child.
# Validate same immutable spawn code gets explicit env, and wrapper cleanup owns
# an entry that failed the new stopped-environment witness before caller return.
fake_state = {'env': {ns.CAPTURE_KEY: '/tmp/capture', 'LD_LIBRARY_PATH': '/tmp/ld', 'RAYON_NUM_THREADS': '1'},
              'raw': b'LD_LIBRARY_PATH=/tmp/ld\0RAYON_NUM_THREADS=1\0',
              'birth': 42, 'state': 'T', 'uid': [1000] * 4, 'cgroup': '0::/owned\n',
              'ready': False, 'popen': [], 'signals': [], 'wait4': [], 'closed': []}
originals = {key: getattr(ns, key) for key in ('os', 'Path', 'select', 'signal', 'subprocess')}
real_os = ns.os


class FakePath:
    def __init__(self, *parts):
        self.parts = parts
    def __truediv__(self, part):
        return FakePath(*self.parts, part)
    def read_bytes(self):
        return fake_state['raw']


def fake_popen(*args, **kwargs):
    fake_state['popen'].append(kwargs)
    return types.SimpleNamespace(pid=4321, returncode=None)


def fake_core_spawn(base, guard, role, argv, stream, inherited=()):
    # Rebound subprocess is private to the cloned fake function, as in the real
    # immutable spawn. This cannot invoke the real subprocess module.
    process = subprocess.Popen(['fake'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    entry = {'role': role, 'process': process, 'pidfd': 99, 'birth': 42, 'reaped': False}
    guard.owned.append(entry)
    return entry


fake_core_spawn.__globals__['subprocess'] = types.SimpleNamespace(Popen=lambda *a, **k: None)
fake_hw = types.SimpleNamespace(spawn_stopped=fake_core_spawn,
                               signal_owned=lambda entry, sig: fake_state['signals'].append((entry['pidfd'], sig)))
fake_guard = types.SimpleNamespace(cg='0::/owned\n', owned=[])
fake_base = types.SimpleNamespace(ident=lambda pid: {'birth': fake_state['birth'], 'state': fake_state['state'],
                                                   'uid': fake_state['uid'], 'cgroup': fake_state['cgroup']})
try:
    ns.os = types.SimpleNamespace(environ=fake_state['env'], fsencode=real_os.fsencode, fsdecode=real_os.fsdecode,
                                  wait4=lambda pid, opt: (fake_state['wait4'].append(pid) or (pid, 0, None)),
                                  waitstatus_to_exitcode=lambda status: status,
                                  close=lambda fd: fake_state['closed'].append(fd))
    ns.Path = FakePath
    ns.select = types.SimpleNamespace(select=lambda *args: ([99] if fake_state['ready'] else [], [], []))
    ns.signal = types.SimpleNamespace(pthread_sigmask=lambda how, mask: set(), SIG_BLOCK=0, SIG_SETMASK=1,
                                      SIGINT=2, SIGTERM=15, SIGKILL=9)
    ns.subprocess = types.SimpleNamespace(Popen=fake_popen, STDOUT=-2, PIPE=-1, DEVNULL=-3)
    receipts = []
    view = ns.stopped_environment_view(fake_hw, receipts)
    entry = view.spawn_stopped(fake_base, fake_guard, 'guest', ['fake'], None)
    check(entry['reaped'] is False and fake_state['popen'][-1]['env'].get(ns.CAPTURE_KEY) is None)
    check(fake_state['popen'][-1]['stdout'] == -1)
    check(fake_state['popen'][-1]['stderr'] == -2)
    check(fake_state['popen'][-1]['stdin'] == -3)
    check(fake_state['env'][ns.CAPTURE_KEY] == '/tmp/capture') # No controller-global env mutation.
    check(receipts[0]['capture_present'] is False and receipts[0]['original_birth'] == 42)
    check(receipts[0]['actual_loader_matches_plan'] and receipts[0]['stopped_before_guest_GO'])
    for key, invalid in (('raw', b'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1\0LD_LIBRARY_PATH=/tmp/ld\0RAYON_NUM_THREADS=1\0'),
                         ('raw', b'LD_LIBRARY_PATH=/tmp/wrong\0RAYON_NUM_THREADS=1\0'),
                         ('raw', b'LD_LIBRARY_PATH=/tmp/ld\0RAYON_NUM_THREADS=2\0'),
                         ('raw', b'LD_LIBRARY_PATH=/tmp/ld\0LD_LIBRARY_PATH=/tmp/ld\0RAYON_NUM_THREADS=1\0'),
                         ('birth', 43), ('state', 'R'), ('uid', [0] * 4), ('cgroup', '0::/foreign\n'),
                         ('ready', True)):
        saved = fake_state[key]
        fake_state[key] = invalid
        count = len(fake_state['wait4'])
        rejects(lambda: view.spawn_stopped(fake_base, fake_guard, 'guest', ['fake'], None))
        check(len(fake_state['wait4']) == count + 1 and fake_state['wait4'][-1] == 4321)
        check(fake_state['signals'][-1] == (99, 9) and fake_state['closed'][-1] == 99)
        check(fake_guard.owned[-1]['reaped'] is True)
        fake_state[key] = saved
    class RejectAppend(list):
        def append(self, item):
            raise MemoryError('synthetic pre-Popen audit allocation failure')
    count = len(fake_state['popen'])
    bad = ns.stopped_environment_view(fake_hw, RejectAppend())
    rejects(lambda: bad.spawn_stopped(fake_base, fake_guard, 'guest', ['fake'], None))
    check(len(fake_state['popen']) == count)
finally:
    for key, value in originals.items():
        setattr(ns, key, value)

# Original check failure can be rechecked only after actual owned retirement.
# Entire protocol/proc/clock below is mocked; never launches a child or thread.
terminal_saved = {k: getattr(ns, k) for k in ('os', 'Path', 'select', 'time', 'save')}
terminal = {}
terminal_plan = {'product': {'path': '/fake/product'}, 'wasmtime': {'path': '/fake/wasmtime'}}
def setup_terminal(kind='reference', fault=None):
    terminal.clear()
    terminal.update(kind=kind, fault=fault, stage='initial', step=0, clock=100, audits=[], checks=0)
    parent = 777
    initial = {'pid': 123, 'birth': 42, 'ppid': parent, 'pgid': 123, 'state': 'R',
               'uid': [1000]*4, 'cgroup': 'cg', 'cpus': '0', 'argv': [], 'rss': 0}
    if kind == 'guest':
        initial.update(argv=[b'/fake/product'], rss=51<<20)
    terminal['initial'] = initial
    if fault in ('uid', 'cgroup', 'birth', 'ppid', 'pgid', 'cpus'):
        terminal['initial'][fault] = {'uid':[0]*4, 'cgroup':'foreign', 'birth':43,
                                     'ppid':778, 'pgid':124, 'cpus':'1'}[fault]
    if fault == 'initial_argv':
        initial.update(argv=[b'/wrong/exec'], rss=4096)
    actual = {'pid':123, 'birth':42, 'ppid':parent, 'pgid':123, 'uid':[1000]*4,
              'cgroup':'cg', 'cpus':'0', 'argv':['/fake/product'], 'executable':'/fake/product'} if kind=='guest' else None
    entry = {'role': kind, 'process': types.SimpleNamespace(pid=123, returncode=None),
             'pidfd':99, 'birth':42, 'cpu':'0', 'expected_exe':'/fake/product' if kind=='guest' else '/fake/wasmtime',
             'exec_argv':[b'/fake/product'], 'actual_exec':actual, 'reaped':False,
             'admitted':{'pid':123, 'birth':42, 'ppid':parent, 'pgid':123, 'uid':[1000]*4,'cgroup':'cg','cpus':'0'}}
    if fault=='product_no_exec': entry['actual_exec']=None
    if fault=='foreign_exe': entry['expected_exe']='/foreign/product'
    if fault=='perf_role': entry['role']='perf'
    terminal['entry']=entry
    return entry

class TerminalPath:
    def __init__(self, *parts): self.parts=parts
    def __truediv__(self, part): return TerminalPath(*self.parts,part)
    def read_text(self): return terminal['stage']
def terminal_stat(raw):
    mm={str(i):0 for i in range(12)}
    if terminal['fault']=='initial_MM' and raw=='initial': mm['0']=4096
    if terminal['fault']=='fresh_MM' and raw=='retiring': mm['0']=4096
    return {'pid':123,'birth':42,'ppid':777,'pgid':123,
            'state':'Z' if terminal['step']>=1 else 'R','mm':mm,'raw':raw}
def terminal_ident(pid):
    if terminal['stage']=='initial': return copy.deepcopy(terminal['initial'])
    row={'pid':123,'birth':42,'ppid':777,'pgid':123,'state':'Z' if terminal['step']>=1 else 'R',
         'uid':[1000]*4,'cgroup':'cg','cpus':'0','argv':[],'rss':0}
    if terminal['fault']=='fresh_uid': row['uid']=[0]*4
    if terminal['fault']=='fresh_argv': row['argv']=[b'/wrong/exec']
    return row
def terminal_select(*args):
    return ([99] if terminal['step']>=1 else [],[],[])
def terminal_clock():
    if terminal['fault']=='timeout' or (terminal['fault']=='late_proof' and terminal['step']>=1):
        terminal['clock'] += 2_000_000_001
    else:
        terminal['clock'] += 100
    return terminal['clock']
def terminal_sleep(seconds):
    terminal['step'] += 1
    if terminal['fault']=='changed_FD': terminal['entry']['pidfd']=100
def terminal_observed_guard(host, base, admission, out):
    guard=types.SimpleNamespace(owned=[terminal['entry']],cg='cg')
    def original_check():
        terminal['checks']+=1
        if terminal['checks']==1:
            base.ident(123)
            terminal['stage']='retiring'
            raise RuntimeError('Missing executable is not proved retired' if terminal['kind']=='guest' else
                               ('Unowned concurrent cgroup process' if terminal['fault']=='unrelated_error' else 'Owned command changed'))
        check(terminal['step']>=1)
    guard.check=original_check
    return guard
try:
    ns.os=types.SimpleNamespace(getpid=lambda:777,fsdecode=real_os.fsdecode)
    ns.Path=TerminalPath
    ns.select=types.SimpleNamespace(select=terminal_select)
    ns.time=types.SimpleNamespace(perf_counter_ns=terminal_clock,sleep=terminal_sleep)
    ns.save=lambda path,value: terminal['audits'].append(copy.deepcopy(value))
    terminal_sampler=types.SimpleNamespace(observed_guard=terminal_observed_guard,
        actual_mm_stat=terminal_stat,cleared_owned_row=sampler.cleared_owned_row,
        same_owned_stat=sampler.same_owned_stat)
    terminal_base=types.SimpleNamespace(ident=terminal_ident,json_ident=copy.deepcopy)
    for kind in ('reference','guest'):
        entry=setup_terminal(kind)
        guard=ns.r5_terminal_guard(terminal_sampler,None,terminal_base,None,TerminalPath('out'),terminal_plan)
        guard.check()
        check(terminal['checks']==2 and terminal['audits'][-1]['terminal_confirmed'])
        check(terminal['audits'][-1]['complete_original_check_repeated'])
        check(entry['actual_exec'] is None if kind=='reference' else entry['actual_exec']['executable']=='/fake/product')
        check(terminal['audits'][-1]['actual_exec_synthesized'] is False)
    for kind,fault in [('reference',k) for k in ('uid','cgroup','birth','ppid','pgid','cpus','initial_argv','initial_MM',
                                                  'fresh_uid','fresh_argv','fresh_MM','changed_FD','timeout','perf_role',
                                                  'foreign_exe','unrelated_error','late_proof')] + [
                                                      ('guest','product_no_exec'),('guest','initial_argv'),
                                                      ('guest','fresh_uid'),('guest','fresh_MM')]:
        setup_terminal(kind,fault)
        guard=ns.r5_terminal_guard(terminal_sampler,None,terminal_base,None,TerminalPath('out'),terminal_plan)
        rejects(guard.check)
        check(terminal['checks']==1) # Cannot bypass the original failed check.
finally:
    for key,value in terminal_saved.items(): setattr(ns,key,value)

print(f'pure Python R5 measurement binding: {checks} checks passed; no native/SSH execution')
