#!/usr/bin/env python3
"""Compile and execute the production pretty-printer component on Linux/QEMU.

This is owned DWARF-shaped DATA coverage, not complete product/debugger parity.
Every child inherits the admitted existing 64 GiB cgroup. Missing providers and
failed compiler/emulator runs are recorded as failures, never as passes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import shlex
import struct
import subprocess
import sys
import time


CGROUP = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
BOOT = '51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
MEMORY_RESERVE = 128 << 20
DISK_RESERVE = 3 << 30
PROFILES = {
    'x86_64': ('x86_64-linux-gnu', 'x86_64', 62, 64, 'little', []),
    'i686': ('i686-linux-gnu', 'i386', 3, 32, 'little', ['-msse2']),
    'aarch64': ('aarch64-linux-gnu', 'aarch64', 183, 64, 'little', []),
    'arm': ('arm-linux-gnueabi', 'arm', 40, 32, 'little', ['-mcpu=arm926ej-s', '-mfloat-abi=soft']),
    'armhf': ('arm-linux-gnueabihf', 'arm', 40, 32, 'little', ['-mcpu=cortex-a15', '-mfpu=neon-vfpv4']),
    'loongarch64': ('loongarch64-linux-gnu', 'loongarch64', 258, 64, 'little', ['-march=la464']),
    'ppc64': ('powerpc64-linux-gnu', 'ppc64', 21, 64, 'big', ['-mcpu=power8']),
    'ppc64le': ('powerpc64le-linux-gnu', 'ppc64le', 21, 64, 'little', ['-mcpu=power8']),
    'mips64': ('mips64-linux-gnuabi64', 'mips64', 8, 64, 'big', ['-march=mips64r2', '-mabi=64']),
    'riscv64': ('riscv64-linux-gnu', 'riscv64', 243, 64, 'little', ['-march=rv64gc', '-mabi=lp64d']),
    'sparc64': ('sparc64-linux-gnu', 'sparc64', 43, 64, 'big', []),
    's390x': ('s390x-linux-gnu', 's390x', 22, 64, 'big', ['-march=z14']),
}
ROS_PROFILES = {'x86_64', 'i686', 'aarch64', 'arm', 'armhf'}
DEBUG_LANGUAGES = ('C', 'CPP', 'RUST', 'GO', 'TINYGO', 'ZIG', 'JAVA', 'CSHARP', 'FSHARP', 'VB', 'ASSEMBLYSCRIPT', 'MOONBIT', 'KOTLIN')
CASES = {
    'pretty_printer_text_display': 'test/0018.debugger/pretty_printer_text_display.cc',
    'pretty_printer_scope_paths': 'test/0018.debugger/pretty_printer_scope_paths.cc',
    'pretty_printer_bench': 'test/0018.debugger/pretty_printer_bench.cc',
    'pretty_printer_stateful_contiguous': 'test/0018.debugger/pretty_printer_stateful_contiguous.cc',
    'pretty_printer_libcxx': 'test/0018.debugger/pretty_printer_libcxx.cc',
    'pretty_printer_linked': 'test/0018.debugger/pretty_printer_linked.cc',
    'pretty_printer_profiles': 'test/0018.debugger/pretty_printer_profiles.cc',
    'pretty_printer_sequences': 'test/0018.debugger/pretty_printer_sequences.cc',
    'pretty_printer_libcxx_nodes': 'test/0018.debugger/pretty_printer_libcxx_nodes.cc',
    'pretty_printer_values': 'test/0018.debugger/pretty_printer_values.cc',
    'pretty_printer_value_aliases': 'test/0018.debugger/pretty_printer_value_aliases.cc',
    'pretty_printer_array_index': 'test/0018.debugger/pretty_printer_array_index.cc',
    'pretty_printer_std_array': 'test/0018.debugger/pretty_printer_std_array.cc',
    'source_pretty_value_boundaries': 'test/0013.debugger/source_pretty_value_boundaries.cc',
    'source_cpp_references': 'test/0013.debugger/source_cpp_references.cc',
    'debug_source_pretty_coroutine': 'test/0017.runtime/debug_source_pretty_coroutine.cc',
    'source_print_commands': 'test/0013.debugger/source_print_commands.cc',
    'source_pretty_formatter': 'test/0013.debugger/source_pretty_formatter.cc',
    'source_pretty_registry': 'test/0013.debugger/source_pretty_registry.cc',
    'source_local_guest_offset': 'test/0018.debugger/source_local_guest_offset.cc',
}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dependency_pins(path):
    text = path.read_text().replace('\\\n', ' ')
    if ':' not in text:
        raise RuntimeError('compiler dependency file has no target')
    names = shlex.split(text.split(':', 1)[1])
    result = {str(Path(name).resolve(strict=True)): sha(name) for name in names}
    if not result:
        raise RuntimeError('compiler dependency closure is empty')
    return result



def admit():
    if os.uname().sysname != 'Linux':
        raise RuntimeError('Linux execution only; local editing does not authorize a local test')
    membership = Path('/proc/self/cgroup').read_text().strip()
    if membership != '0::' + CGROUP:
        raise RuntimeError('the existing 64 GiB cgroup is required')
    cg = Path('/sys/fs/cgroup' + CGROUP)
    actual = cg.stat()
    if (actual.st_dev, actual.st_ino) != (29, 19075):
        raise RuntimeError('existing cgroup device/inode identity changed')
    if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != BOOT:
        raise RuntimeError('existing host boot identity changed')
    if (cg / 'memory.max').read_text().strip() != '68719476736' or (cg / 'memory.swap.max').read_text().strip() != '0':
        raise RuntimeError('existing cgroup memory/swap limits changed')
    if (cg / 'cpuset.cpus.effective').read_text().strip() != '0-31':
        raise RuntimeError('existing cgroup CPU set changed')
    if str(os.getpid()) not in (cg / 'cgroup.procs').read_text().split():
        raise RuntimeError('current process not in verified cgroup')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return {'membership': membership, 'boot': BOOT, 'device': actual.st_dev, 'inode': actual.st_ino,
            'memory_max': 68719476736, 'swap_max': 0,
            'cpuset_cpus_effective': '0-31',
            'affinity': sorted(os.sched_getaffinity(0)), 'memory_events': (cg / 'memory.events').read_text()}


def elf(path, machine, bits, order):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF' + bytes([2 if bits == 64 else 1, 1 if order == 'little' else 2]):
        raise RuntimeError('ELF class/endianness differs from target')
    if struct.unpack_from(('<' if order == 'little' else '>') + 'H', data, 18)[0] != machine:
        raise RuntimeError('ELF machine differs from target')
    endian = '<' if order == 'little' else '>'
    header_size, minimum_entry = (64, 56) if bits == 64 else (52, 32)
    if len(data) < header_size:
        raise RuntimeError('truncated ELF header')
    phoff = struct.unpack_from(endian + ('Q' if bits == 64 else 'I'), data, 32 if bits == 64 else 28)[0]
    entsize, count = struct.unpack_from(endian + 'HH', data, 54 if bits == 64 else 42)
    if count == 65535 or count and (entsize < minimum_entry or phoff > len(data) or count > (len(data) - phoff) // entsize):
        raise RuntimeError('unsupported or truncated ELF program headers')
    stacks = []
    for index in range(count):
        start = phoff + index * entsize
        kind = struct.unpack_from(endian + 'I', data, start)[0]
        if kind == 0x6474e551:
            stacks.append(struct.unpack_from(endian + 'I', data, start + (4 if bits == 64 else 24))[0])
    if machine == 8 and (len(stacks) != 1 or stacks[0] & 1):
        raise RuntimeError('MIPS fixture requires one explicit non-executable GNU stack')
    return {'machine': machine, 'bits': bits, 'byte_order': order, 'bytes': len(data), 'sha256': sha(path),
            'gnu_stack_flags': stacks, 'non_executable_stack': len(stacks) == 1 and not bool(stacks[0] & 1)}


class CleanupFailure(RuntimeError):
    """A phase could not finish cleanup/recording; no further job is admitted."""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True, help='parent containing three source snapshots')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--clang', type=Path, default=Path('/usr/lib/llvm-22/bin/clang++'))
    parser.add_argument('--lld', type=Path, default=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/ld.lld'))
    parser.add_argument('--deps-root', type=Path, required=True)
    parser.add_argument('--provider-map', type=Path, help='optional profile-to-dependency-root JSON map')
    parser.add_argument('--qemu-root', type=Path, default=Path('/home/macromodel/.local/opt/qemu-10.2.1'))
    parser.add_argument('--profiles', default=','.join(PROFILES))
    parser.add_argument('--repositories', default='uwvm2,uwvm2-ros,uwvm2-stable')
    parser.add_argument('--maximum-seconds', type=int, default=7200)
    parser.add_argument('--iterations', type=int, default=1)
    parser.add_argument('--case', choices=CASES, default='pretty_printer_bench')
    parser.add_argument('--root-disk-reserve-bytes', type=int, default=DISK_RESERVE)
    parser.add_argument('--maximum-memory-bytes', type=int, default=2684354560,
                        help='allocated total existing-cgroup ceiling; stop 128MiB early, including tmpfs')
    args = parser.parse_args()
    if not 1 <= args.maximum_seconds <= 14400:
        parser.error('finite matrix window of at most4h required')
    selected = args.profiles.split(',')
    repos = args.repositories.split(',')
    if not selected or not set(selected) <= PROFILES.keys() or len(set(selected)) != len(selected):
        parser.error('each known profile must occur once')
    if not repos or not set(repos) <= {'uwvm2', 'uwvm2-ros', 'uwvm2-stable'} or len(set(repos)) != len(repos):
        parser.error('each known repository must occur once')
    if not 1 <= args.iterations <= 100000000:
        parser.error('iterations outside bounded range')
    if not MEMORY_RESERVE < args.maximum_memory_bytes <= 64 << 30:
        parser.error('allocated total ceiling must exceed 128MiB and remain within the existing 64GiB cgroup')
    providers = json.loads(args.provider_map.read_text()) if args.provider_map else {}
    if not isinstance(providers, dict) or not set(providers) <= PROFILES.keys():
        parser.error('provider-map must map known profiles to absolute roots')
    for value in providers.values():
        if not isinstance(value, str) or not Path(value).is_absolute():
            parser.error('provider roots must be absolute')
    if not 2 << 30 <= args.root_disk_reserve_bytes <= DISK_RESERVE:
        parser.error('root reserve must be2..3GiB; output reserve remains3GiB')
    proof = admit()
    sys.path.insert(0, str(args.arena / repos[0] / 'test/0050.high_level_languages'))
    import owned_process
    from owned_process import remember_process, wait_process, terminate_group, owned_process_record
    ownership_source = Path(owned_process.__file__).resolve(strict=True)
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    if args.root_disk_reserve_bytes < DISK_RESERVE:
        mounts = [line.split() for line in Path('/proc/self/mountinfo').read_text().splitlines()]
        on_tmpfs = any(len(row)>6 and row[4]=='/tmp' and row[row.index('-')+1]=='tmpfs' for row in mounts if '-' in row)
        if not on_tmpfs or args.out.stat().st_dev == Path('/').stat().st_dev:
            raise RuntimeError('reduced root reserve requires existing separate /tmp tmpfs')
    report = {'passed': False, 'scope': 'production pretty-printer copied DATA component; not full product CLI parity',
              'admission': proof, 'rows': [], 'cleanup_errors': [],
              'debug_languages_enabled': list(DEBUG_LANGUAGES), 'interpreted_language_bridges_enabled': [],
              'inputs': {str(Path(__file__).resolve()): sha(__file__), str(ownership_source): sha(ownership_source)},
              'maximum_memory_bytes': args.maximum_memory_bytes,
              'memory_stop_threshold_bytes': args.maximum_memory_bytes - MEMORY_RESERVE,
              'reactive_memory_reserve_bytes': MEMORY_RESERVE, 'delegated_memory_cap': False,
              'disk_reserve_bytes': DISK_RESERVE, 'root_disk_reserve_bytes': args.root_disk_reserve_bytes,
              'observed_memory_peak_bytes': 0}
    started = time.monotonic()

    def budget():
        if time.monotonic() - started >= args.maximum_seconds:
            raise TimeoutError('allocated matrix window expired')
        current = int(Path('/sys/fs/cgroup' + CGROUP + '/memory.current').read_text())
        report['observed_memory_peak_bytes'] = max(report['observed_memory_peak_bytes'], current)
        root_free, output_free = shutil.disk_usage('/').free, shutil.disk_usage(args.out).free
        report['last_disk_observation'] = {'rootfs_free_bytes': root_free, 'outputfs_free_bytes': output_free}
        if current >= report['memory_stop_threshold_bytes']:
            raise RuntimeError('total cgroup memory.current reached the reactive stop threshold 128MiB below the allocated ceiling')
        if output_free < DISK_RESERVE or root_free < args.root_disk_reserve_bytes:
            raise RuntimeError('root/output free disk below the separately recorded reserves')

    def publish():
        report['seconds'] = time.monotonic() - started
        temp = args.out / 'results.json.tmp'
        temp.write_text(json.dumps(report, indent=2) + '\n')
        temp.replace(args.out / 'results.json')

    def run(row, phase, command, log, timeout, environment=None):
        before = time.monotonic()
        child = None
        entry = {'phase': phase, 'argv': list(map(str, command)), 'log': str(log), 'cleanup_errors': []};row['phases'].append(entry)
        try:
            with log.open('wb') as stream:
                budget()
                child = subprocess.Popen(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT,
                                         start_new_session=True, env=environment)
                remember_process(child)
                code = wait_process(child, timeout, observe=budget)
                entry['exit'] = code
        except Exception as error:
            entry['error'] = type(error).__name__ + ': ' + str(error)
            raise
        finally:
            # Each diagnostic is independent: an undrained owner must not erase
            # the available ownership/log/timing evidence or start another job.
            try:
                terminate_group(child)
                entry['cleanup_outcome'] = 'helper_returned' if child is not None else 'no_child_started'
            except BaseException as error:
                entry['cleanup_outcome'] = 'drain_unconfirmed'
                entry['cleanup_errors'].append({'operation': 'terminate_group', 'error': type(error).__name__ + ': ' + str(error)})
            try:
                entry['process_ownership'] = owned_process_record(child)
            except BaseException as error:
                entry['cleanup_errors'].append({'operation': 'owned_process_record', 'error': type(error).__name__ + ': ' + str(error)})
            try:
                entry['sha256'] = sha(log)
            except BaseException as error:
                entry['cleanup_errors'].append({'operation': 'log_sha256', 'error': type(error).__name__ + ': ' + str(error)})
            try:
                entry['seconds'] = time.monotonic() - before
            except BaseException as error:
                entry['cleanup_errors'].append({'operation': 'elapsed_seconds', 'error': type(error).__name__ + ': ' + str(error)})
            if entry['cleanup_errors']:
                raise CleanupFailure(phase + ' cleanup/recording failed; no further jobs admitted')
        if code:
            raise RuntimeError(phase + ' failed; see ' + str(log))

    for profile in selected:
        triple, emulator, machine, bits, order, flags = PROFILES[profile]
        for repo in repos:
            if repo == 'uwvm2-ros' and profile not in ROS_PROFILES:
                continue
            row = {'profile': profile, 'repository': repo, 'case': args.case, 'passed': False, 'phases': []}
            report['rows'].append(row)
            directory = args.out / profile / repo
            directory.mkdir(parents=True)
            binary = directory / 'pretty_printer.elf'
            try:
                admit()
                source = args.arena / repo
                fixture = source / CASES[args.case]
                dependency_root = Path(providers.get(profile, str(args.deps_root)))
                sysroot = dependency_root / 'usr' / triple
                gcc_root = dependency_root / 'usr/lib/gcc-cross' / triple
                versions = sorted((p for p in gcc_root.glob('*') if p.is_dir() and p.name.isdecimal()),
                                  key=lambda p: int(p.name))
                gcc = versions[-1] if versions else gcc_root / '15'
                if profile == 'x86_64':
                    sysroot, gcc = Path('/'), Path('/usr/lib/gcc/x86_64-linux-gnu/15')
                qemu = args.qemu_root / 'usr/bin' / ('qemu-' + emulator)
                loader = args.qemu_root / 'usr/lib64/ld-linux-x86-64.so.2'
                for path in (fixture, args.clang, args.lld, sysroot, gcc, qemu, loader):
                    if not path.exists():
                        raise FileNotFoundError('missing actual provider: ' + str(path))
                linker = dependency_root / 'usr/bin' / (triple + '-ld') if profile in {'ppc64', 'sparc64', 'mips64'} else args.lld
                report['inputs'].update({str(p.resolve()): sha(p) for p in (fixture, args.clang, linker, qemu, loader)})
                command = [args.clang, '--target=' + triple,
                           '--sysroot=' + ('/' if profile == 'x86_64' else str(dependency_root)),
                           '--gcc-install-dir=' + str(gcc), '-idirafter', sysroot / 'include',
                           *['-DUWVM_DEBUG_LANGUAGE_' + name + '=1' for name in DEBUG_LANGUAGES],
                           '-std=c++26', '-stdlib=libstdc++', '-O2', '-g1', '-fno-rtti',
                           '-fasynchronous-unwind-tables', *flags,
                           *(['-DUWVM=2', '-DUWVM_DISABLE_INT=1', '-DUWVM_DISABLE_JIT=1'] if args.case in ('source_pretty_formatter', 'pretty_printer_text_display') else []),
                           '--ld-path=' + str(linker), '-pthread',
                           '-MD', '-MF', directory / 'fixture.d',
                           *['-I' + str(source / p) for p in ('src', 'third-parties/fast_io/include', 'third-parties/bizwen/include', 'third-parties/boost_unordered/include')], fixture]
                if profile == 'mips64':
                    # Signed target CRT objects request an executable stack.
                    # GNU ld honors explicit noexecstack; verify the final ELF.
                    command += ['-Wl,-z,noexecstack']
                if profile != 'x86_64':
                    command += ['-L' + str(sysroot / 'lib'), '-L' + str(dependency_root / 'usr/lib' / triple)]
                command += ['-latomic', '-o', binary]
                native_libraries = [args.lld.parent.parent / 'lib/x86_64-unknown-linux-gnu', args.lld.parent.parent / 'lib']
                if linker != args.lld:
                    native_libraries.insert(0, dependency_root / 'usr/lib/x86_64-linux-gnu')
                environment = dict(os.environ, TMPDIR=str(directory), LD_LIBRARY_PATH=':'.join(str(p) for p in native_libraries if p.is_dir()))
                # The preprocessor and actual compiler must consume the same
                # resolved Source/provider bytes. Post-build MD alone cannot
                # establish which bytes were read during compilation.
                dependency_command = list(command[:-3])
                dependency_command[dependency_command.index('-MD')] = '-M'
                dependency_command[dependency_command.index('-MF') + 1] = directory / 'fixture-before.d'
                dependency_command += ['-MT', binary]
                run(row, 'dependencies-before', dependency_command, directory / 'dependencies.log', 240, environment)
                row['dependency_hashes_before'] = dependency_pins(directory / 'fixture-before.d')
                run(row, 'compile-link', command, directory / 'compile.log', 240, environment)
                actual_dependencies = dependency_pins(directory / 'fixture.d')
                if row['dependency_hashes_before'] != actual_dependencies:
                    raise RuntimeError('actual Source/header/provider MD closure changed during compilation')
                row['dependencies_before_equals_after'] = True
                row['elf'] = elf(binary, machine, bits, order)
                target_libraries = ([Path('/usr/lib/x86_64-linux-gnu'), gcc] if profile == 'x86_64' else
                                    [dependency_root / 'usr/lib' / triple, sysroot / 'lib', sysroot / 'lib64', gcc])
                qemu_libraries = ':'.join(str(args.qemu_root / p) for p in ('usr/lib/x86_64-linux-gnu', 'lib/x86_64-linux-gnu', 'usr/lib64', 'lib64'))
                target = [loader, '--library-path', qemu_libraries, qemu, '-U', 'LD_LIBRARY_PATH',
                          '-E', 'LD_LIBRARY_PATH=' + ':'.join(map(str, target_libraries)), '-L', sysroot,
                          binary]
                if args.case == 'pretty_printer_bench':
                    target += ['--iterations', str(args.iterations)]
                run(row, 'qemu-execute', target, directory / 'qemu.log', 180)
                if args.case == 'pretty_printer_bench':
                    row['component_result'] = json.loads((directory / 'qemu.log').read_text())
                    if row['component_result'].get('passed') is not True:
                        raise RuntimeError('component did not report passed')
                else:
                    row['component_output'] = (directory / 'qemu.log').read_text()
                row['dependency_hashes'] = dependency_pins(directory / 'fixture.d')
                if row['dependency_hashes'] != row['dependency_hashes_before']:
                    raise RuntimeError('compiled dependency bytes changed during QEMU execution')
                row['passed'] = True
            except CleanupFailure as error:
                row['passed'] = False
                row['error'] = type(error).__name__ + ': ' + str(error)
                row['cleanup_errors'] = [entry for entry in row['phases'] if entry.get('cleanup_errors')]
                report['passed'] = False
                report['abort_reason'] = 'phase_cleanup_or_recording_failed'
                report['cleanup_errors'].append({'profile': profile, 'repository': repo, 'phases': row['cleanup_errors']})
                report['passed_cases'] = sum(item['passed'] for item in report['rows'])
                report['completed_cases'] = len(report['rows'])
                try:
                    report['admission_after'] = admit()
                except Exception as admission_error:
                    report['admission_after_error'] = type(admission_error).__name__ + ': ' + str(admission_error)
                try:
                    publish()
                except Exception as receipt_error:
                    report['receipt_error'] = type(receipt_error).__name__ + ': ' + str(receipt_error)
                    print(json.dumps(report), file=sys.stderr, flush=True)
                print(json.dumps({key: row.get(key) for key in ('profile', 'repository', 'passed', 'error', 'cleanup_errors')}), flush=True)
                return 1
            except Exception as error:
                row['error'] = type(error).__name__ + ': ' + str(error)
            publish()
            print(json.dumps({key: row.get(key) for key in ('profile', 'repository', 'passed', 'error')}), flush=True)
    for path, digest in report['inputs'].items():
        if sha(path) != digest:
            raise RuntimeError('input changed during run: ' + path)
    for row in report['rows']:
        for path, digest in row.get('dependency_hashes', {}).items():
            if sha(path) != digest:
                raise RuntimeError('dependency changed during run: ' + path)
    report['admission_after'] = admit()
    report['passed'] = bool(report['rows']) and all(row['passed'] for row in report['rows'])
    report['passed_cases'] = sum(row['passed'] for row in report['rows'])
    report['completed_cases'] = len(report['rows'])
    publish()
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
