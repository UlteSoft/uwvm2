#!/usr/bin/env python3
"""Prepare same-source A/B/C collectors using the existing keeper supervisor.

This file never launches a compiler, VM, profiler or SSH. `prepare` creates
private source/build inputs; subsequent phases are commands for the keeper's
already reviewed resource/ownership supervisor. All objects are fresh.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import sys
import tempfile
import types

HERE = Path(__file__).resolve().parent
B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
T = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
G = B / 'llvm-native-b23-ros9-r2/build'
V = B / 'candidates/fused-inline-20261001-r5/uwvm2-ros/third-parties/llvm/llvm/include'
BASE = B / 'candidates/debug-joint-r5-full-current-cold-20261002-r1'
ORIGINAL = B / 'candidates/r5-nativeTLS-combined-gc-membership-v2-20261002-r1'
COMMON = B / 'candidates/gc-nativeTLS-ABC-common-20261002-r1'
BUILD = B / 'builds/gc-nativeTLS-ABC-20261002-r1'
EVIDENCE = B / 'evidence/gc-nativeTLS-ABC-20261002-r1'
CONTRACT = HERE / 'gc_nativeTLS_ABC_source_contract.json'
CONTRACT_SHA = '011483d59edede21051c56f66975ad66abdfaf675037b8dd4d6defbb30e398b8'
ORIGINAL_RSP_SHA = '08974aa924283cd7912e7ee904d29da91669d3f87df3455a54f7ce73d640930a'
MAPPED_RSP_SHA = 'b3ad1de4c162c22476fb8cbb974c52d23818bfd1f30c17328229c52449224287'
STATIC_REFERENCE_SHA = 'ef76117d6aeb343e82ee324f12c15d833cdf678cf0f99ee79e1779b37cc80655'
COLD_RECIPE_SHA = '2afc5478b9b0d7d8bd94d29d92ea07e33d4f8ccf7de36104606f90d5453d23aa'
LD = str(T/'lib') + ':' + str(T/'lib/x86_64-unknown-linux-gnu') + ':' + str(B.parent/'deps/usr/lib/x86_64-linux-gnu')
ENV_PATH = 'src/uwvm2/runtime/llvm_jit_cache/environment.h'
GC_PATH = 'src/uwvm2/uwvm/runtime/storage/gc_object.h'
PROFILES = {'A': (), 'B': ('-DUWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP=1',),
            'C': ('-DUWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1',)}
UNITS = {'runtime': 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp',
         'main': 'src/uwvm2/uwvm/main.default.cpp',
         'host-api': 'src/uwvm2/uwvm/host_api.default.cpp'}
ANCHOR = b'#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1\n'
LINK_LIBRARIES = {
    'libssl.a': {'bytes':2171484,'sha256':'67bd905e6e62d2b3410fa28206ea0020b00a53e154926d1fd72e914fb3f6ff8f'},
    'libcrypto.a': {'bytes':12391256,'sha256':'225c244fbc3bfb7da904aafea00c53fc3c8b78303f20fd92faec8586992665a7'},
    'libz.so.1.3.2': {'bytes':121328,'sha256':'032754aa5f865b32034dbc2e6b70153f29f38cf182dc41fa36aa469143fa53ca'},
    'libzstd.so.1.5.7': {'bytes':821240,'sha256':'afac1dce1f655e102000a1ad60c1b7be4f74772b6514105130a77dae71303c93'},
}
KEY_BLOCK = b'''#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
        // Host-owned type-layout/tracing profiles differ even when an embedder
        // deliberately reuses its source ID; never share native cache context.
        // This identity does not authorize mixing differently built C++ objects.
        details::append_cache_key_value(out, u8"gc-precise-trace-metadata", u8"owned-canonical-reference-plan-v1");
#endif
'''


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path):
    path = Path(path).resolve(strict=True)
    return {'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha(path)}


def value(path):
    return {k: pin(path)[k] for k in ('bytes', 'sha256')}


def atomic(path, data):
    path = Path(path)
    data = data.encode() if isinstance(data, str) else data
    fd, temporary = tempfile.mkstemp(prefix='.'+path.name+'-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def save(path, data):
    atomic(path, json.dumps(data, indent=2, allow_nan=False)+'\n')


def contract():
    require(sha(CONTRACT) == CONTRACT_SHA, 'Frozen source contract changed')
    c = json.loads(CONTRACT.read_text())
    require(c['schema'] == 'uwvm-gc-nativetls-ABC-source-contract-v1' and len(c['records']) == 32,
            'Different source contract')
    return c


def fingerprint(source):
    # Exact tools/ci canonical algorithm. Macro identity is separately recorded.
    entries = []
    for directory in ('src', 'third-parties'):
        root = (Path(source)/directory).resolve(strict=True)
        require(root.is_dir(), 'Missing source root')
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._'):
                entries.append({'path': (Path(directory)/path.relative_to(root)).as_posix(), 'sha256': sha(path)})
    require(bool(entries), 'Empty source fingerprint')
    data = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    return {'source_id': 'sha256:'+hashlib.sha256(data).hexdigest(), 'files': entries}


def changed(a, b):
    first = {v['path']: v['sha256'] for v in a['files']}
    second = {v['path']: v['sha256'] for v in b['files']}
    return {p: {'before': first.get(p), 'after': second.get(p)} for p in sorted(first.keys() | second.keys())
            if first.get(p) != second.get(p)}


def add_key(before):
    require(before.count(ANCHOR) == 1 and KEY_BLOCK not in before,
            'Precise key anchor already changed/ambiguous')
    after = before.replace(ANCHOR, KEY_BLOCK+ANCHOR)
    require(after.count(KEY_BLOCK) == 1 and after.replace(KEY_BLOCK, b'') == before,
            'Default-OFF text projection changed')
    return after


def original_source(c, repo):
    require(sha(BASE/'source-manifest.json') == c['root_base_manifest_sha256'], 'R5 root source manifest changed')
    root_records = json.loads((BASE/'source-manifest.json').read_text())['records']
    require(len(root_records) == 194, 'Incomplete root R5 source pins')
    # Test-only corrections outside src/vendors do not alter production identity.
    for name, expected in root_records.items():
        if name.startswith(repo+'/src/') or name.startswith(repo+'/third-parties/'):
            require(value(BASE/name) == {'bytes':expected['bytes'],'sha256':expected['sha256']}, 'Original root production pin changed: '+name)
    require(sha(ORIGINAL/'gc-source-derivation.json') == c['original_derivation_sha256'], 'Original 13 derivation changed')
    for name, expected in c['records'].items():
        if name.startswith(repo+'/'):
            require(value(ORIGINAL/name) == expected, 'Original frozen payload changed: '+name)
    before, after = fingerprint(BASE/repo), fingerprint(ORIGINAL/repo)
    differences = changed(before, after)
    production = {name[len(repo)+1:]: row for name, row in c['records'].items()
                  if name.startswith(repo+'/src/')}
    require(len(production) == 13 and set(differences) == set(production), 'Unreviewed source/vendor delta outside 13 paths')
    for path, expected in production.items():
        old = c['old_production_pins'][repo+'/'+path]
        require(differences[path] == {'before': old['sha256'], 'after': expected['sha256']},
                'Original reviewed before/after mismatch: '+path)
    for row in c['runtime_preserved']:
        if row['repository'] == repo:
            require(value(ORIGINAL/repo/row['path']) == {'bytes': row['bytes'], 'sha256': row['sha256']},
                    'R5 runtime/API owner source changed')
    return before, after


def prefix(repo, profile):
    source = COMMON/repo
    return ['env', '-u', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', 'LD_LIBRARY_PATH='+LD,
            str(T/'bin/clang++'), '-std=c++26', '-stdlib=libc++', '-O3', '-g0', '-fno-rtti',
            '-fasynchronous-unwind-tables', '-Wno-undefined-inline', '-pthread',
            '-DUWVM=2', '-DUWVM_USE_LLVM_JIT', '-DUWVM_USE_UWVM_INT', '-DUWVM_USE_THREAD_LOCAL',
            '-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
            '-I'+str(G/'include'), '-I'+str(V), '-I'+str(source/'src'),
            '-I'+str(source/'third-parties/fast_io/include'), '-I'+str(source/'third-parties/bizwen/include'),
            '-I'+str(source/'third-parties/boost_unordered/include'),
            '-I'+str(B.parent/'deps/usr/include'), '-I'+str(B.parent/'deps/usr/include/x86_64-linux-gnu'),
            '-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519', '-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1',
            *PROFILES[profile]]


def profile_macros(text, profile):
    macros = {}
    for line in text.splitlines():
        match = re.fullmatch(r'#define (\w+)(?: (.*))?', line)
        if match:
            require(match[1] not in macros, 'Duplicate preprocessor macro witness')
            macros[match[1]] = match[2] or ''
    require('UWVM_USE_THREAD_LOCAL' in macros, 'Actual TU lacks native TLS')
    observed = {k: v for k, v in macros.items() if k.startswith('UWVM_EXPERIMENTAL_')}
    wanted = {v.split('=', 1)[0][2:]: '1' for v in PROFILES[profile]}
    require(observed == wanted, 'Actual TU experiment macro vector differs')
    require(macros.get('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT') == '1', 'Capture support compilation differs')
    require('UWVM2_BUILD_SOURCE_ID' not in macros, 'One-TU embedded source ID override')
    return {'nativeTLS_defined': True, 'experiments': observed, 'capture_support': '1',
            'source_id_embedded': False, 'signed_cache_qualification': False}


def fresh_outputs(directory):
    directory = Path(directory)
    paths = [directory/'main', directory/'consumer-host-link.rsp', directory/'closure-before.json']
    paths += [directory/(name+suffix) for name in UNITS for suffix in ('.o','.d','.macros','.artifact-bind.json')]
    require(not any(p.exists() or p.is_symlink() for p in paths), 'Pre-existing product/object/MD/RSP/proof would violate fresh build')
    require(shutil.disk_usage(directory).free >= 16<<30, 'Fresh compilation disk headroom below16GiB')


def stable_receipt(row, receipt_file):
    # The keeper appends to receipts.json between stages; authenticate the selected
    # original row instead of treating a growing whole-file hash as immutable.
    raw = json.dumps(row, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()
    return {'receipt_file':str(Path(receipt_file).resolve(strict=True)),
            'selected_row_sha256':hashlib.sha256(raw).hexdigest(), 'selected_row':row}


def build_commands(repo, profile):
    d = BUILD/repo/profile
    p = prefix(repo, profile)
    helper = str(BUILD/'closure.py')
    call = ['python3', helper, '--repo', repo, '--profile', profile]
    result = [['input-before', call+['before']]]
    for name, source in UNITS.items():
        result += [[name+'-macros', p+['-dM', '-E', str(COMMON/repo/source), '-o', str(d/(name+'.macros'))]],
                   [name+'-compile', p+['-MD', '-MF', str(d/(name+'.d')), '-c', str(COMMON/repo/source), '-o', str(d/(name+'.o'))]],
                   [name+'-bind', call+['bind', name]]]
    libs = B.parent/'deps/usr/lib/x86_64-linux-gnu'
    result += [['main-link', p+[str(d/(name+'.o')) for name in UNITS]+['@'+str(d/'consumer-host-link.rsp'),
                      *[str(libs/n) for n in ('libssl.a','libcrypto.a','libz.so.1.3.2','libzstd.so.1.5.7')],
                      '-ldl', '-pthread', '-o', str(d/'main')]],
               ['main-executable-bind', call+['executable']], ['input-after', call+['after']]]
    require(len(result) == 13 and len({r[0] for r in result}) == 13, 'Build command cardinality')
    return result


def cold_recipe():
    source = HERE/'prepare_general_gc_R5_nativeTLS_long_cold.py'
    require(sha(source) == COLD_RECIPE_SHA, 'Immutable long cold parser/oracle changed')
    module = types.ModuleType('_immutable_general_gc_oracle')
    module.__file__ = str(source)
    exec(compile(source.read_bytes(), str(source), 'exec'), module.__dict__)
    return module


def cold_commands(repo, profile, fixtures):
    d = BUILD/repo/profile
    call = ['python3', str(BUILD/'closure.py'), '--repo', repo, '--profile', profile]
    result = [['input-before', call+['cold-before']]]
    for name in sorted(fixtures):
        for policy in ('instruction', 'unwind'):
            result.append([name+'-'+policy,
                ['env', '-u', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', 'LD_LIBRARY_PATH='+LD, 'RAYON_NUM_THREADS=1',
                 str(d/'main'), *(['-Rcc', 'jit', '-Rcm', 'full'] if repo == 'uwvm2' else ['-Raot']), '-Rllvm-full-policy', 'pb-o3',
                 '-Rllvm-call-stack', policy, '-Rllvm-exception-dispatch', 'auto', '-Rllvm-cache-path', 'disable',
                 '-Rct', '0', '-WFE-gc', '--wasm-feature-enable-reference-types',
                 '--wasm-feature-enable-function-references', '--log-verbose', '-Rclog', 'err', '--run',
                 fixtures[name]['wasm']['path']]])
    result.append(['input-after', call+['cold-after']])
    require(len(fixtures) == 16 and len(result) == 34, 'Cold command cardinality')
    return result


def prepare(reference):
    require(not COMMON.exists() and not BUILD.exists(), 'Never overwrite a prior common source/build')
    c = contract()
    require(sha(reference) == STATIC_REFERENCE_SHA, 'Actual static/DSO reference changed')
    fixtures = cold_recipe().oracle_fixtures()  # Same bytes; no old product PASS is imported.
    references = {repo: original_source(c, repo) for repo in ('uwvm2','uwvm2-ros')}
    BUILD.mkdir(parents=True)
    atomic(BUILD/'closure.py', Path(__file__).read_bytes())
    atomic(BUILD/CONTRACT.name, CONTRACT.read_bytes())
    atomic(BUILD/'prepare_general_gc_R5_nativeTLS_long_cold.py', (HERE/'prepare_general_gc_R5_nativeTLS_long_cold.py').read_bytes())
    atomic(BUILD/'static-library-reference.json', Path(reference).read_bytes())
    sources = {}
    for repo in references:
        source = COMMON/repo
        source.mkdir(parents=True)
        # Private hardlinks avoid another LLVM source copy. ENV is replaced atomically,
        # never written in place; original reviewed source bytes remain unchanged.
        for name in ('src','third-parties','tools','test','benchmark'):
            shutil.copytree(ORIGINAL/repo/name, source/name, copy_function=os.link, symlinks=True)
        before_env = (source/ENV_PATH).read_bytes()
        atomic(source/ENV_PATH, add_key(before_env))
        require(value(source/ENV_PATH) == c['derived_environment'][repo], 'Derived common ENV differs from exact six-line key')
        final = fingerprint(source)
        require(changed(references[repo][1], final) == {ENV_PATH: {'before': c['records'][repo+'/'+ENV_PATH]['sha256'],
                                                               'after': c['derived_environment'][repo]['sha256']}},
                'Common source changed outside approved ENV key')
        require(fingerprint(ORIGINAL/repo) == references[repo][1], 'Original source modified by private preparation')
        sources[repo] = {'baseline': references[repo][0], 'reviewed_13': references[repo][1], 'common': final,
                         'default_off_ENV_projection_equal_reviewed13': (source/ENV_PATH).read_bytes().replace(KEY_BLOCK,b'') == before_env}
        save(BUILD/(repo+'-source-derivation.json'), sources[repo])
        for profile in PROFILES:
            d = BUILD/repo/profile; d.mkdir(parents=True)
            save(d/'build-commands.json', build_commands(repo, profile))
            save(d/'cold-commands.json', cold_commands(repo, profile, fixtures))
    save(BUILD/'plan.json', {'schema':'uwvm-gc-nativeTLS-ABC-build-plan-v1', 'source_only_preparation':True,
         'source_contract':pin(CONTRACT), 'helper':pin(BUILD/'closure.py'), 'oracle_helper':pin(BUILD/'prepare_general_gc_R5_nativeTLS_long_cold.py'),
         'common_source':str(COMMON), 'source_ids':{r:v['common']['source_id'] for r,v in sources.items()},
         'profiles':{k:list(v) for k,v in PROFILES.items()}, 'nativeTLS_every_TU':True, 'all_RT_main_host_objects_fresh':True,
         'actual_builds_passed':False, 'actual_cold_passed':False, 'signed_cache_qualified':False,
         'fixtures':fixtures, 'static_reference':pin(BUILD/'static-library-reference.json'),
         'formal_acceptance':False, 'temperature_policy':'observation_only',
         'measurement_boundaries':'Internal Wasm excludes startup/JIT but includes full workload; wait4 whole original process separate; HW whole guest counts and VTune separate.'})
    print('PREPARED same-source three profiles; actual compilation/cold pending', str(BUILD))


def snapshot(repo):
    c = contract(); source = COMMON/repo
    expected = json.loads((BUILD/(repo+'-source-derivation.json')).read_text())['common']
    require(fingerprint(source) == expected, 'Actual common src/vendor tree changed')
    for key, row in c['records'].items():
        if key.startswith(repo+'/'):
            path = key[len(repo)+1:]
            wanted = c['derived_environment'][repo] if path == ENV_PATH else row
            require(value(source/path) == wanted, 'Common source/test pin changed: '+path)
    inputs = {}
    include_roots = [Path(v[2:]).resolve(strict=True) for v in prefix(repo,'A') if v.startswith('-I')]
    for root in (source/'src', source/'third-parties', *include_roots,
                 T/'include', T/'lib/clang', Path('/usr/include')):
        root = Path(root).resolve(strict=True)
        for directory, dirs, files in os.walk(root, followlinks=False):
            for name in files:
                p = Path(directory)/name
                if p.is_file():
                    item = pin(p); inputs[item['path']] = {k:item[k] for k in ('bytes','sha256')}
    tools = {pin(p)['path']:value(p) for p in (T/'bin/clang++', T/'bin/ld.lld', T/'bin/llvm-readobj',
                  T/'bin/llvm-objdump', Path(sys.executable), Path('/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'))}
    reference = BUILD/'static-library-reference.json'
    require(sha(reference) == STATIC_REFERENCE_SHA, 'Static/DSO reference changed')
    libraries = {}
    for row in json.loads(reference.read_text())['libraries']:
        item = pin(row['resolved']); require(item['bytes'] == row['bytes'] and item['sha256'] == row['sha256'], 'Library/DSO changed')
        libraries[item['path']] = {k:item[k] for k in ('bytes','sha256')}
    for name, expected in LINK_LIBRARIES.items():
        p = B.parent/'deps/usr/lib/x86_64-linux-gnu'/name
        require(value(p) == expected, 'Actual linked OpenSSL/compression library changed: '+name)
        libraries[pin(p)['path']] = expected
    raw = G/'consumer-link.rsp'; require(sha(raw) == ORIGINAL_RSP_SHA, 'Actual LLVM consumer RSP changed')
    mapped = raw.read_text().replace('/work/', '/home/macromodel/Documents/uwvm3-implementation/')
    require(hashlib.sha256(mapped.encode()).hexdigest() == MAPPED_RSP_SHA, 'Mapped consumer RSP changed')
    tokens = shlex.split(mapped)
    archives = [Path(v) for v in tokens if v.endswith('.a')]
    require(len(archives) == 63 and not any(v.startswith('@') or v.startswith('-DUWVM') or v.startswith('-U') or v.startswith('-B') for v in tokens),
            'Response nested/profile override or incomplete LLVM archive closure')
    for p in archives:
        item = pin(p); libraries[item['path']] = {k:item[k] for k in ('bytes','sha256')}
    site = pin(T/'include/x86_64-unknown-linux-gnu/c++/v1/__config_site')
    require(site['path'] in inputs, 'Actual target config_site missing from before inventory')
    return {'source_fingerprint':expected, 'inputs':inputs, 'tools':tools, 'libraries':libraries,
            'original_response':pin(raw), 'mapped_response':{'bytes':len(mapped.encode()),'sha256':MAPPED_RSP_SHA},
            'target_config_site':site, 'source_contract':pin(CONTRACT), 'helper':pin(Path(__file__)),
            'loaded_DSO_maps_qualified':False}, mapped


def receipts_prefix_snapshot(repo,profile,kind):
    d = BUILD/repo/profile
    source = EVIDENCE/repo/profile/kind/'receipts.json'
    target = d/(kind+'-receipts-before-final-helper-return.json')
    atomic(target,source.read_bytes())
    return {'snapshot':pin(target),'includes_own_final_helper_receipt':False,
            'final_complete_supervisor_receipts_qualified':False,
            'required_final_raw_receipts_path':str(source)}


def stage(repo, profile, label, *, cold=False):
    d = BUILD/repo/profile; e = EVIDENCE/repo/profile/('cold' if cold else 'build')
    command_file = d/('cold-commands.json' if cold else 'build-commands.json')
    commands = dict(json.loads(command_file.read_text()))
    rows = json.loads((e/'receipts.json').read_text())
    selected = [r for r in rows if r['label'] == label]
    require(len(selected) == 1, 'Missing/duplicate actual stage receipt: '+label)
    row = selected[0]; argv = commands[label]
    require(row['passed'] is True and row['returncode'] == 0 and row['argv'] == argv and
            row['argv_sha256'] == hashlib.sha256(json.dumps(argv).encode()).hexdigest() and
            row['command_file_sha256'] == sha(command_file), 'Actual command failed/changed: '+label)
    require(row['memory_max_bytes'] == 64<<30 and row['swap_max_bytes'] == 0 and
            row['remaining_roster'] == [row['init']['pid']] and row['retirement'] and
            all(v['pidfd_readable'] is True for v in row['retirement']), 'Actual bounded stage not retired')
    require(all(row['memory_events_before'][k] == row['memory_events_after'][k] for k in ('oom','oom_kill','oom_group_kill')), 'Actual stage OOM')
    require(sha(e/(label+'.log')) == row['log_sha256'], 'Actual stage log changed')
    if cold:
        require(row['Popen_root_pidfd_retirement']['pidfd_readable'] is True and
                row['Popen_root_pidfd_retirement']['actual_reaped_returncode'] == 0,
                'Actual guest root not reaped')
        require(row['actual_stopped_child_environment']['capture_presence'] == {'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT':False},
                'Actual guest capture environment not cleared')
    return {'actual_argv':argv, 'argv_sha256':row['argv_sha256'], 'returncode':0,
            'log_sha256':row['log_sha256'], 'receipt':stable_receipt(row,e/'receipts.json'), 'log':pin(e/(label+'.log'))}


def bind(repo, profile, name):
    require(name in UNITS, 'Unknown build TU')
    d = BUILD/repo/profile; before = json.loads((d/'closure-before.json').read_text())
    require(json.loads((d/'build-commands.json').read_text()) == build_commands(repo,profile), 'Build argv changed')
    require(stage(repo,profile,name+'-macros')['actual_argv'][:len(prefix(repo,profile))] == prefix(repo,profile), 'Macro TU prefix mismatch')
    macros = profile_macros((d/(name+'.macros')).read_text(),profile)
    proof = stage(repo,profile,name+'-compile')
    dependencies = {}; spellings = {}
    body = (d/(name+'.d')).read_text().replace('\\\n',' ').split(':',1)[1]
    for token in shlex.split(body):
        item = pin(token); expected = before['inputs'].get(item['path'])
        require(expected is not None and expected == {k:item[k] for k in ('bytes','sha256')}, 'MD input was not pinned before compilation: '+item['path'])
        dependencies[item['path']] = expected
        spellings[token] = item['path']
    for path in (COMMON/repo/GC_PATH, COMMON/repo/'src/uwvm2/uwvm/runtime/storage/gc_trace_metadata.h'):
        require(pin(path)['path'] in dependencies, 'Actual TU MD omitted required collector/type-layout header')
    result = {'actual_dependencies':dependencies, 'MD_spelling_to_resolved_path':spellings, 'dependency_file':pin(d/(name+'.d')),
              'output':pin(d/(name+'.o')), 'actual_compile_proof':proof,
              'macro_file':pin(d/(name+'.macros')), 'actual_macro_profile':macros,
              'actual_macro_proof':stage(repo,profile,name+'-macros'),
              'objects_fresh':True, 'source_id':before['source_fingerprint']['source_id'],
              'public_bridge_ABI_changed':False, 'signed_cache_qualified':False}
    save(d/(name+'.artifact-bind.json'),result)
    print('BOUND fresh matching TU', repo,profile,name,result['output']['sha256'])


def artifacts(repo,profile):
    d = BUILD/repo/profile; result = {}
    for name in UNITS:
        row = json.loads((d/(name+'.artifact-bind.json')).read_text())
        require(pin(d/(name+'.o')) == row['output'] and pin(d/(name+'.d')) == row['dependency_file'] and
                pin(d/(name+'.macros')) == row['macro_file'], 'Actual object/MD/macro file changed')
        require(stage(repo,profile,name+'-compile') == row['actual_compile_proof'], 'Actual compile receipt changed')
        require(profile_macros((d/(name+'.macros')).read_text(),profile) == row['actual_macro_profile'] and
                stage(repo,profile,name+'-macros') == row['actual_macro_proof'], 'Actual macro profile/receipt changed')
        for path,expected in row['actual_dependencies'].items(): require(value(path) == expected, 'Actual MD dependency changed')
        result[name] = row
    return result


def run_phase(repo,profile,phase,name):
    d = BUILD/repo/profile
    require(repo in ('uwvm2','uwvm2-ros') and profile in PROFILES, 'Unknown repository/profile')
    if phase == 'before':
        fresh_outputs(d)
        require(json.loads((d/'build-commands.json').read_text()) == build_commands(repo,profile), 'Prepared commands changed')
        state,mapped = snapshot(repo)
        atomic(d/'consumer-host-link.rsp',mapped)
        state['commands_sha256'] = sha(d/'build-commands.json');state['compile_prefix'] = prefix(repo,profile)
        state['profile'] = profile;state['all_objects_fresh'] = True
        save(d/'source-before.json',state['source_fingerprint'])
        save(d/'closure-before.json',state)
        print('BOUND same-source fresh build before',repo,profile,state['source_fingerprint']['source_id'])
    elif phase == 'bind':
        bind(repo,profile,name)
    elif phase == 'executable':
        save(d/'main.executable-bind.json', {'components':artifacts(repo,profile),'output':pin(d/'main'),
             'actual_link_proof':stage(repo,profile,'main-link'), 'all_objects_fresh':True,
             'source_id':json.loads((d/'closure-before.json').read_text())['source_fingerprint']['source_id'],
             'profile':profile,'loaded_DSO_maps_qualified':False,'signed_cache_qualified':False})
    elif phase == 'after':
        before = json.loads((d/'closure-before.json').read_text()); state,mapped = snapshot(repo)
        expected = {k:v for k,v in before.items() if k not in ('commands_sha256','compile_prefix','profile','all_objects_fresh')}
        require(state == expected and sha(d/'build-commands.json') == before['commands_sha256'] and
                sha(d/'consumer-host-link.rsp') == MAPPED_RSP_SHA,'Build source/SDK/RSP/tools changed')
        ex = json.loads((d/'main.executable-bind.json').read_text())
        require(ex['components'] == artifacts(repo,profile) and ex['output'] == pin(d/'main') and
                ex['actual_link_proof'] == stage(repo,profile,'main-link'),'Final fresh product provenance changed')
        save(d/'source-after.json',state['source_fingerprint'])
        save(d/'closure-after.json', {'passed':True,'before':pin(d/'closure-before.json'), 'source_id':state['source_fingerprint']['source_id'],
              'product':pin(d/'main'),'executable_bind':pin(d/'main.executable-bind.json'), 'profile':profile,
              'source_dependencies_tools_libraries_before_after_equal':True,'all_objects_fresh':True,
              'signed_cache_qualified':False,'actual_cold_passed':False,'formal_acceptance':False,
              'receipt_prefix_snapshot':receipts_prefix_snapshot(repo,profile,'build')})
        print('PASS actual fresh build closure, cold/performance pending',repo,profile)
    elif phase in ('cold-before','cold-after'):
        build = json.loads((d/'closure-after.json').read_text())
        require(build['passed'] is True and pin(d/'main') == build['product'], 'Fresh build not closed')
        state,_ = snapshot(repo); before = json.loads((d/'closure-before.json').read_text())
        require(state == {k:v for k,v in before.items() if k not in ('commands_sha256','compile_prefix','profile','all_objects_fresh')},
                'Cold source/SDK/tools/DSOs differ from fresh build')
        fixtures = cold_recipe().oracle_fixtures()
        require(json.loads((d/'cold-commands.json').read_text()) == cold_commands(repo,profile,fixtures),'Cold argv/fixture changed')
        record = {'product':pin(d/'main'),'build_closure':pin(d/'closure-after.json'),'executable_bind':pin(d/'main.executable-bind.json'),
                  'state':state,'fixtures':fixtures,'cold_commands':pin(d/'cold-commands.json')}
        if phase == 'cold-before':
            require(not (d/'cold-before.json').exists(), 'Never replace cold before evidence')
            save(d/'cold-before.json',record)
        else:
            require(record == json.loads((d/'cold-before.json').read_text()),'Cold inputs/source/SDK changed')
            rows = []
            parser = cold_recipe()
            for label,argv in cold_commands(repo,profile,fixtures)[1:-1]:
                proof = stage(repo,profile,label,cold=True)
                definition = json.loads(Path(fixtures[label.rsplit('-',1)[0]]['manifest']['path']).read_text())
                parsed = parser.parse_log((EVIDENCE/repo/profile/'cold'/(label+'.log')).read_text(),
                                          definition['expected'],definition['phase'])
                rows.append({'label':label,'actual_proof':proof,'semantic':parsed})
            save(d/'cold-summary.json', {'passed':True,'source_id':state['source_fingerprint']['source_id'],
                  'product':pin(d/'main'),'profile':profile,'rows':rows,'actual_cold_rows':32,
                  'all_allocation_collector_qualified':all(r['semantic']['collector_qualified'] for r in rows if '-allocate-' in r['label']),
                  'all_mutation_field_lookup_qualified':all(r['semantic']['field_lookup_qualified'] for r in rows if '-mutate-' in r['label']),
                  'before_after_equal':True,'old_product_used_for_qualification':False,'formal_acceptance':False,
                  'receipt_prefix_snapshot':receipts_prefix_snapshot(repo,profile,'cold')})
            print('PASS actual source-bound 32 product cold rows; performance pending',repo,profile)
    else:
        raise RuntimeError('Unknown phase')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', choices=('uwvm2','uwvm2-ros'))
    p.add_argument('--profile', choices=tuple(PROFILES))
    p.add_argument('--static-reference', type=Path)
    p.add_argument('phase', choices=('prepare','before','bind','executable','after','cold-before','cold-after'))
    p.add_argument('unit',nargs='?')
    a = p.parse_args()
    if a.phase == 'prepare':
        require(a.static_reference is not None, 'Actual static-library reference path required')
        prepare(a.static_reference)
    else:
        require(a.repo is not None and a.profile is not None,'Repository/profile required')
        run_phase(a.repo,a.profile,a.phase,a.unit)


if __name__ == '__main__':
    main()
