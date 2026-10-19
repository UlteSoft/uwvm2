#!/usr/bin/env python3
"""Prepare or build fresh current CLI products in a later exclusive Linux window.

This inherits the existing exact 64GiB/no-swap admission and pidfd-owned process
helper. The current 2.5GiB component window is intentionally insufficient for
this builder; --execute requires a separately allocated larger total-cgroup
budget and at least 3GiB free root/output disk throughout. No LLVM rebuild, old runtime.o,
object cache or cross-repository object reuse is performed. Each selected repo
compiles runtime/main/host-api serially, with a default inherited AS limit of 4GiB. A separately authorized window
may explicitly select --tu-as-bytes up to 6GiB; this does not change the total
cgroup ceiling, reactive reserve or serial compilation policy.

With --wasi-sdk, also produce the unchanged real standard-library -g fixture
using the existing SDK24 libc++ with -fno-exceptions to match its no-EH
runtime, plus a native fixture for GDB/LLDB comparison. This producer build does
not qualify C++ exception handling or valueless-by-exception paths.
ROS imports are explicitly selected with --ros-imports; the default none keeps
the historical recipe. Only the uwvm2-ros repository receives those definitions
in all three product TUs. A repository name alone does not enable ROS imports.
All three product TUs enable UWVM_USE_THREAD_LOCAL, matching xmake's default
TLS feature profile. All three also use -fconstexpr-steps=8388608, matching
xmake's ROS+LLVM feature budget for the expanded constexpr command table.
Every change to these shared flags requires fresh objects.
A prepared plan is not a successful build, runtime or performance receipt.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shlex
import shutil
import subprocess
import sys
import time

REPOSITORIES = ('uwvm2', 'uwvm2-ros', 'uwvm2-stable')
# Native-compiled producer support; interpreted Python/JS are outside this profile.
DEBUG_LANGUAGES = ('C', 'CPP', 'RUST', 'GO', 'TINYGO', 'ZIG', 'JAVA', 'CSHARP', 'FSHARP', 'VB', 'ASSEMBLYSCRIPT', 'MOONBIT', 'KOTLIN')
UNITS = (('runtime', 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),
         ('main', 'src/uwvm2/uwvm/main.default.cpp'),
         ('host-api', 'src/uwvm2/uwvm/host_api.default.cpp'))
FOUR_GIB = 4 << 30
SIX_GIB = 6 << 30
COMPONENT_WINDOW = 2684354560
DISK_FLOOR = 3 << 30
REACTIVE_RESERVE = 128 << 20
HLL = Path('/home/macromodel/Documents/uwvm-test-261018/workspaces/uwvm-hll-20261017')
SDK = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
WASI24 = HLL / 'python/work-r4/sources/sdk/wasi-sdk-24.0-x86_64-linux'


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path, observe=None):
    with Path(path).open('rb') as stream:
        if observe is None:
            return hashlib.file_digest(stream, 'sha256').hexdigest()
        digest = hashlib.sha256()
        while data := stream.read(1024 * 1024):
            digest.update(data)
            observe()  # Large archive/header pinning also stays inside the allocated window.
        return digest.hexdigest()


def pin(path, observe=None):
    path = Path(path).resolve(strict=True)
    if not path.is_file():
        raise RuntimeError('regular build input required: ' + str(path))
    return {'sha256': sha(path, observe), 'bytes': path.stat().st_size}


def publish(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def dependencies(path):
    text = path.read_text().replace('\\\n', ' ')
    if ':' not in text:
        raise RuntimeError('compiler dependency file has no target')
    names = shlex.split(text.split(':', 1)[1])
    result = {str(Path(name).resolve(strict=True)): pin(name) for name in names}
    if not result:
        raise RuntimeError('compiler dependency closure is empty')
    return result


def provider_inputs(args, observe=None):
    response = args.llvm_link_response
    paths = [args.sdk / 'bin/clang++', args.sdk / 'bin/ld.lld', response,
             args.llvm / 'include/llvm/Config/llvm-config.h',
             args.llvm_source_include / 'llvm/IR/Module.h', args.ssl, args.crypto]
    for token in shlex.split(response.read_text()):
        if token.endswith('.a'):
            archive = Path(token).resolve(strict=True)
            if not archive.is_relative_to(args.llvm):
                raise RuntimeError('LLVM link response contains a foreign archive: ' + token)
            paths.append(archive)
    # Pin selected native SDK link-provider bytes; compile headers additionally
    # have per-TU -M-before / actual -MD-after closures, including system headers.
    for path in (args.sdk / 'lib/x86_64-unknown-linux-gnu').rglob('*'):
        if path.is_file():
            paths.append(path)
    if args.wasi_sdk is not None:
        paths.extend((args.wasi_sdk / 'bin/clang++', args.native_compare_cxx, args.wasi_vector_header))
        for path in (args.wasi_sdk / 'share/wasi-sysroot/lib').rglob('*'):
            if path.is_file():
                paths.append(path)
    return {str(Path(path).resolve(strict=True)): pin(path, observe) for path in paths}


def ros_import_profile(args, root):
    mode = args.ros_imports if root.name == 'uwvm2-ros' else 'none'
    defines = []
    if mode in ('ros1', 'both'):
        defines.append('-DUWVM_IMPORT_ROS1')
    if mode in ('ros2', 'both'):
        defines.append('-DUWVM_IMPORT_ROS2')
    return mode, defines


def recipe(args, root, source_id):
    flags = ['-std=c++26', '-stdlib=libc++', '-fno-rtti', '-fasynchronous-unwind-tables', '-O2', '-g1',
             '-fconstexpr-steps=8388608',
             '-Wno-undefined-inline', '-DUWVM=2', '-DUWVM_USE_LLVM_JIT', '-DUWVM_DISABLE_INT',
             '-DUWVM_USE_THREAD_LOCAL',
             '-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519', '-DUWVM_ENABLE_WASIP1_SOCKET',
             '-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
             '-DUWVM2_BUILD_SOURCE_ID=u8"' + source_id + '"',
             '-I' + str(args.llvm / 'include'), '-I' + str(args.llvm_source_include)]
    flags.extend('-DUWVM_DEBUG_LANGUAGE_' + name + '=1' for name in DEBUG_LANGUAGES)
    _, ros_defines = ros_import_profile(args, root)
    flags.extend(ros_defines)
    for relative in ('src', 'third-parties/bizwen/include', 'third-parties/fast_io/include', 'third-parties/boost_unordered/include'):
        flags.append('-I' + str(root / relative))
    library = args.sdk / 'lib/x86_64-unknown-linux-gnu'
    link = ['-stdlib=libc++', '-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind',
            '-L' + str(library), '-Wl,-rpath,' + str(library)]
    return flags, link


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True, help='parent containing three separate immutable source cuts')
    parser.add_argument('--out', type=Path, required=True, help='new private directory; no existing objects are accepted')
    parser.add_argument('--only', choices=REPOSITORIES)
    parser.add_argument('--ros-imports', choices=('none', 'ros1', 'ros2', 'both'), default='none',
                        help='explicit imports for uwvm2-ros only; all its three TUs use the same definitions')
    parser.add_argument('--sdk', type=Path, default=SDK)
    parser.add_argument('--llvm', type=Path, default=HLL / 'llvm')
    parser.add_argument('--llvm-link-response', type=Path, help='explicit owned link response; default LLVM/consumer-link.rsp; archives must resolve inside --llvm')
    parser.add_argument('--llvm-source-include', type=Path, default=HLL / 'uwvm2-ros/third-parties/llvm/llvm/include')
    parser.add_argument('--ssl', type=Path, default=Path('/usr/lib/x86_64-linux-gnu/libssl.so'))
    parser.add_argument('--crypto', type=Path, default=Path('/usr/lib/x86_64-linux-gnu/libcrypto.so'))
    parser.add_argument('--wasi-sdk', type=Path, help='existing SDK24; validated path is ' + str(WASI24))
    parser.add_argument('--native-compare-cxx', type=Path, default=Path('/usr/bin/g++-15'), help='native libstdc++ debugger control compiler; distinct ABI/provider is recorded')
    parser.add_argument('--window-memory-bytes', type=int, required=True, help='later exclusive total existing-cgroup memory.current ceiling')
    parser.add_argument('--tu-as-bytes', type=int, default=FOUR_GIB, help='default 4GiB; an explicit separately authorized value may be at most 6GiB')
    parser.add_argument('--disk-reserve-bytes', type=int, default=DISK_FLOOR)
    parser.add_argument('--root-disk-reserve-bytes', type=int, default=DISK_FLOOR,
                        help='default3GiB; explicit2GiB is allowed only for separate tmpfs outputs/temporaries')
    parser.add_argument('--maximum-seconds', type=int, default=7200)
    parser.add_argument('--keep-objects', action='store_true')
    parser.add_argument('--execute', action='store_true', help='actually build; omitted creates only the inspectable plan')
    args = parser.parse_args()
    if args.only in ('uwvm2', 'uwvm2-stable') and args.ros_imports != 'none':
        parser.error('ROS imports require the uwvm2-ros repository; ordinary/stable never receive ROS definitions')
    if not COMPONENT_WINDOW < args.window_memory_bytes <= 64 << 30:
        parser.error('full product needs a separately allocated window larger than the current 2.5GiB component ceiling')
    if not 0 < args.tu_as_bytes <= SIX_GIB or args.disk_reserve_bytes < DISK_FLOOR or not 1 <= args.maximum_seconds <= 14400:
        parser.error('TU AS <=6GiB (default 4GiB), disk reserve >=3GiB and a finite <=4h window are required')
    if not 2 << 30 <= args.root_disk_reserve_bytes <= args.disk_reserve_bytes:
        parser.error('root reserve must be at least2GiB and no greater than the output reserve')
    for name in ('arena', 'sdk', 'llvm', 'llvm_source_include', 'ssl', 'crypto'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.llvm_link_response = (args.llvm_link_response if args.llvm_link_response is not None else
                               args.llvm / 'consumer-link.rsp').resolve(strict=True)
    if not args.llvm_link_response.is_file():
        parser.error('selected LLVM link response must be a regular file')
    args.wasi_vector_header = None
    if args.wasi_sdk is not None:
        args.wasi_sdk = args.wasi_sdk.resolve(strict=True)
        args.native_compare_cxx = args.native_compare_cxx.resolve(strict=True)
        include = args.wasi_sdk / 'share/wasi-sysroot/include'
        # SDK24 also installs libc++ in target-specific include directories.
        # This is a provider preflight only; the compiler's actual -MD closure
        # remains authoritative about the headers selected by its default target.
        for relative in ('c++/v1/vector', 'wasm32-wasip1/c++/v1/vector', 'wasm32-wasi/c++/v1/vector'):
            candidate = include / relative
            if candidate.is_file():
                args.wasi_vector_header = candidate.resolve(strict=True)
                break
        if args.wasi_vector_header is None:
            parser.error('existing common or wasm32-wasi/wasip1 WASI SDK libc++ vector header is required')
    selected = (args.only,) if args.only else REPOSITORIES
    owner_root = args.arena / selected[0]
    admission = load(owner_root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'pretty_product_admission')
    proof = admission.admit()
    sys.path.insert(0, str(owner_root / 'test/0050.high_level_languages'))
    from owned_process import remember_process, wait_process, terminate_group, owned_process_record
    if not args.out.resolve().is_relative_to(Path('/tmp')):
        raise RuntimeError('new build outputs must stay in a private /tmp directory counted by the existing cgroup')
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    args.out = args.out.resolve(strict=True)
    reduced_root_reserve = args.root_disk_reserve_bytes < DISK_FLOOR
    if reduced_root_reserve:
        # Compilers/linkers/producers write only in this private tmpfs. Root
        # tools/providers remain read-only inputs, with their own free reserve.
        mounts = [line.split() for line in Path('/proc/self/mountinfo').read_text().splitlines()]
        on_tmpfs = any(len(row)>6 and row[4]=='/tmp' and row[row.index('-')+1]=='tmpfs' for row in mounts if '-' in row)
        if not on_tmpfs or args.out.stat().st_dev == Path('/').stat().st_dev:
            raise RuntimeError('reduced root reserve requires the existing separate /tmp tmpfs')

    record = {'passed': False, 'executed': args.execute, 'admission': proof, 'repositories': [],
              'scope': 'three actually compiled current TUs plus source/provider/MD before-after equality; product execution and full current qualification are separate',
              'source_root': str(args.arena), 'runtime_reused': False,
              'ros_imports_requested': args.ros_imports, 'ros_imports_repository': 'uwvm2-ros',
              'debug_languages_enabled': list(DEBUG_LANGUAGES), 'interpreted_language_bridges_enabled': [],
              'thread_local_profile_enabled': True, 'thread_local_profile': 'UWVM_USE_THREAD_LOCAL, matching xmake default in all three fresh product TUs',
              'constexpr_steps': 8388608, 'constexpr_steps_profile': 'matching xmake ROS+LLVM feature budget in all three fresh product TUs',
              'llvm_link_response': str(args.llvm_link_response),
              'wasi_preflight_vector_header': str(args.wasi_vector_header) if args.wasi_vector_header is not None else None,
              'tu_rlimit_as_bytes': args.tu_as_bytes, 'tu_rlimit_as_default_bytes': FOUR_GIB,
              'tu_rlimit_as_supported_maximum_bytes': SIX_GIB, 'window_memory_bytes': args.window_memory_bytes,
              'reactive_reserve_bytes': REACTIVE_RESERVE,
              'reactive_stop_threshold_bytes': args.window_memory_bytes - REACTIVE_RESERVE,
              'disk_reserve_bytes': args.disk_reserve_bytes, 'root_disk_reserve_bytes': args.root_disk_reserve_bytes,
              'separate_tmpfs_temporaries': reduced_root_reserve, 'observed_memory_peak_bytes': 0,
              'minimum_observed_free_bytes': None, 'commands': []}
    began = time.monotonic()
    receipt = args.out / 'results.json'
    environment = dict(os.environ, LC_ALL='C', LANG='C', PYTHONDONTWRITEBYTECODE='1',
                       LD_LIBRARY_PATH=str(args.sdk / 'lib/x86_64-unknown-linux-gnu') + ':' + str(args.sdk / 'lib'))
    environment['TMPDIR'] = str(args.out)
    limits = {'AS': args.tu_as_bytes, 'FSIZE': 2 << 30, 'CORE': 0}

    def observe():
        current = int(Path('/sys/fs/cgroup' + admission.CGROUP + '/memory.current').read_text())
        output_free = shutil.disk_usage(args.out).free
        root_free = shutil.disk_usage(Path('/')).free
        free = min(output_free, root_free)
        record['last_disk_observation'] = {'rootfs_free_bytes': root_free, 'outputfs_free_bytes': output_free}
        record['observed_memory_peak_bytes'] = max(record['observed_memory_peak_bytes'], current)
        previous = record['minimum_observed_free_bytes']
        record['minimum_observed_free_bytes'] = free if previous is None else min(previous, free)
        if current >= args.window_memory_bytes - REACTIVE_RESERVE:
            raise RuntimeError('total cgroup memory.current reached the reactive stop threshold (128MiB below allocated ceiling)')
        if output_free < args.disk_reserve_bytes or root_free < args.root_disk_reserve_bytes:
            raise RuntimeError('root/output free disk below the separately recorded physical reserves')
        if time.monotonic() - began > args.maximum_seconds:
            raise TimeoutError('allocated full-product window expired')

    def child_limits():
        resource.setrlimit(resource.RLIMIT_AS, (limits['AS'], limits['AS']))
        resource.setrlimit(resource.RLIMIT_FSIZE, (limits['FSIZE'], limits['FSIZE']))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    def run(argv, log, timeout):
        admission.admit()
        observe()
        child = None
        row = {'actual_argv': list(map(str, argv)), 'log': str(log), 'rlimits': limits, 'returncode': None}
        record['commands'].append(row)
        started = time.monotonic()
        publish(receipt, record)
        try:
            with log.open('wb') as stream:
                child = subprocess.Popen(row['actual_argv'], cwd=args.arena, stdout=stream, stderr=subprocess.STDOUT,
                                         env=environment, start_new_session=True, preexec_fn=child_limits)
                remember_process(child)
                row['returncode'] = wait_process(child, min(timeout, args.maximum_seconds), observe=observe)
        finally:
            terminate_group(child)
            row.update(seconds=time.monotonic() - started, process_ownership=owned_process_record(child),
                       log_sha256=sha(log) if log.exists() else None)
            publish(receipt, record)
        if row['returncode'] != 0:
            raise RuntimeError('build command failed; see ' + str(log))
        return row

    # Planning is intentionally read-only with respect to products and sources.
    # Resource values remain recorded; an execution rechecks all providers below.
    for name in selected:
        root = (args.arena / name).resolve(strict=True)
        flags, link = recipe(args, root, '<actual source fingerprint before compilation>')
        ros_mode, ros_defines = ros_import_profile(args, root)
        record['repositories'].append({'repository': name, 'source_root': str(root), 'passed': False,
            'runtime_reused': False, 'all_product_TUs_fresh': False, 'components': {},
            'actual_ros_import_mode': ros_mode, 'actual_ros_import_defines': ros_defines,
            'planned_compile_flags': flags, 'planned_units': list(UNITS), 'planned_link_flags': link})
    publish(receipt, record)
    if not args.execute:
        record['prepared_only'] = True
        publish(receipt, record)
        print('PREPARED product plan only; no compiler/linker/guest was launched:', receipt)
        return 0
    try:
        observe()
        record['provider_pins_before'] = provider_inputs(args, observe)
        record['helper_pins'] = {str(path): pin(path) for path in (Path(__file__),
            owner_root / 'test/0018.debugger/run_pretty_printer_matrix.py',
            owner_root / 'test/0050.high_level_languages/owned_process.py')}
        for row in record['repositories']:
            root = Path(row['source_root'])
            dest = args.out / row['repository']
            dest.mkdir(mode=0o700)
            fingerprint = root / 'tools/ci/wasm3_source_fingerprint.py'
            run([sys.executable, '-I', '-B', fingerprint, root, dest / 'sources-before.json'], dest / 'source-before.log', 600)
            source_id = json.loads((dest / 'sources-before.json').read_text())['source_id']
            row['source_id'] = row['source_id_external'] = source_id
            flags, link = recipe(args, root, source_id)
            objects = []
            for label, unit in UNITS:
                object_path, before_dep, actual_dep = dest / (label + '.o'), dest / (label + '-before.d'), dest / (label + '.d')
                run([args.sdk / 'bin/clang++', *flags, '-M', '-MF', before_dep, '-MT', object_path, root / unit],
                    dest / (label + '-dependencies.log'), 600)
                before = dependencies(before_dep)
                proof = run([args.sdk / 'bin/clang++', *flags, '-MD', '-MF', actual_dep, '-MT', object_path,
                             '-c', root / unit, '-o', object_path], dest / (label + '-compile.log'), 2400)
                after = dependencies(actual_dep)
                if before != after:
                    raise RuntimeError('actual source/header/SDK dependency bytes changed while compiling ' + label)
                component = {'same_task_or_historical_object_reused': False, 'actual_compile_proof': proof,
                             'actual_dependencies': after, 'dependencies_before_equals_after': True,
                             'object': pin(object_path), 'object_path': str(object_path)}
                row['components'][label] = component
                publish(dest / (label + '-receipt.json'), component)
                objects.append(object_path)
            binary = dest / 'uwvm'
            proof = run([args.sdk / 'bin/clang++', *link, *objects, '@' + str(args.llvm_link_response),
                         args.ssl, args.crypto, '-ldl', '-pthread', '-o', binary], dest / 'link.log', 1200)
            row.update(main_link_returncode=proof['returncode'], product=pin(binary), product_path=str(binary),
                       binary_sha256=sha(binary), all_product_TUs_fresh=True)
            run([sys.executable, '-I', '-B', fingerprint, root, dest / 'sources-after.json'], dest / 'source-after.log', 600)
            row['inputs_before_equals_after'] = json.loads((dest / 'sources-after.json').read_text())['source_id'] == source_id
            if not row['inputs_before_equals_after']:
                raise RuntimeError('repository source identity changed across its fresh three-TU build')
            row['source_tools_headers_SDK_MD_before_after_equal'] = all(
                component['actual_dependencies'] == {path: pin(path) for path in component['actual_dependencies']}
                for component in row['components'].values())
            if not row['source_tools_headers_SDK_MD_before_after_equal']:
                raise RuntimeError('compiled component dependency bytes changed before product publication')
            if not args.keep_objects:
                for path in objects:
                    path.unlink()  # Only this new build's successfully linked, hash-recorded objects.
                row['owned_objects_retired_after_successful_link'] = True
            if args.wasi_sdk is not None:
                row['fixtures'] = {}
                for tag, relative in (('containers', 'test/0018.debugger/fixtures/pretty_containers.cc'),
                                      ('configuration', 'test/0018.debugger/fixtures/source_print_configuration.cc'),
                                      ('references', 'test/0018.debugger/fixtures/pretty_references.cc'),
                                      ('sequences', 'test/0018.debugger/fixtures/pretty_sequences.cc'),
                                      ('stateful', 'test/0018.debugger/fixtures/pretty_sequences_stateful.cc'),
                                      ('coroutine', 'test/0018.debugger/fixtures/pretty_coroutine.cc'),
                                      ('latency', 'test/0018.debugger/pretty_printer_native.cc')):
                    fixture = root / relative
                    fixture_before = pin(fixture)
                    wasm, native = dest / (tag + '.wasm'), dest / (tag + '.native')
                    producer_rows = []
                    for kind in ('wasi', 'native'):
                        output = wasm if kind == 'wasi' else native
                        if kind == 'wasi':
                            compiler, options = args.wasi_sdk / 'bin/clang++', ['-std=c++20', '-stdlib=libc++', '-fno-exceptions', '-O0', '-g', '-fno-omit-frame-pointer']
                        elif tag == 'latency':
                            compiler, options = args.native_compare_cxx, ['-std=c++20', '-O0', '-g', '-fno-omit-frame-pointer']
                        else:
                            compiler, options = args.sdk / 'bin/clang++', ['-std=c++20', '-O0', '-g', '-fno-omit-frame-pointer', *link]
                        before_dep, actual_dep = dest / (tag + '-' + kind + '-before.d'), dest / (tag + '-' + kind + '.d')
                        run([compiler, *options, '-M', '-MF', before_dep, '-MT', output, fixture],
                            dest / (tag + '-' + kind + '-dependencies.log'), 300)
                        before = dependencies(before_dep)
                        proof = run([compiler, *options, '-MD', '-MF', actual_dep, '-MT', output, fixture, '-o', output],
                                    dest / (tag + '-' + kind + '-compile.log'), 300)
                        after = dependencies(actual_dep)
                        if before != after or pin(fixture) != fixture_before:
                            raise RuntimeError('real producer source/header bytes changed during ' + tag + '-' + kind)
                        producer_rows.append({'kind': kind, 'compiler': str(compiler), 'compiler_pin': pin(compiler),
                            'actual_compile_options': list(options),
                            'actual_compile_proof': proof, 'actual_dependencies': after, 'dependencies_before_equals_after': True})
                    if wasm.read_bytes()[:8] != b'\x00asm\x01\x00\x00\x00':
                        raise RuntimeError('standard-library compiler did not emit core Wasm')
                    row['fixtures'][tag] = {'source': str(fixture), 'source_pin': fixture_before,
                        'wasm': str(wasm), 'wasm_pin': pin(wasm), 'native': str(native), 'native_pin': pin(native),
                        'wasm_bytes_not_rewritten': True, 'producers': producer_rows,
                        'native_comparison_provider': 'system libstdc++ compiler; distinct from SDK24 WASI libc++' if tag == 'latency' else 'native SDK libc++',
                        'scope': 'actual library producers and before/after MD closure; no guest execution or debugger speed inferred'}
            row['build_completed'] = True
            row['admission_after'] = admission.admit()
            publish(dest / 'build-receipt.json', row)
            publish(receipt, record)
        record['provider_pins_after'] = provider_inputs(args, observe)
        if record['provider_pins_after'] != record['provider_pins_before']:
            raise RuntimeError('pinned LLVM/native SDK/link-provider bytes changed across builds')
        for row in record['repositories']:
            for component in row['components'].values():
                if component['actual_dependencies'] != {path: pin(path) for path in component['actual_dependencies']}:
                    raise RuntimeError('compiled dependency closure changed before the final all-repository publication')
            row['provider_inputs_before_equals_after'] = True
            row['passed'] = row['build_completed']
            publish(args.out / row['repository'] / 'build-receipt.json', row)
        record['inputs_before_equals_after'] = True
        record['passed'] = all(row['passed'] for row in record['repositories'])
    except BaseException as error:
        record['error'] = type(error).__name__ + ': ' + str(error)
        raise
    finally:
        record['seconds'] = time.monotonic() - began
        record['admission_after'] = admission.admit()
        publish(receipt, record)
    print('PASS three-TU compilation and before/after MD closure; CLI/DAP/qualification/latency remain separate:', receipt)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
