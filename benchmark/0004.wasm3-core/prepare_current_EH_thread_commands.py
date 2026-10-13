#!/usr/bin/env python3
"""Fixed current Ordinary EH/thread commands; no tool or guest is executed here.

Only the already built R3c Ordinary CLI is admissible. ROS R3d failed; no ROS
command may be emitted until a separately reviewed R3e source/after binding is
added. The existing keeper supervisor and plain measurement protocols remain
unchanged. Generated commands are not evidence that a fixture was compiled.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
S = B/'candidates/debug-native-window-R3c-full-nativeTLS-20261002-r1/uwvm2'
D = B/'builds/debug-native-window-R3c-fresh-nativeTLS-20261002-r1-uwvm2'
OUT = B/'builds/current-R3c-EH-thread-commands-20261002-r2'
SID = 'sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d'
F = B/'evidence/gc-ring-quiet-host-r3b-20260928/fixtures'
WASM_TOOLS = B/'tools/wasm-tools-1.259.0-x86_64-linux/wasm-tools'
CAPTURE = 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'
CPUSET = '0,2,4,6,16-31'
PYC = S/'tools/debug/__pycache__/dap_adapter.cpython-314.pyc'
PYC_OLD = {'bytes':44474,'sha256':'0aaee44943b226d1a3cae27f2d48e222f089ab8f55d57d9607c03bf7c1f49f76'}
PYC_NEW = {'bytes':87853,'sha256':'c30f0a7dd6d0f0c46b34c52bc99e10c6f8ac9fc9afc13fea5e60824e203e539d'}
ADAPTER_SOURCE = {'bytes':66436,'sha256':'7dc31138531a11505dff77f3047c5e3daa70da5bea7e7d6623bbe80edf6f96da'}
CACHE_PROOF = B/'builds/debug-native-window-R3c-ordinary-cold-20261002-r5/cold-finalized-generated-cache-r1.json'
CACHE_PROOF_PIN = {'bytes':7341,'sha256':'9fdd63e1d9efd715a55bac7230ed4a1b84b6f6cc98321dda51c31c101cf0ff15'}
CACHE_FINALIZER = CACHE_PROOF.with_name('finalize-generated-cache-r1.py')
CACHE_FINALIZER_SHA = 'efb22049e28c6f1f9d3f7ee6fca3a97f8b06a85bafee3ed09c1ef5563895fca4'
EH = {
    'plain_normal': {'sha256':'03c56518a1d2ed9d0149791e3a241ddff773395e51bd48aeb67a9160d5bdba01', 'iterations':200000000, 'checksum':1231817216, 'catches':0},
    'eh_normal': {'sha256':'f2b9df696835bdd0f759885b77862e4a3de56b506a96f3edc41e54b20a63be8a', 'iterations':200000000, 'checksum':1231817216, 'catches':0},
    'eh_throws': {'sha256':'560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560', 'iterations':8000000, 'checksum':2464256, 'catches':500000},
}
TESTS = {
    'test/0017.runtime/wasm_thread_performance.cc': {'bytes':16360,'sha256':'5c0e6cde79ce924d184cd279031d93ab739c7d1cd814e589a1b1bd47786105a8'},
    'test/0017.runtime/wasm_parked_wait_notify_performance.cc': {'bytes':14031,'sha256':'240a8f0952a2b80e26fc977c5a332e91511a5d883198255e68ac4f47b5797f52'},
    'test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h': {'bytes':60110,'sha256':'0eccb457c66639a1bacb61144f1bccbd602f46452fa57944811f96eda3d8abf0'},
}

def require(ok, reason):
    if not ok: raise RuntimeError(reason)

def sha(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()

def content_pin(path):
    p=Path(path).resolve(strict=True)
    return {'bytes':p.stat().st_size,'sha256':sha(p)}

def pin(path):
    return {'path':str(Path(path).resolve(strict=True)),**content_pin(path)}

def atomic(path,data):
    fd,tmp=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as stream: stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(tmp,path)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)

def save(path,data):
    atomic(path,(json.dumps(data,indent=2,allow_nan=False)+'\n').encode())

def checked_parent_input(group,path,expected):
    if group=='inputs' and path==str(PYC):
        # Root-reviewed actual generated-cache proof binds this ONE old/new
        # path. No directory/extension-wide exception, and no source authority
        # comes from Python bytecode. The immutable old inventory is preserved.
        require(expected==PYC_OLD and content_pin(PYC)==PYC_NEW,
                'Exact old/new generated cache identity changed')
        require(content_pin(S/'tools/debug/dap_adapter.py')==ADAPTER_SOURCE and
                content_pin(CACHE_PROOF)==CACHE_PROOF_PIN and sha(CACHE_FINALIZER)==CACHE_FINALIZER_SHA,
                'Actual independently reviewed cache/source/finalizer proof changed')
        json.loads(CACHE_PROOF.read_bytes())  # Exact pinned receipt; no execution.
        return True
    require(content_pin(path)==expected,'Actual parent input/tool/library changed')
    return False

def parent_product():
    after=json.loads((D/'closure-after.json').read_bytes())
    before=json.loads((D/'closure-before.json').read_bytes())
    require(after['passed'] is True and after['repository']=='uwvm2' and after['source_id_external']==SID,
            'Only actual completed current R3c Ordinary build is supported')
    require(after['all_product_TUs_fresh'] is True and after['runtime_reused'] is False and
            after['source_tools_headers_SDK_MD_before_after_equal'] is True,
            'Fresh aligned complete product closure is required')
    require(after['build_before_sha256']==sha(D/'closure-before.json') and after['product_path']==str(D/'main') and
            after['product']==content_pin(D/'main') and after['main_link_returncode']==0,
            'Actual product or source closure changed')
    require(before['source_fingerprint']['source_id']==SID and
            json.loads((D/'source-before.json').read_bytes())==json.loads((D/'source-after.json').read_bytes())==before['source_fingerprint'],
            'Actual source-before/after differs')
    for name,row in after['components'].items():
        require(name in ('runtime','main','host-api') and row['same_task_or_historical_object_reused'] is False,
                'Unexpected or reused component')
        require(content_pin(D/(name+'.o'))==row['output'] and sha(D/(name+'.d'))==row['dependency_file_sha256'],
                'Actual component/MD changed')
        for path,expected in row['actual_dependencies'].items():
            require(content_pin(path)==expected,'Actual bound MD source/header changed')
    require(set(after['components'])=={'runtime','main','host-api'},'Incomplete actual complete product')
    commands=dict(json.loads((D/'commands.json').read_bytes()))
    prefix=commands['runtime-compile'][:commands['runtime-compile'].index('-MD')]
    require(before['shared_compile_prefix']==prefix and sha(D/'commands.json')==before['commands_sha256'],
            'Actual matching compiler prefix changed')
    require('-DUWVM_USE_THREAD_LOCAL' in prefix and not any(v.startswith('-DUWVM_EXPERIMENTAL_') or 'UWVM2_BUILD_SOURCE_ID' in v for v in prefix),
            'Current default nativeTLS compiler profile changed')
    for name in ('main','host-api'):
        require(commands[name+'-compile'][:commands[name+'-compile'].index('-MD')]==prefix,'Mixed actual TU prefix')
    link=commands['main-link'];require(link==after['main_link_argv'] and link[:len(prefix)]==prefix,'Actual link command changed')
    require(link[len(prefix):len(prefix)+3]==[str(D/(n+'.o')) for n in ('main','runtime','host-api')],
            'Unexpected actual three-object link order')
    tail=link[len(prefix)+3:-2]
    require(tail[0]=='@'+str(D/'consumer-host-link.rsp') and link[-2:]==['-o',str(D/'main')],
            'Actual link response/output changed')
    cache_exceptions=0
    for group in ('inputs','tools','libraries'):
        for path,expected in before[group].items():
            cache_exceptions+=checked_parent_input(group,path,expected)
    require(cache_exceptions==1,'Expected exactly one independently closed generated-cache delta')
    for group in ('archive_pins','OpenSSL_static_archive_pins','compression_DSO_pins'):
        for path,expected in before['sdk'][group].items():require(content_pin(path)==expected,'Actual SDK/link dependency changed')
    require(content_pin(D/'consumer-host-link.rsp')==before['sdk']['derived_response'],'Actual mapped RSP changed')
    return prefix,tail,{'after':pin(D/'closure-after.json'),'before':pin(D/'closure-before.json'),
           'product':pin(D/'main'),'runtime':pin(D/'runtime.o'),'host_api':pin(D/'host-api.o'),
           'response':pin(D/'consumer-host-link.rsp'),'source_id':SID,'loaded_DSO_maps_qualified':False,
           'entire_original_inventory_equal':False,
           'generated_cache_delta':{'path':str(PYC),'before':PYC_OLD,'after':PYC_NEW,
                 'actual_receipt':pin(CACHE_PROOF),'finalizer':pin(CACHE_FINALIZER),
                 'source':pin(S/'tools/debug/dap_adapter.py'),'source_authority_from_pyc':False}}

def lists(prefix,tail):
    loader=[v for v in prefix if v.startswith('LD_LIBRARY_PATH=')]
    require(len(loader)==1,'Exact actual loader assignment required')
    env=['env','-u',CAPTURE,loader[0],'RAYON_NUM_THREADS=1','UWVM_TEST_CPUSET='+CPUSET,'PYTHONDONTWRITEBYTECODE=1']
    eh=[]
    for name in EH:
        for trace in ('instruction','unwind'):
            for dispatch in ('auto','native-unwind'):
                args=[str(D/'main'),'-Rcc','jit','-Rcm','full','-Rllvm-full-policy','pb-o3',
                      '-Rllvm-call-stack',trace,'-Rllvm-exception-dispatch',dispatch,'-Rllvm-cache-path','disable','-Rct','0',
                      '-WFD-exceptions' if name=='plain_normal' else '-WFE-exceptions','--log-verbose','-Rclog','err','--run',str(F/(name+'.wasm'))]
                eh.append([name+'-'+trace+'-'+dispatch,env+args])
    compile_prefix=[prefix[0],'-u',CAPTURE,'UWVM_TEST_CPUSET='+CPUSET,'PYTHONDONTWRITEBYTECODE=1',*prefix[1:]]
    build=[]
    for name,relative in (('thread-timed','test/0017.runtime/wasm_thread_performance.cc'),
                          ('thread-qualifier','test/0017.runtime/wasm_thread_performance.cc'),
                          ('parked','test/0017.runtime/wasm_parked_wait_notify_performance.cc')):
        selected=compile_prefix+['-I'+str(S/'test/0013.uwvm_int/strict'),'-DUWVM2TEST_RUNNER_USE_LLVM_JIT']
        if name=='thread-qualifier':selected+=['-DUWVM_THREAD_BENCH_QUALIFY_CREATION']
        build.append([name+'-compile',selected+['-MD','-MF',str(OUT/(name+'.d')),'-c',str(S/relative),'-o',str(OUT/(name+'.o'))]])
        link=compile_prefix+[str(OUT/(name+'.o')),str(D/'runtime.o'),str(D/'host-api.o'),*tail]
        if name=='thread-qualifier':link+=['-Wl,--export-dynamic']
        build.append([name+'-link',link+['-o',str(OUT/name)]])
    cold=[]
    for trace in ('unwind','instruction'):
        dump=OUT/('qualifier-'+trace+'.wasm')
        cold.append(['thread-qualify-'+trace,env+[str(OUT/'thread-qualifier'),trace,'1','4',str(dump)]])
        cold.append(['thread-validate-'+trace,env+[str(WASM_TOOLS),'validate',str(dump)]])
    dump=OUT/'parked.wasm'
    cold += [['parked-dump',env+[str(OUT/'parked'),'--dump-wasm',str(dump)]],
             ['parked-validate',env+[str(WASM_TOOLS),'validate',str(dump)]]]
    timed=[]
    for trace in ('unwind','instruction'):
        timed.append(['thread-timing-'+trace,env+['taskset','-c','0',str(OUT/'thread-timed'),trace,'9','1024',str(OUT/('timed-'+trace+'.wasm'))]])
        timed.append(['parked-timing-'+trace,env+['taskset','-c','0,2',str(OUT/'parked'),trace]])
    return {'eh-cold':eh,'thread-build':build,'thread-cold':cold,'thread-plain':timed}

def prepare():
    require(not OUT.exists(),'Never overwrite earlier build/dump/evidence paths')
    prefix,tail,product=parent_product()
    tests={}
    for relative,expected in TESTS.items():
        require(content_pin(S/relative)==expected,'Actual fixed fixture source changed: '+relative)
        tests[relative]=pin(S/relative)
    fixtures={}
    for name,definition in EH.items():
        require(sha(F/(name+'.wasm'))==definition['sha256'],'Actual original EH bytes changed')
        fixtures[name]={'file':pin(F/(name+'.wasm')),**definition}
    tool=pin(WASM_TOOLS)
    require(os.statvfs(B).f_bavail*os.statvfs(B).f_frsize>=16<<30,'Existing compile 16GiB floor')
    commands=lists(prefix,tail)
    OUT.mkdir()
    for name,rows in commands.items():save(OUT/(name+'-commands.json'),rows)
    save(OUT/'plan.json',{'schema':'uwvm-current-R3c-EH-thread-command-plan-v1','repository':'uwvm2','ROS_supported':False,
         'product':product,'fixture_sources':tests,'EH_fixtures':fixtures,'wasm_tools':tool,'compile_prefix':prefix,
         'commands':{name:pin(OUT/(name+'-commands.json')) for name in commands},'source_helper':pin(__file__),
         'actual_fixture_compilation_passed':False,'actual_EH_cold_passed':False,'actual_thread_cold_passed':False,
         'requires_keeper_fixture_MD_object_link_before_after_binding':True,
         'requires_actual_creation_counter_and_dump_validation_before_timing':True,
         'requires_fresh_timed_dump_and_independent_SHA_validation_after':True,
         'requires_current_boot_CG_init_PIDFD_and_capture_unset_witness':True,
         'thread_multiTID_pure_counter_qualified':False,'formal_acceptance':False,
         'temperature_policy':'observation_only','signed_CLI_cache_hit_qualified':False,
         'ROI':'EH internal WASM excludes JIT/startup; thread warmed creation/work/join and genuine parked notify ROI; parent whole-process wait4 distinct',
         'timing_environment_note':'env wrapper is shell/functional syntax; existing plain adapter must move assignments into real Popen env and authenticate taskset-stripped original executable. Thread uses approved multiTID supervisor only.'})
    print('PREPARED fixed Ordinary commands only; actual fixture build/cold/performance pending',OUT)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase',choices=('prepare',))
    args=parser.parse_args();prepare()
