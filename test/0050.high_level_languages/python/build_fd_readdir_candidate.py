#!/usr/bin/env python3
"""Prepare/build a separate six-header candidate after a released Linux window.

This helper never edits an original source/product, builds LLVM, or runs a VM.
The default plan mode only emits its static contract. Preparation and compilation
are separate explicit stages, with new directories and retained failure receipts.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import shutil
import sys
import time

import linux_control


REPOS = ('uwvm2', 'uwvm2-stable', 'uwvm2-ros')
HEADERS = ('src/uwvm2/imported/wasi/wasip1/func/fd_readdir.h',
           'src/uwvm2/imported/wasi/wasip1/func/fd_readdir_wasm64.h')
BASELINES = {
    'uwvm2': ('sha256:c8f683fcbe9a56ab03c12a96b93e50ddc551fb1ede1a92ccce9e971dfb57fd80',
              '5953cb03d5a15805d26dfcf9edef5d9cb9da87b9f10c78ca828c161d8471474e'),
    'uwvm2-stable': ('sha256:04fbb20f066e9d74b5e945593f2c8d20f17d6b29be07a413dd055c6a44b183f4',
                     'e0c9461ea749788d3d97495dac7c575853ab5e849c3956a12019777570e7fc0b'),
    'uwvm2-ros': ('sha256:e5729fb928ab7356300fd455bb58996877472cb02fb29050b735b3839289ef38',
                  'd0390dba6da583506cd12ea5acf2210a4b02753a76137874710a1bccd0668b86'),
}
BUILD_SHA = '63cf4f8020b9bce6881a67e931edd35e35093644dd43f87458b5dbb01fb6ad05'
FP_SHA = '3c69d487cf880a3a5d42482c7d2accc8581097dff109e11a38cce7959724aece'
COMPILER_SHA = '0e800e4decf0de00b5ebeefed814b2cf6551d17291a220ee107b5198cd7ca23b'
LLVM_RSP_SHA = 'ad3e718ec49df75ad6ebb599f22df3825a3e4cb2c6e5ce675e12c6f49ae5f621'
CONTROL_SHA = '22f0afab44698eb2d76d5634c4f7d6f2494eebc3be97cd391e778b959bb2571d'
OWNED_SHA = '37d9d95d078bfbeb8fbca80dc060e2274d1b569589770ed06895825a3a36d9c9'
DEFAULT_ARENA = Path('/tmp/high-level-20261017')
DEFAULT_SDK = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
digest = linux_control.digest


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.new')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    os.replace(temporary, path)


def tree_inventory(root):
    """Hash bytes and symlink identities, following dependency directory links.

    The per-call inode cache avoids hashing equivalent compiler driver aliases
    repeatedly. A fresh call hashes bytes again; it never reuses old hash data.
    """
    root = Path(root)
    cache, rows = {}, []

    def visit(path, ancestors):
        target = path.resolve(strict=True)
        if path.is_dir():
            assert target not in ancestors, f'dependency directory cycle: {path}'
            if path.is_symlink():
                rows.append({'path': str(path.relative_to(root)), 'directory_link': os.readlink(path),
                             'resolved': str(target)})
            for child in sorted(path.iterdir()):
                if child.name == '.DS_Store' or child.name.startswith('._'):
                    continue
                visit(child, ancestors | {target})
        else:
            assert path.is_file(), f'non-regular dependency: {path}'
            stat = target.stat()
            key = (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)
            if key not in cache:
                cache[key] = digest(target)
            row = {'path': str(path.relative_to(root)), 'bytes': stat.st_size, 'sha256': cache[key]}
            if path.is_symlink():
                row.update(link=os.readlink(path), resolved=str(target))
            rows.append(row)

    visit(root, set())
    return rows


def payload_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def runtime_state(folder):
    state = linux_control.guard(folder)
    assert state['disk_free'] > 2 << 30, '2GiB disk floor'
    assert state['anon'] < 50 << 30, 'shared anonymous limit50GiB'
    return state


def released_window(folder):
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    state = runtime_state(folder)
    assert state['anon'] < 8 << 30, 'wait for the large DBG guest to retire before building'
    os.sched_setaffinity(0, {4, 5, 6, 7})
    return runtime_state(folder)


def checked_manifest(args):
    assert re.fullmatch('[0-9a-f]{64}', args.reviewed_manifest_sha256 or '')
    assert re.fullmatch('[0-9a-f]{64}', args.reviewed_patch_sha256 or '')
    assert digest(args.manifest) == args.reviewed_manifest_sha256
    assert digest(args.patch) == args.reviewed_patch_sha256
    rows = json.loads(args.manifest.read_bytes())
    assert len(rows) == 6
    assert {(r['repository'], r['path']) for r in rows} == {(n, h) for n in REPOS for h in HEADERS}
    for row in rows:
        for field in ('before_sha256', 'after_sha256'):
            assert re.fullmatch('[0-9a-f]{64}', row[field])
        source = args.patch_root / row['repository'] / row['path']
        assert source.resolve().is_relative_to(args.patch_root.resolve())
        assert not source.is_symlink() and digest(source) == row['after_sha256']
    return rows


def input_inventory(arena, sdk, helper, system_files):
    """Complete source is checked separately by the pinned official fingerprint.

    This external inventory covers every SDK bin/include/lib byte, every bundled
    LLVM generated header and consumed archive, the exact response file, Console
    directory, helper sources and original build/identity/fingerprint receipts.
    System headers and the loader/linker input closure are hashed separately.
    """
    result = {'sdk': {}, 'llvm': {}, 'console': {}, 'helpers': {}, 'baseline_receipts': {},
              'system_headers': {}, 'system_link_inputs': []}
    for name in ('bin', 'include', 'lib'):
        result['sdk'][name] = tree_inventory(sdk / name)
    for path in (Path('/usr/include'), Path('/usr/local/include')):
        if path.is_dir():
            result['system_headers'][str(path)] = tree_inventory(path)
    for path in sorted(set(map(Path, system_files))):
        result['system_link_inputs'].append({'path': str(path), 'resolved': str(path.resolve(strict=True)),
                                            'sha256': digest(path), 'bytes': path.stat().st_size})
    llvm = arena / 'llvm'
    rsp = llvm / 'consumer-link.rsp'
    assert digest(rsp) == LLVM_RSP_SHA
    result['llvm']['response'] = {'path': str(rsp), 'sha256': digest(rsp)}
    result['llvm']['generated_include'] = tree_inventory(llvm / 'include')
    archives = [Path(t) for t in shlex.split(rsp.read_text()) if t.endswith('.a')]
    assert len(archives) == 64 and all(p.resolve().is_relative_to(llvm) for p in archives)
    result['llvm']['archives'] = [{'path': str(p), 'sha256': digest(p), 'bytes': p.stat().st_size} for p in archives]
    console = arena / 'uwvm2-ros/test/0017.runtime'
    result['console'] = {'path': str(console), 'files': tree_inventory(console)}
    for path in (helper, Path(linux_control.__file__), Path(linux_control.__file__).parent.parent / 'owned_process.py',
                 arena / 'uwvm2-ros/test/0050.high_level_languages/build_products.py'):
        result['helpers'][str(path)] = digest(path)
    for repo in REPOS:
        root = arena / repo
        fp = root / 'tools/ci/wasm3_source_fingerprint.py'
        assert digest(fp) == FP_SHA
        identity_path = arena / 'products' / repo / 'identity.json'
        identity = json.loads(identity_path.read_bytes())
        source_id, binary = BASELINES[repo]
        assert identity['source_id'] == source_id and identity['binary_sha256'] == binary
        assert digest(arena / 'products' / repo / 'uwvm') == binary
        for p in (fp, identity_path, arena / 'products' / repo / 'sources-before.json', original_build_receipt(arena, repo)):
            result['baseline_receipts'][str(p)] = digest(p)
    assert digest(sdk / 'bin/clang++') == COMPILER_SHA
    return result


def original_build_receipt(arena, repo):
    if repo == 'uwvm2':
        return arena / 'candidate-native-budget/products/build-uwvm2.json'
    return arena / 'products' / ('build-' + repo + '.json')


def canon_commands(record, repo):
    commands = record['commands']
    assert len(commands) == 4 and all(c['exit'] == 0 for c in commands)
    units = ('src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', 'src/uwvm2/uwvm/main.default.cpp',
             'src/uwvm2/uwvm/host_api.default.cpp')
    first = commands[0]['argv']
    file = first[first.index('-c') + 1]
    assert file.endswith('/' + units[0])
    source_root = file[:-len('/' + units[0])]
    product = str(Path(first[first.index('-o') + 1]).parent)
    result = []
    for i, command in enumerate(commands):
        argv = command['argv']
        if i < 3:
            assert argv[argv.index('-c') + 1] == source_root + '/' + units[i]
        normalized = []
        for token in argv:
            if token.startswith('-DUWVM2_BUILD_SOURCE_ID='):
                token = '<BUILD_SOURCE_ID>'
            else:
                token = token.replace(source_root + '/', '<SOURCE>/').replace(product + '/', '<PRODUCT>/')
                token = re.sub(r'[^@\s]*?/uwvm2-ros/third-parties/llvm/', '<ROS_LLVM_SOURCE>/', token) if '/uwvm2-ros/third-parties/llvm/' in token else token
                # Preserve the -I/@ prefix while abstracting only the arena path.
                token = re.sub(r'(?<=-I)/.*?/llvm/include$', '<LLVM>/include', token) if token.startswith('-I') and token.endswith('/llvm/include') else token
                token = re.sub(r'^@/.*/llvm/consumer-link\.rsp$', '@<LLVM>/consumer-link.rsp', token)
            normalized.append(token)
        result.append(normalized)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', choices=('plan', 'prepare', 'build'), default='plan')
    parser.add_argument('--arena', type=Path, default=DEFAULT_ARENA)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--sdk', type=Path, default=DEFAULT_SDK)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--patch-root', type=Path)
    parser.add_argument('--patch', type=Path)
    parser.add_argument('--reviewed-manifest-sha256')
    parser.add_argument('--reviewed-patch-sha256')
    parser.add_argument('--window-released', action='store_true', help='Only after the parent explicitly releases the build window')
    args = parser.parse_args()
    if args.stage == 'plan':
        print(json.dumps({'candidate': str(args.candidate or args.arena / 'fd-readdir-candidate-r1'),
                          'copies': ['src', 'third-parties', 'tools/ci/wasm3_source_fingerprint.py'],
                          'replacement_allowlist': [(r, h) for r in REPOS for h in HEADERS],
                          'llvm_build_forbidden': True, 'helper_sha256': BUILD_SHA,
                          'compiler_sha256': COMPILER_SHA, 'cpus': [4, 5, 6, 7],
                          'owned_rss_limit': 8 << 30, 'preflight_shared_anon_limit': 8 << 30,
                          'running_shared_anon_limit': 50 << 30, 'disk_floor': 2 << 30,
                          'execution_status': 'static preparation only; no snapshot/build has run'}, indent=2))
        return
    assert args.window_released, 'An explicit parent build-window release is required'
    arena = args.arena.resolve(strict=True)
    candidate = (args.candidate or arena / 'fd-readdir-candidate-r1').absolute()
    assert candidate.parent.resolve() == arena and candidate.name.startswith('fd-readdir-candidate-')
    assert not candidate.is_symlink()
    before = released_window(arena)
    helper = Path(__file__).resolve()
    assert digest(Path(linux_control.__file__)) == CONTROL_SHA
    assert digest(Path(linux_control.__file__).parent.parent / 'owned_process.py') == OWNED_SHA
    folder = candidate / ('prepare' if args.stage == 'prepare' else 'build')
    if args.stage == 'prepare':
        assert not candidate.exists(), 'fresh candidate root required'
        candidate.mkdir()
    else:
        assert candidate.is_dir() and not (candidate / 'products').exists(), 'fresh products required'
    folder.mkdir()
    record = {'passed': False, 'stage': args.stage, 'before': before, 'helper_sha256': digest(helper),
              'candidate': str(candidate), 'original_arena': str(arena), 'commands': [], 'guard_stops': []}
    report = folder / 'receipt.json'
    read_guard = linux_control.guard

    def priority_guard(path):
        state = read_guard(path)
        if state['anon'] >= 50 << 30 and not record['guard_stops']:
            record['guard_stops'].append({'anon': state['anon'], 'reason': 'shared50GiB limit'})
            save(report, record)
            raise RuntimeError('candidate shared memory guard stopped the owned command')
        return state

    linux_control.guard = priority_guard

    def run(argv, name, *, env=None, timeout=600):
        command = {'name': name, 'receipt': str(folder / name / 'receipt.json'), 'status': 'started',
                   'argv': list(map(str, argv)), 'timeout': timeout}
        record['commands'].append(command)
        save(report, record)
        try:
            result = linux_control.invoke(argv, folder / name, timeout=timeout, env=env)
        except BaseException as error:
            command.update(status='failed', error=repr(error))
            save(report, record)
            raise
        assert result['retired'] and result['ownership']['closed']
        assert result['ownership']['members'] and all(not m['pidfd_retained'] for m in result['ownership']['members'])
        command['status'] = 'passed'
        save(report, record)
        return (folder / name / 'stdout.log').read_text().strip()

    def fingerprint(root, label):
        assert digest(root / 'tools/ci/wasm3_source_fingerprint.py') == FP_SHA
        dest = folder / (label + '.json')
        value = run(['/usr/bin/python3', root / 'tools/ci/wasm3_source_fingerprint.py', root, dest], label)
        assert value == json.loads(dest.read_bytes())['source_id']
        return json.loads(dest.read_bytes())

    started = time.monotonic()
    external_inputs = None
    external_audited = False
    try:
        if args.stage == 'prepare':
            assert args.manifest and args.patch_root and args.patch
            rows = checked_manifest(args)
            record['reviewed_manifest_sha256'] = args.reviewed_manifest_sha256
            record['reviewed_patch_sha256'] = args.reviewed_patch_sha256
            record['patches'] = rows
            system_files = set()
            # ldd performs dependency inspection on these already trusted host
            # binaries; none of these commands executes a Wasm guest or a build.
            env = dict(os.environ, LD_LIBRARY_PATH=str(args.sdk / 'lib/x86_64-unknown-linux-gnu') + ':' + str(args.sdk / 'lib'))
            targets = [args.sdk / 'bin/clang++', args.sdk / 'bin/ld.lld',
                       *[arena / 'products' / r / 'uwvm' for r in REPOS]]
            for i, target in enumerate(targets):
                output = run(['/usr/bin/ldd', target], 'host-dependencies-' + str(i), env=env)
                assert 'not found' not in output
                for line in output.splitlines():
                    match = re.search(r'(?:=>\s+|^\s*)(/\S+)\s+\(0x[0-9a-f]+\)', line)
                    if match:
                        system_files.add(str(Path(match[1]).absolute()))
            for name in ('Scrt1.o', 'crti.o', 'crtn.o', 'crtbeginS.o', 'crtendS.o',
                         'libssl.so', 'libcrypto.so', 'libdl.a', 'libpthread.a', 'libm.so'):
                output = run([args.sdk / 'bin/clang++', '-print-file-name=' + name], 'link-input-' + name.replace('.', '-'), env=env)
                path = Path(output)
                assert path.is_absolute() and path.is_file(), f'unresolved link input {name}: {output}'
                system_files.add(str(path))
            record['system_dependency_paths'] = sorted(system_files)
            record['input_inventory'] = input_inventory(arena, args.sdk, helper, system_files)
            external_inputs = record['input_inventory']
            save(folder / 'inputs-before.json', record['input_inventory'])
            record['input_inventory_sha256'] = digest(folder / 'inputs-before.json')
            copied_helper = candidate / 'build_products.py'
            original_helper = arena / 'uwvm2-ros/test/0050.high_level_languages/build_products.py'
            assert digest(original_helper) == BUILD_SHA
            shutil.copy2(original_helper, copied_helper)
            reviewed = candidate / 'reviewed'
            reviewed.mkdir()
            shutil.copy2(args.manifest, reviewed / 'candidate-files.json')
            shutil.copy2(args.patch, reviewed / 'candidate-all.diff')
            record['snapshots'] = {}
            last = [0.0]

            def copy_file(source, destination):
                if time.monotonic() - last[0] > .5:
                    runtime_state(candidate)
                    last[0] = time.monotonic()
                assert not Path(source).is_symlink(), 'original source file symlink is unexpected'
                return shutil.copy2(source, destination)

            for repo in REPOS:
                original, root = arena / repo, candidate / repo
                old = fingerprint(original, repo + '-original-before')
                assert old['source_id'] == BASELINES[repo][0]
                root.mkdir()
                for directory in ('src', 'third-parties'):
                    assert not any(p.is_symlink() for p in (original / directory).rglob('*'))
                    shutil.copytree(original / directory, root / directory, copy_function=copy_file,
                                    ignore=shutil.ignore_patterns('.DS_Store', '._*'))
                fp = root / 'tools/ci/wasm3_source_fingerprint.py'
                fp.parent.mkdir(parents=True)
                shutil.copy2(original / 'tools/ci/wasm3_source_fingerprint.py', fp)
                assert digest(fp) == FP_SHA
                clone = fingerprint(root, repo + '-clone-before-patch')
                assert clone == old
                for row in (r for r in rows if r['repository'] == repo):
                    target = root / row['path']
                    source = args.patch_root / repo / row['path']
                    assert digest(target) == row['before_sha256']
                    assert target.stat().st_ino != (original / row['path']).stat().st_ino
                    replacement = target.with_name(target.name + '.candidate-new')
                    copy_file(source, replacement)
                    assert digest(replacement) == row['after_sha256']
                    os.replace(replacement, target)
                    capture = reviewed / repo / row['path']
                    capture.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, capture)
                    assert digest(capture) == row['after_sha256']
                new = fingerprint(root, repo + '-candidate')
                before_map = {r['path']: r['sha256'] for r in old['files']}
                after_map = {r['path']: r['sha256'] for r in new['files']}
                assert before_map.keys() == after_map.keys()
                assert {p for p in before_map if before_map[p] != after_map[p]} == set(HEADERS)
                assert fingerprint(original, repo + '-original-after') == old
                original_meta = original_build_receipt(arena, repo)
                shutil.copy2(original_meta, reviewed / ('baseline-build-' + repo + '.json'))
                record['snapshots'][repo] = {'old_source_id': old['source_id'], 'new_source_id': new['source_id'],
                                             'original_build_receipt_sha256': digest(original_meta)}
                save(report, record)
            (candidate / 'llvm').symlink_to(arena / 'llvm', target_is_directory=True)
            console = candidate / 'uwvm2-ros/test/0017.runtime'
            console.parent.mkdir(parents=True)
            console.symlink_to(arena / 'uwvm2-ros/test/0017.runtime', target_is_directory=True)
            record['shared_paths'] = {'llvm': str((candidate / 'llvm').resolve()), 'Console': str(console.resolve())}
            after = input_inventory(arena, args.sdk, helper, system_files)
            save(folder / 'inputs-after.json', after)
            external_audited = True
            record['external_inputs_unchanged'] = after == external_inputs
            assert record['external_inputs_unchanged'], 'original inputs changed during preparation'
        else:
            prepared = json.loads((candidate / 'prepare/receipt.json').read_bytes())
            assert prepared['passed'] and prepared['helper_sha256'] == digest(helper)
            inputs = json.loads((candidate / 'prepare/inputs-before.json').read_bytes())
            assert digest(candidate / 'prepare/inputs-before.json') == prepared['input_inventory_sha256']
            assert inputs == prepared['input_inventory']
            external_inputs = inputs
            system_files = prepared['system_dependency_paths']
            record['system_dependency_paths'] = system_files
            assert input_inventory(arena, args.sdk, helper, system_files) == inputs
            assert digest(candidate / 'build_products.py') == BUILD_SHA
            assert digest(candidate / 'reviewed/candidate-files.json') == prepared['reviewed_manifest_sha256']
            assert digest(candidate / 'reviewed/candidate-all.diff') == prepared['reviewed_patch_sha256']
            record['preparation_receipt_sha256'] = digest(candidate / 'prepare/receipt.json')
            for repo in REPOS:
                old = fingerprint(arena / repo, repo + '-original-before')
                assert old['source_id'] == BASELINES[repo][0]
                new = fingerprint(candidate / repo, repo + '-candidate-before')
                assert new['source_id'] == prepared['snapshots'][repo]['new_source_id']
                # Every --only is a repository name: LLVM configuration/build is unreachable.
                run(['/usr/bin/python3', candidate / 'build_products.py', '--arena', candidate,
                     '--sdk', args.sdk, '--only', repo], repo + '-compile', timeout=1500)
                metadata = json.loads((candidate / 'products' / ('build-' + repo + '.json')).read_bytes())
                baseline = json.loads((candidate / 'reviewed' / ('baseline-build-' + repo + '.json')).read_bytes())
                assert canon_commands(metadata, repo) == canon_commands(baseline, repo), 'build flags or backend drift'
                identity = json.loads((candidate / 'products' / repo / 'identity.json').read_bytes())
                assert identity['source_id'] == new['source_id'] and identity['compiler_sha256'] == COMPILER_SHA
                assert identity['binary_sha256'] == digest(candidate / 'products' / repo / 'uwvm')
                assert fingerprint(candidate / repo, repo + '-candidate-after') == new
                assert fingerprint(arena / repo, repo + '-original-after') == old
                record.setdefault('products', {})[repo] = identity
                save(report, record)
            after = input_inventory(arena, args.sdk, helper, system_files)
            save(folder / 'inputs-after.json', after)
            external_audited = True
            record['external_inputs_unchanged'] = after == inputs
            assert record['external_inputs_unchanged'], 'original inputs changed during compilation'
        assert not record['guard_stops']
        record['passed'] = True
    except BaseException as error:
        record['passed'] = False
        record['error'] = repr(error)
        raise
    finally:
        # Preserve after evidence even when a compiler, patch precondition or
        # metadata check failed. These are bounded hash commands, never retries
        # of a compiler or guest. Keep the original exception as the failure.
        if external_inputs is not None and not external_audited:
            try:
                after = input_inventory(arena, args.sdk, helper, record['system_dependency_paths'])
                save(folder / 'inputs-after-failure.json', after)
                record['external_inputs_unchanged'] = after == external_inputs
                if not record['external_inputs_unchanged']:
                    record['passed'] = False
            except BaseException as error:
                record['passed'] = False
                record['input_audit_error'] = repr(error)
        for repo in REPOS:
            original_before = folder / (repo + '-original-before.json')
            original_after = folder / (repo + '-original-after.json')
            if original_before.exists() and not original_after.exists():
                try:
                    old = json.loads(original_before.read_bytes())
                    after = fingerprint(arena / repo, repo + '-original-after-failure')
                    record.setdefault('original_source_after_failure', {})[repo] = after['source_id']
                    if after != old:
                        record['passed'] = False
                except BaseException as error:
                    record['passed'] = False
                    record.setdefault('source_audit_errors', {})[repo] = repr(error)
        record['seconds'] = time.monotonic() - started
        record['helper_sha256_after'] = digest(helper)
        if (candidate / 'build_products.py').exists():
            record['build_helper_sha256_after'] = digest(candidate / 'build_products.py')
            if record['build_helper_sha256_after'] != BUILD_SHA:
                record['passed'] = False
        try:
            record['after'] = read_guard(candidate)
            if record['after']['memory_events'] != before['memory_events']:
                record['passed'] = False
        except BaseException as error:
            record['after_guard_error'] = repr(error)
            record['passed'] = False
        if record['helper_sha256_after'] != record['helper_sha256']:
            record['passed'] = False
        save(report, record)
    assert record['passed'], f'candidate final audit failed; preserve {report}'


if __name__ == '__main__':
    main()
