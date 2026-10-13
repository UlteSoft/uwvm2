#!/usr/bin/env python3
"""Pure Python source/recipe tests. No CPP/native/SSH/tool subprocesses."""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import tarfile
import types
import tempfile

HERE = Path(__file__).resolve().parent
checks = 0


def test(ok, reason):
    global checks
    if not ok:
        raise AssertionError(reason)
    checks += 1


def rejection(action, reason):
    try:
        action()
    except (RuntimeError, KeyError):
        test(True,reason)
    else:
        test(False,reason)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-archive',type=Path,required=True)
    p.add_argument('--actual-main-commands',type=Path)
    p.add_argument('--actual-runtime-commands',type=Path)
    p.add_argument('--actual-host-commands',type=Path)
    args = p.parse_args()
    source = HERE/'prepare_gc_nativeTLS_ABC.py'
    tree = ast.parse(source.read_bytes())
    test(not any(isinstance(n,ast.Name) and n.id in ('subprocess','Popen','exec_command') for n in ast.walk(tree)), 'Producer may not execute native tools')
    m = types.ModuleType('_ABC_source_only'); m.__file__ = str(source)
    exec(compile(source.read_bytes(),str(source),'exec'),m.__dict__)
    c = m.contract()
    test(c['source_only'] is True and len(c['records']) == 32,'Frozen source contract')
    test(args.source_archive.stat().st_size == c['canonical_source_archive_bytes'] and
         m.sha(args.source_archive) == c['canonical_source_archive_sha256'],'Actual reviewed source archive')
    with tarfile.open(args.source_archive) as t:
        for name,row in c['records'].items():
            data = t.extractfile(name).read()
            test(len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256'], 'Original source pin: '+name)
        for repo in ('uwvm2','uwvm2-ros'):
            before = t.extractfile(repo+'/'+m.ENV_PATH).read()
            after = m.add_key(before)
            test({'bytes':len(after),'sha256':hashlib.sha256(after).hexdigest()} == c['derived_environment'][repo], 'Common source exact six-line derived ENV')
            test(after.replace(m.KEY_BLOCK,b'') == before,'Default OFF ENV projection')
            rejection(lambda:m.add_key(after),'Never insert key twice')
            rejection(lambda:m.add_key(before.replace(m.ANCHOR,b'')),'Missing key anchor')
            rejection(lambda:m.add_key(before+m.ANCHOR),'Ambiguous key anchor')
            for path,needle in (('src/uwvm2/uwvm/runtime/storage/gc_object.h',b'# include "gc_trace_metadata.h"'),
                                ('src/uwvm2/uwvm/runtime/storage/impl.h',b'# include "gc_trace_metadata.h"'),
                                ('src/uwvm2/uwvm/runtime/storage/impl.cppm',b'export import :gc_trace_metadata;'),
                                ('src/uwvm2/uwvm/runtime/storage/wasm_module.cppm',b'import :gc_trace_metadata;'),
                                ('src/uwvm2/uwvm/runtime/storage/gc_trace_metadata.cppm',b'export module uwvm2.uwvm.runtime.storage:gc_trace_metadata;')):
                test(needle in t.extractfile(repo+'/'+path).read(),'Required metadata dependency/import is physically in pinned source')
    for profile in m.PROFILES:
        macros = '#define UWVM_USE_THREAD_LOCAL\n#define UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT 1\n'
        for flag in m.PROFILES[profile]:
            macros += '#define '+flag[2:].replace('=',' ')+'\n'
        test(m.profile_macros(macros,profile)['experiments'] == {v[2:].split('=')[0]:'1' for v in m.PROFILES[profile]}, 'Actual macro witness accepted')
        rejection(lambda:m.profile_macros(macros+'#define UWVM_EXPERIMENTAL_UNKNOWN 1\n',profile),'Extra experiment rejected')
        rejection(lambda:m.profile_macros(macros.replace('#define UWVM_USE_THREAD_LOCAL\n',''),profile),'Missing TLS rejected')
        rejection(lambda:m.profile_macros(macros+'#define UWVM2_BUILD_SOURCE_ID "fake"\n',profile),'Embedded source ID override rejected')
        rejection(lambda:m.profile_macros(macros+'#define UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT 0\n',profile),'Duplicate/changed support rejected')
        if profile != 'A':
            rejection(lambda:m.profile_macros(macros.replace(' 1\n',' 0\n'),profile),'Experiment 0 is not profile 1')
            rejection(lambda:m.profile_macros(macros.replace(' 1\n',' 2\n'),profile),'Experiment 2 is not profile 1')
        for repo in ('uwvm2','uwvm2-ros'):
            commands = m.build_commands(repo,profile); rows = dict(commands)
            test(len(commands) == len(rows) == 13,'Build cardinality')
            prefix = m.prefix(repo,profile)
            for name,relative in m.UNITS.items():
                test(rows[name+'-compile'][:len(prefix)] == prefix and rows[name+'-macros'][:len(prefix)] == prefix,'Every actual TU uses one matching complete prefix')
                test(rows[name+'-compile'][rows[name+'-compile'].index('-c')+1] == str(m.COMMON/repo/relative),'Correct production TU')
                test(str(m.BUILD/repo/profile/(name+'.o')) in rows['main-link'],'Link only fresh same-profile objects')
            test(rows['main-link'][:len(prefix)] == prefix and not any('reused' in v or 'runtime1' in v for v in rows['main-link']),'Same link profile and no old runtime binding')
            fixtures = {f'{family}-{phase}-{n}':{'wasm':{'path':'/fixture/'+f'{family}-{phase}-{n}'+'.wasm'}}
                        for family in ('mutable-struct','reference-cycle','numeric-array','reference-array')
                        for phase in ('allocate','mutate') for n in (1000000,2000000)}
            cold = m.cold_commands(repo,profile,fixtures)
            test(len(cold) == 34 and len({row[0] for row in cold}) == 34,'Actual cold cardinality')
            for label,argv in cold[1:-1]:
                test((argv[6:10] == ['-Rcc','jit','-Rcm','full'] and '-Raot' not in argv) if repo == 'uwvm2' else
                     (argv[6:7] == ['-Raot'] and '-Rcc' not in argv and '-Rcm' not in argv),
                     'Cold selects actual ordinary full mode or ROS AOT mode')
                test(argv[:3] == ['env','-u','UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'] and
                     argv[argv.index('-Rclog')+1] == 'err' and argv[argv.index('-Rllvm-cache-path')+1] == 'disable' and
                     argv[5] == str(m.BUILD/repo/profile/'main') and argv[-1] == fixtures[label.rsplit('-',1)[0]]['wasm']['path'],
                     'Cold uses fresh product, same fixture, managed metrics and actual capture-unset contract')
    # Model only accumulated JSON receipts; no process/guest is ever created.
    with tempfile.TemporaryDirectory(prefix='uwvm-ABC-python-only-') as temporary:
        root = Path(temporary)
        original_build, original_evidence = m.BUILD, m.EVIDENCE
        m.BUILD, m.EVIDENCE = root/'build', root/'evidence'
        directory = m.BUILD/'uwvm2/A'; directory.mkdir(parents=True)
        evidence = m.EVIDENCE/'uwvm2/A/build'; evidence.mkdir(parents=True)
        commands = m.build_commands('uwvm2','A')
        (directory/'build-commands.json').write_text(json.dumps(commands))
        argv = dict(commands)['runtime-compile']
        log = evidence/'runtime-compile.log'; log.write_text('synthetic parser test, not actual compile\n')
        row = {'label':'runtime-compile','passed':True,'returncode':0,'argv':argv,
               'argv_sha256':hashlib.sha256(json.dumps(argv).encode()).hexdigest(),
               'command_file_sha256':m.sha(directory/'build-commands.json'),
               'memory_max_bytes':64<<30,'swap_max_bytes':0,'init':{'pid':12345},
               'remaining_roster':[12345],'retirement':[{'pidfd_readable':True}],
               'memory_events_before':{'oom':0,'oom_kill':0,'oom_group_kill':0},
               'memory_events_after':{'oom':0,'oom_kill':0,'oom_group_kill':0},'log_sha256':m.sha(log)}
        receipt = evidence/'receipts.json'; receipt.write_text(json.dumps([row]))
        proof = m.stage('uwvm2','A','runtime-compile')
        receipt.write_text(json.dumps([row,{'label':'a-later-unrelated-stage'}]))
        test(m.stage('uwvm2','A','runtime-compile') == proof,'Appending keeper receipts does not invalidate selected original proof')
        row['returncode'] = 1; receipt.write_text(json.dumps([row]))
        rejection(lambda:m.stage('uwvm2','A','runtime-compile'),'Changed original selected receipt fails')
        row['returncode'] = 0; receipt.write_text(json.dumps([row,row]))
        rejection(lambda:m.stage('uwvm2','A','runtime-compile'),'Duplicate selected receipt fails')
        for name in ('main','consumer-host-link.rsp','runtime.o','host-api.o','main.o'):
            file = directory/name; file.write_text('prior artifact')
            rejection(lambda:m.fresh_outputs(directory),'Pre-existing fresh output rejected '+name)
            file.unlink()
        original_disk = m.shutil.disk_usage
        m.shutil.disk_usage = lambda path: types.SimpleNamespace(free=15<<30)
        rejection(lambda:m.fresh_outputs(directory),'Compilation floor16GiB rejects15')
        m.shutil.disk_usage = lambda path: types.SimpleNamespace(free=16<<30)
        m.fresh_outputs(directory); test(True,'Compilation floor16GiB accepts exact16 model')
        m.shutil.disk_usage = original_disk
        m.BUILD, m.EVIDENCE = original_build, original_evidence
    test(set(m.LINK_LIBRARIES) == {'libssl.a','libcrypto.a','libz.so.1.3.2','libzstd.so.1.5.7'},'Explicit actual link dependencies include static OpenSSL/compression')
    # Actual old successful ordinary prefixes are optional input data, never run.
    actual = ((args.actual_main_commands,'main-compile'),(args.actual_runtime_commands,'runtime-compile'),(args.actual_host_commands,'host-api-compile'))
    for path,label in actual:
        if path is not None:
            raw = dict(json.loads(path.read_text()))[label]
            old = raw[:raw.index('-MD')]
            expected = [v.replace(str(m.BASE/'uwvm2'),str(m.COMMON/'uwvm2')) for v in old]
            prepared = m.prefix('uwvm2','A')
            test([prepared[0],*prepared[3:]] == expected,'Actual R5 successful prefix preserved except env capture-unset/common-source path')
    a = {'files':[{'path':'src/a','sha256':'a'},{'path':'third-parties/b','sha256':'b'}]}
    b = {'files':[{'path':'src/a','sha256':'c'},{'path':'third-parties/b','sha256':'b'},{'path':'src/new','sha256':'n'}]}
    test(m.changed(a,b) == {'src/a':{'before':'a','after':'c'},'src/new':{'before':None,'after':'n'}},'Full source/vendor delta includes added paths')
    print('PASS pure Python ABC recipe/source checks',checks,'native_executed=false')


if __name__ == '__main__':
    main()
