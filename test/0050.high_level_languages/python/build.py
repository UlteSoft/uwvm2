#!/usr/bin/env python3
"""Pinned real CPython native/WASI producer; Linux-only, no UWVM execution.

Official configure helpers are unchanged. Make jobs are explicitly bounded.
Build receipts preserve commands, inputs and native/WASI/frozen asset hashes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import re
import shutil
import sys
import sysconfig
import tarfile
import time
import urllib.request

from linux_control import guard, digest, invoke

INPUT_DIRECTORY = None


def acquire(url, destination, expected=None):
    if destination.exists():
        actual = digest(destination)
        if expected is not None and actual != expected:
            raise RuntimeError(f'existing archive SHA mismatch: {destination}')
        return {'url': url, 'path': str(destination), 'bytes': destination.stat().st_size,
                'sha256': actual, 'expected_sha256': expected, 'reused': True}
    incoming = INPUT_DIRECTORY / destination.name if INPUT_DIRECTORY else None
    if incoming is not None and incoming.exists():
        # Static acquisition may occur on the local host; all extraction,
        # bytecode production, compilation and execution remain on Linux.
        os.link(incoming, destination)
    else:
        # urllib body reads were reset on this host; original failed attempts
        # remain separate. Curl accesses the same official archive URL.
        invoke(['curl', '--fail', '--location', '--http1.1', '--retry', '3',
                '--retry-all-errors', '--connect-timeout', '15', '--max-time', '240',
                '--output', destination, url],
               destination.parent / (destination.name + '.transfer'), timeout=1000)
    actual = digest(destination)
    if expected is not None and actual != expected:
        raise RuntimeError(f'archive SHA mismatch: {destination}')
    return {'url': url, 'path': str(destination), 'bytes': destination.stat().st_size,
            'sha256': actual, 'expected_sha256': expected,
            'transferred_static_input': str(incoming) if incoming is not None and incoming.exists() else None}


def unpack(archive, destination):
    marker = destination.with_name(destination.name + '.unpacked.json')
    if marker.exists():
        assert json.loads(marker.read_text())['archive_sha256'] == digest(archive)
        return
    destination.mkdir()
    with tarfile.open(archive) as stream:
        stream.extractall(destination, filter='data')
    marker.write_text(json.dumps({'archive_sha256': digest(archive)}) + '\n')


def make_vars(path):
    # CPython3.14 removed sysconfig._parse_makefile. These link-recipe variables
    # use ordinary generated Makefile assignments and recursive $(NAME) refs.
    # Fail rather than silently inventing a library list or expanding a make function.
    text = path.read_text().replace('\\\n', '')
    raw = {}
    for line in text.splitlines():
        match = re.match(r'^([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*)$', line)
        if match:
            raw[match[1]] = match[2].strip()
    variable = re.compile(r'\$\(([A-Za-z_][A-Za-z_0-9]*)\)|\$\{([A-Za-z_][A-Za-z_0-9]*)\}')
    def resolve(name, stack=()):
        if name in stack:
            raise RuntimeError('recursive Makefile link variable: ' + name)
        value = raw.get(name, os.environ.get(name, ''))
        value = variable.sub(lambda m: resolve(m[1] or m[2], (*stack, name)), value)
        if '$' in value:
            raise RuntimeError('unsupported Makefile syntax for link variable: ' + name)
        return value
    return {name: resolve(name) for name in ['CC', 'LINKCC', 'PY_CORE_LDFLAGS',
            'LIBRARY', 'LIBS', 'MODLIBS', 'SYSLIBS', 'LINKFORSHARED']}


def main():
    global INPUT_DIRECTORY
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--wasmtime', required=True, type=Path)
    parser.add_argument('--jobs', type=int, choices=[2, 4], default=4)
    parser.add_argument('--cpus', default='4,5,6,7')
    parser.add_argument('--input-dir', type=Path)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    own = Path(__file__).resolve().parent
    INPUT_DIRECTORY = args.input_dir
    args.work.mkdir(parents=True, exist_ok=args.resume)
    before = guard(args.work)
    os.environ['UWVM_PYTHON_BASE_FREE'] = str(before['disk_free'])
    cpus = {int(value) for value in args.cpus.split(',')}
    assert cpus <= os.sched_getaffinity(0)
    os.sched_setaffinity(0, cpus)
    versions = json.loads((own / 'versions.json').read_text())
    downloads, sources, logs = [args.work / n for n in ['downloads', 'sources', 'commands']]
    for p in [downloads, sources, logs]:
        p.mkdir(exist_ok=args.resume)
    report = {'passed': False, 'before': before, 'versions': versions,
              'inputs': {str(p): digest(p) for p in own.iterdir() if p.is_file()},
              'commands': [], 'downloads': [], 'artifacts': {}}
    report_path = args.work / 'build-receipt.json'

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    def run(name, argv, *, cwd=None, env=None, timeout=1800):
        folder = logs / name
        if args.resume and (folder / 'receipt.json').exists():
            old = json.loads((folder / 'receipt.json').read_text())
            if old.get('passed') and old.get('retired') and old['argv'] == list(map(str, argv)):
                report['commands'].append(old | {'reused_verified_command': True})
                save()
                return old
        attempt = 1
        while folder.exists():
            attempt += 1
            folder = logs / (name + f'-attempt-{attempt}')
        row = invoke(argv, folder, cwd=cwd, env=env, timeout=timeout)
        report['commands'].append(row)
        save()
        return row

    if args.resume and report_path.exists():
        index = 1
        while (args.work / f'build-receipt-attempt-{index}.json').exists():
            index += 1
        shutil.copy2(report_path, args.work / f'build-receipt-attempt-{index}.json')
    save()
    try:
        python_archive = downloads / 'Python-3.14.8.tar.xz'
        report['downloads'].append(acquire(versions['cpython']['source_url'], python_archive,
                                           versions['cpython']['source_sha256']))
        if not (args.resume and (sources / 'python').is_dir()):
            unpack(python_archive, sources / 'python')
        source = sources / 'python/Python-3.14.8'
        assert '#define PY_VERSION' in (source / 'Include/patchlevel.h').read_text()
        sdk = sources / 'sdk/wasi-sdk-24.0-x86_64-linux'
        env = dict(os.environ)
        env.update(PATH=str(args.wasmtime.parent) + ':' + env['PATH'],
                   WASI_SDK_PATH=str(sdk), SOURCE_DATE_EPOCH='1790726400',
                   CFLAGS='-O2 -g', PYTHONHASHSEED='0')
        for key in list(env):
            if key.startswith('PYTHON') and key != 'PYTHONHASHSEED':
                del env[key]
        helper = source / 'Tools/wasm/wasi'
        run('configure-native', [sys.executable, helper, 'configure-build-python', '--',
                                '--without-ensurepip', '--disable-test-modules'], cwd=source, env=env)
        native_dir = source / 'cross-build' / sysconfig.get_config_var('BUILD_GNU_TYPE')
        run('make-native', ['make', '-C', native_dir, '-j', str(args.jobs), 'all'], env=env)
        native_python = native_dir / 'python'
        run('native-version', [native_python, '--version'], env=env)
        sdk_metadata = downloads / 'wasi-sdk-release.json'
        report['downloads'].append(acquire(versions['wasi_sdk']['release_api'], sdk_metadata))
        release = json.loads(sdk_metadata.read_text())
        asset = next(item for item in release['assets'] if item['name'] == versions['wasi_sdk']['asset'])
        assert asset['browser_download_url'] == versions['wasi_sdk']['asset_url']
        sdk_archive = downloads / asset['name']
        published_digest = asset.get('digest')
        expected_sdk = published_digest.removeprefix('sha256:') if published_digest else None
        report['sdk_release_asset'] = asset
        report['downloads'].append(acquire(asset['browser_download_url'], sdk_archive, expected_sdk))
        if not (args.resume and sdk.is_dir()):
            unpack(sdk_archive, sources / 'sdk')
        assert sdk.is_dir()
        run('sdk-version', [sdk / 'bin/clang', '--version'])
        run('configure-wasi', [sys.executable, helper, 'configure-host', '--wasi-sdk', sdk,
                              '--', '--without-ensurepip', '--disable-test-modules'], cwd=source, env=env)
        wasi_dir = source / 'cross-build/wasm32-wasip1'
        run('make-wasi', ['make', '-C', wasi_dir, '-j', str(args.jobs), 'all'], env=env)
        artifacts = args.work / 'artifacts'
        artifacts.mkdir(exist_ok=args.resume)
        # The producer retains the unmodified standard command for baseline import/CLI inspection.
        shutil.copy2(wasi_dir / 'python.wasm', artifacts / 'cpython-3.14.8-standard.wasm')
        report['native_python'] = {'path': str(native_python), 'sha256': digest(native_python)}
        report['sdk_clang'] = {'path': str(sdk / 'bin/clang'), 'sha256': digest(sdk / 'bin/clang')}
        report['source'] = str(source)
        report['wasi_build'] = str(wasi_dir)
        for entry in ['semantics', 'timed', 'profile']:
            frozen = args.work / ('frozen-' + entry)
            run('freeze-' + entry, [native_python, own / 'freeze_fixture.py', '--cpython-source', source,
                                   '--entry', entry, '--out', frozen], env=env)
            for target, directory in [('native', native_dir), ('wasi', wasi_dir)]:
                variables = make_vars(directory / 'Makefile')
                output = artifacts / ('python-' + entry + ('.wasm' if target == 'wasi' else '.native'))
                command = shlex.split(variables.get('LINKCC', variables['CC']))
                command += shlex.split(variables.get('PY_CORE_LDFLAGS', ''))
                command += ['-O2', '-g', '-I' + str(directory), '-I' + str(source / 'Include'),
                            '-I' + str(frozen), str(own / 'embedded_main.c'),
                            str(directory / variables['LIBRARY'])]
                for key in ['LIBS', 'MODLIBS', 'SYSLIBS', 'LINKFORSHARED']:
                    command += shlex.split(variables.get(key, ''))
                command += ['-o', str(output)]
                run('link-' + entry + '-' + target, command, cwd=directory, env=env)
                report['artifacts'][entry + '-' + target] = {'path': str(output),
                    'sha256': digest(output), 'bytes': output.stat().st_size,
                    'frozen_assets': str(frozen / 'frozen-assets.json'),
                    'frozen_assets_sha256': digest(frozen / 'frozen-assets.json'),
                    'external_runtime_assets': [], 'preopens': [], 'qualification': 'not yet executed'}
        # Optional full stdlib is independent of zero-preopen base fixtures. Keep source paths
        # and hashes rather than copying Lib into every output or inheriting a root mount.
        report['stdlib_source_assets'] = [{'path': str(p.relative_to(source)), 'bytes': p.stat().st_size,
                                          'sha256': digest(p)} for p in sorted((source / 'Lib').rglob('*.py'))]
        report['passed'] = True
        report['after'] = guard(args.work)
        save()
    except BaseException as error:
        report['error'] = repr(error)
        report['after'] = guard(args.work)
        save()
        raise


if __name__ == '__main__':
    main()
