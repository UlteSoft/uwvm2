#!/usr/bin/env python3
"""Rebuild a changed Wasm 3 source snapshot from successful compiler commands.

Source paths, output files and the source fingerprint are rebound explicitly.
The current Xmake host stack-probe flags are added to both compile commands.
Bundled LLVM archives retain their original build SID; reuse requires an exact
archive/header/response/vendor manifest match, separately from the product SID.
This is intentionally a fresh compilation, never a replay of its runtime object
or signed native-code cache. Run solely in the required remote Linux cgroup.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import time


SOURCE_FLAG = re.compile(r'^-DUWVM2_BUILD_SOURCE_ID=u8"(sha256:[0-9a-f]{64})"$')
EMBEDDED_SOURCE = re.compile(rb'sha256:[0-9a-f]{64}')
STACK_PROBES = ('-fstack-clash-protection', '-mstack-probe-size=4096')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def embedded_source_ids(path):
    found = set()
    tail = b''
    with Path(path).open('rb') as stream:
        while chunk := stream.read(1 << 20):
            data = tail + chunk
            found.update(item.decode() for item in EMBEDDED_SOURCE.findall(data))
            tail = data[-70:]
    return found


def output_index(command):
    positions = [index for index, token in enumerate(command) if token == '-o']
    if len(positions) != 1 or positions[0] + 1 >= len(command):
        raise RuntimeError('expected exactly one complete compiler output option')
    return positions[0] + 1


def replace_source_id(command, source_id):
    positions = [index for index, token in enumerate(command) if SOURCE_FLAG.fullmatch(token)]
    if len(positions) != 1:
        raise RuntimeError('expected one qualified source fingerprint definition')
    previous = SOURCE_FLAG.fullmatch(command[positions[0]]).group(1)
    command[positions[0]] = '-DUWVM2_BUILD_SOURCE_ID=u8"' + source_id + '"'
    return previous


def bind_source_paths(runtime, cli, source):
    """Retarget absolute snapshot paths and prove every VM translation unit is from source.

    Some qualified ROS commands contain absolute -I and .cpp paths. Merely
    changing cwd and the embedded source ID would silently rebuild the old
    snapshot while stamping the new one.
    """
    suffix = '/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'
    absolute = [token for token in runtime if token.startswith('/') and token.endswith(suffix)]
    if len(absolute) > 1:
        raise RuntimeError('multiple absolute runtime source inputs')
    old_root = Path(absolute[0][:-len(suffix)]) if absolute else None
    if old_root is not None:
        prefix = str(old_root) + '/'
        def retarget(token):
            flag = '-I' if token.startswith('-I/') else ''
            path = token[len(flag):]
            if path == str(old_root):
                return flag + str(source)
            if path.startswith(prefix):
                return flag + str(source) + path[len(str(old_root)):]
            return token
        runtime[:] = [retarget(token) for token in runtime]
        cli[:] = [retarget(token) for token in cli]

    source_dir = (source / 'src').resolve(strict=True)
    expected = {
        'runtime': {'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'},
        'cli': {'src/uwvm2/uwvm/main.default.cpp', 'src/uwvm2/uwvm/host_api.default.cpp'},
    }
    for label, command in (('runtime', runtime), ('cli', cli)):
        actual = set()
        for token in command:
            if not token.endswith('.cpp') or '/src/uwvm2/' not in ('/' + token):
                continue
            path = Path(token)
            resolved = (path if path.is_absolute() else source / path).resolve(strict=True)
            if not resolved.is_relative_to(source_dir):
                raise RuntimeError(f'{label} translation unit escapes selected source: {token}')
            actual.add(resolved.relative_to(source).as_posix())
        if actual != expected[label]:
            raise RuntimeError(f'{label} translation units mismatch selected source: {sorted(actual)}')
    return str(old_root) if old_root is not None else None


def apply_host_stack_probes(command):
    """Match Xmake's Clang x86-64 host protection without changing optimization."""
    if '-O3' not in command or any(item in command for item in ('-target', '--target')) or any(item.startswith(('--target=', '-target=')) and
                                   not item.split('=', 1)[1].startswith('x86_64-') for item in command):
        raise RuntimeError('requires a native x86-64 O3 provenance command')
    if '-fno-stack-clash-protection' in command:
        raise RuntimeError('provenance command explicitly disables host stack protection')
    previous = [item for item in command if item.startswith('-mstack-probe-size=')]
    if any(item != STACK_PROBES[1] for item in previous):
        raise RuntimeError('x86-64 provenance has a conflicting host stack-probe size')
    added = [item for item in STACK_PROBES if item not in command]
    command[1:1] = added
    return added


def bind_llvm_headers(command, provider_root):
    selected = str(provider_root / 'third-parties/llvm/llvm/include')
    suffix = '/third-parties/llvm/llvm/include'
    count = 0
    for index, token in enumerate(command):
        flag = '-I' if token.startswith('-I/') else ''
        path = token[len(flag):]
        if path.endswith(suffix):
            command[index] = flag + selected
            count += 1
    if count != 1:
        raise RuntimeError('expected exactly one qualified LLVM vendor include path')


def verify_llvm_provider(qualification, provider_root, commands):
    """Preserve the real origin of byte-identical, previously built X86 archives."""
    value = json.loads(qualification.read_text())
    origin = value.get('archive_origin_source_id', '')
    if (value.get('passed') is not True or re.fullmatch(r'sha256:[0-9a-f]{64}', origin) is None):
        raise RuntimeError('LLVM archive origin qualification did not pass or lacks its original SID')
    rsp = Path(value['new_response']).resolve(strict=True)
    build = rsp.parent
    header = build / 'include/llvm/Config/llvm-config.h'
    cache = build / 'CMakeCache.txt'
    vendor = provider_root / 'third-parties/llvm'
    source_manifest = vendor / 'sources.sha256'
    expected = {rsp: value['new_response_sha256'], header: value['generated_version_header_sha256'],
                cache: value['cmake_cache_sha256'], source_manifest: value['vendor_manifest_sha256']}
    for path, wanted in expected.items():
        if digest(path) != wanted:
            raise RuntimeError('qualified LLVM provider input changed: ' + str(path))
    vendor_files = set()
    for line in source_manifest.read_text().splitlines():
        wanted, name = line.split('  ', 1)
        path = (vendor / name).resolve(strict=True)
        if (not path.is_relative_to(vendor.resolve(strict=True)) or path in vendor_files or
                re.fullmatch(r'[0-9a-f]{64}', wanted) is None or digest(path) != wanted):
            raise RuntimeError('LLVM vendor source entry differs: ' + name)
        vendor_files.add(path)
    if len(vendor_files) != 12874:
        raise RuntimeError('LLVM vendor source manifest is not the complete qualified 12874-file tree')
    tokens = shlex.split(rsp.read_text())
    if any(token.startswith('@') for token in tokens):
        raise RuntimeError('LLVM response unexpectedly nests another response file')
    archives = [Path(token).resolve(strict=True) for token in tokens if token.endswith('.a')]
    hashes = value['archive_sha256']
    if (len(archives) != value['archive_count'] or len(archives) != 63 or
            len(set(archives)) != len(archives) or set(hashes) != {path.name for path in archives} or
            type(value.get('member_count')) is not int or value['member_count'] <= 0):
        raise RuntimeError('LLVM archive closure/member qualification is incomplete')
    for archive in archives:
        if (archive.parent != (build / 'lib').resolve(strict=True) or
                digest(archive) != hashes[archive.name]):
            raise RuntimeError('LLVM archive changed or escapes its qualified build: ' + str(archive))
    if not any(path.name == 'libLLVMX86CodeGen.a' for path in archives):
        raise RuntimeError('LLVM archive closure lacks the X86 backend')
    for command in commands:
        if ('@' + str(rsp)) in command:
            continue
        # The runtime command only compiles an object; the CLI must link this
        # exact response once. Generated include paths are checked separately.
        if '-c' not in command:
            raise RuntimeError('CLI does not link the exact qualified LLVM response')
    generated = str(build / 'include')
    if any(command.count(generated) + command.count('-I' + generated) != 1 for command in commands):
        raise RuntimeError('compiler commands differ from the qualified generated LLVM headers')
    return {'archive_origin_source_id': origin,
            'original_product_source_id': value.get('product_source_id'),
            'qualification': str(qualification), 'qualification_sha256': digest(qualification),
            'vendor_source_root': str(provider_root), 'vendor_manifest_sha256': digest(source_manifest),
            'vendor_files_verified': len(vendor_files), 'archive_count': len(archives),
            'archive_member_count_originally_verified': value['member_count'],
            'archive_sha256': hashes, 'consumer_link_sha256': digest(rsp),
            'generated_version_header_sha256': digest(header), 'cmake_cache_sha256': digest(cache),
            'scope': 'byte-verified reuse of real source-built X86 archives; no new archive build claimed'}


def process_tree_rss(pid):
    children, sizes = {}, {}
    page_size = os.sysconf('SC_PAGESIZE')
    for path in Path('/proc').iterdir():
        if not path.name.isdecimal():
            continue
        try:
            fields = path.joinpath('stat').read_text().rsplit(')', 1)[1].split()
            parent = int(fields[1])
            resident = max(0, int(fields[21])) * page_size
        except (OSError, ValueError, IndexError):
            continue
        member = int(path.name)
        children.setdefault(parent, []).append(member)
        sizes[member] = resident
    pending, members, total = [pid], [], 0
    while pending:
        member = pending.pop()
        members.append(member)
        total += sizes.get(member, 0)
        pending.extend(children.get(member, ()))
    return total, members


def compile_cpus(value):
    selected = set()
    for item in value.split(','):
        bounds = item.split('-')
        if len(bounds) == 1:
            selected.add(int(bounds[0]))
        elif len(bounds) == 2 and int(bounds[0]) <= int(bounds[1]):
            selected.update(range(int(bounds[0]), int(bounds[1]) + 1))
        else:
            raise ValueError('invalid compiler CPU range')
    if not selected or not selected.issubset(set(range(16, 32))):
        raise ValueError('correctness compilation must use available E-core CPUs 16–31')
    return selected


def guarded_build(command, source, log, limit=56_000_000_000, rss_limit=24 * 1024**3):
    """Stop this command's process group before the shared 64 GiB hard limit."""
    started = time.monotonic()
    peak = int(Path('/sys/fs/cgroup/memory.current').read_text())
    if peak >= 48_000_000_000:
        raise RuntimeError('native O3 build start requires cgroup memory.current below 48 GB')
    free_before = shutil.disk_usage(log.parent).free
    if free_before < 8 * 1024**3:
        raise RuntimeError('native O3 build start requires at least 8 GiB of disk headroom')
    resident_peak = 0
    with log.open('wb') as stream:
        process = subprocess.Popen(command, cwd=source, stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        stopped = None
        while process.returncode is None:
            current = int(Path('/sys/fs/cgroup/memory.current').read_text())
            peak = max(peak, current)
            resident, members = process_tree_rss(process.pid)
            resident_peak = max(resident_peak, resident)
            if resident > rss_limit:
                stopped = 'compiler-process-tree-RSS-cap'
            elif current >= limit:
                stopped = 'cgroup-memory-cap'
            elif shutil.disk_usage(log.parent).free < 2 * 1024**3:
                stopped = 'disk-headroom-cap'
            elif time.monotonic() - started >= 3600:
                stopped = 'one-hour-command-timeout'
            if stopped is not None:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                for member in reversed(members):
                    try:
                        os.kill(member, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            waited, raw_status, usage = os.wait4(process.pid, os.WNOHANG)
            if waited:
                process.returncode = os.waitstatus_to_exitcode(raw_status)
                resident_peak = max(resident_peak, usage.ru_maxrss * 1024)
                break
            time.sleep(0.2)
    if resident_peak > rss_limit:
        stopped = 'compiler-process-tree-RSS-cap'
    return {'exit': process.returncode, 'reason': stopped, 'cgroup_peak_observed_bytes': peak,
            'peak_process_tree_rss_bytes': resident_peak, 'process_tree_rss_limit_bytes': rss_limit,
            'disk_free_before_bytes': free_before, 'disk_free_after_bytes': shutil.disk_usage(log.parent).free,
            'seconds': time.monotonic() - started, 'log_sha256': digest(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--expected-source-id', required=True)
    parser.add_argument('--runtime-command', type=Path, required=True)
    parser.add_argument('--cli-command', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--llvm-provider-source', type=Path, required=True)
    parser.add_argument('--expected-llvm-provider-source-id', required=True)
    parser.add_argument('--llvm-provider-qualification', type=Path, required=True)
    parser.add_argument('--compile-cpus', default='24-31', help='E-core CPU list/ranges; recorded in the build manifest')
    parser.add_argument('--max-process-tree-rss', type=int, default=24 * 1024**3)
    parser.add_argument('--max-cgroup-memory', type=int, default=56_000_000_000)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    selected_cpus = compile_cpus(args.compile_cpus)
    if not selected_cpus.issubset(os.sched_getaffinity(0)):
        raise RuntimeError('requested compiler CPUs are unavailable in the established cgroup')
    if not (0 < args.max_process_tree_rss <= 24 * 1024**3 and
            0 < args.max_cgroup_memory <= 56_000_000_000):
        raise RuntimeError('compiler/shared-memory budgets exceed this qualification lane')
    os.sched_setaffinity(0, selected_cpus)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)

    def fingerprint(name):
        command = ['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
                   str(source), str(output / name)]
        return subprocess.check_output(command, text=True).strip()

    before = fingerprint('source-before.json')
    if before != args.expected_source_id:
        raise RuntimeError('selected source differs from explicitly requested candidate SID')
    provider_source = args.llvm_provider_source.resolve(strict=True)
    provider_id = subprocess.check_output(['python3', str(provider_source / 'tools/ci/wasm3_source_fingerprint.py'),
        str(provider_source), str(output / 'llvm-provider-source-before.json')], text=True).strip()
    if provider_id != args.expected_llvm_provider_source_id:
        raise RuntimeError('selected LLVM vendor source differs from explicitly requested provider SID')
    runtime = shlex.split(args.runtime_command.read_text())
    cli = shlex.split(args.cli_command.read_text())
    prior_source_root = bind_source_paths(runtime, cli, source)
    old_runtime_id = replace_source_id(runtime, before)
    old_cli_id = replace_source_id(cli, before)
    if old_runtime_id != old_cli_id:
        raise RuntimeError('provenance commands came from different source snapshots')
    if '-c' not in runtime or '-c' in cli:
        raise RuntimeError('expected runtime compile and separate CLI link')
    old_object = runtime[output_index(runtime)]
    if cli.count(old_object) != 1:
        raise RuntimeError('CLI must link the exact qualified runtime object once')
    runtime[output_index(runtime)] = str(output / 'runtime.o')
    cli[cli.index(old_object)] = str(output / 'runtime.o')
    cli[output_index(cli)] = str(output / 'uwvm')
    recipe_adjustments = {}
    for label, command in (('runtime', runtime), ('cli', cli)):
        recipe_adjustments[label] = apply_host_stack_probes(command)
        bind_llvm_headers(command, provider_source)
    qualification = args.llvm_provider_qualification.resolve(strict=True)
    provider = verify_llvm_provider(qualification, provider_source, (runtime, cli))
    triple = subprocess.check_output([runtime[0], '-dumpmachine'], text=True).strip()
    if not triple.startswith('x86_64-'):
        raise RuntimeError('qualified native compiler is not an x86-64 host driver')
    before_resources = {name: Path('/sys/fs/cgroup', name).read_text().strip() for name in
                        ('memory.max', 'memory.swap.max', 'memory.current', 'memory.peak',
                         'memory.events', 'cpuset.cpus.effective', 'cpu.stat')}

    # This is an explicit template for separately compiled public-host runtime
    # tests, not an artifact purported to have been compiled already.
    test = [token for token in cli if not token.endswith('/uwvm/host_api.default.cpp')]
    positions = [index for index, token in enumerate(test) if token.endswith('/uwvm/main.default.cpp')]
    if len(positions) != 1:
        raise RuntimeError('cannot derive a source-bound runtime host-test command')
    test[positions[0]] = str(source / 'test/0017.runtime/wasm_thread_performance.cc')
    test[output_index(test)] = str(output / 'host-test-template-output')
    test[1:1] = ['-DUWVM2TEST_RUNNER_USE_LLVM_JIT', '-I' + str(source / 'test/0013.uwvm_int/strict')]

    (output / 'runtime.command').write_text(shlex.join(runtime) + '\n')
    (output / 'cli.command').write_text(shlex.join(cli) + '\n')
    (output / 'test.command').write_text(shlex.join(test) + '\n')
    metadata = dict(source=str(source), source_id=before, prior_source_id=old_runtime_id,
                    prior_source_root=prior_source_root,
                    llvm_provider={**provider, 'provider_source_id': provider_id},
                    stack_probe_flags=list(STACK_PROBES), recipe_adjustments=recipe_adjustments,
                    xmake_rule_sha256=digest(source / 'xmake/rule.lua'),
                    test_command=test, test_command_is_unexecuted_template=True,
                    compiler_triple=triple, resources_before=before_resources,
                    compile_cpus=sorted(selected_cpus),
                    max_process_tree_rss_bytes=args.max_process_tree_rss,
                    max_cgroup_memory_bytes=args.max_cgroup_memory,
                    runtime_command=runtime, cli_command=cli,
                    input_runtime_command_sha256=digest(args.runtime_command),
                    input_cli_command_sha256=digest(args.cli_command),
                    runner_sha256=digest(Path(__file__)), compiler_sha256=digest(runtime[0]),
                    cgroup={name: Path('/sys/fs/cgroup', name).read_text().strip()
                            for name in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')})
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    for label, command in (('runtime', runtime), ('cli', cli)):
        result = guarded_build(command, source, output / (label + '.log'),
                               args.max_cgroup_memory, args.max_process_tree_rss)
        metadata[label + '_build'] = result
        (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
        if result['exit'] != 0 or result['reason'] is not None:
            raise RuntimeError(label + ' source-bound compilation failed; see preserved build log')
        print('BUILT', label, flush=True)
    after = fingerprint('source-after.json')
    if before != after:
        (output / 'uwvm').rename(output / 'uwvm-source-changed-invalid')
        raise RuntimeError('source or dependency changed during compilation')
    after_provider_id = subprocess.check_output(['python3', str(provider_source / 'tools/ci/wasm3_source_fingerprint.py'),
        str(provider_source), str(output / 'llvm-provider-source-after.json')], text=True).strip()
    if (after_provider_id != provider_id or
            verify_llvm_provider(qualification, provider_source, (runtime, cli)) != provider):
        raise RuntimeError('LLVM provider source or archive inputs changed during compilation')
    after_resources = {name: Path('/sys/fs/cgroup', name).read_text().strip()
                       for name in before_resources}
    before_events = dict(line.split() for line in before_resources['memory.events'].splitlines())
    after_events = dict(line.split() for line in after_resources['memory.events'].splitlines())
    if any(before_events[key] != after_events[key] for key in ('oom', 'oom_kill')):
        raise RuntimeError('cgroup recorded an OOM while compiling this product')
    embedded = embedded_source_ids(output / 'uwvm')
    if embedded != {before}:
        (output / 'uwvm').rename(output / 'uwvm-embedded-source-invalid')
        raise RuntimeError(f'binary embedded source IDs differ: {sorted(embedded)}')
    metadata['runtime_object_sha256'] = digest(output / 'runtime.o')
    metadata['binary_sha256'] = digest(output / 'uwvm')
    metadata['binary_embedded_source_ids'] = sorted(embedded)
    metadata['resources_after'] = after_resources
    metadata['test_command_sha256'] = digest(output / 'test.command')
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('PASS requalified CLI', before, flush=True)


if __name__ == '__main__':
    main()
