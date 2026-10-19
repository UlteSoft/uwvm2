#!/usr/bin/env python3
"""Isolated O2 minimal-builtin debug fixture; full O2 artifacts stay unchanged."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import sys
import tarfile

from build import make_vars
from inspect_function_shape import shape
from linux_control import digest, guard, invoke


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--wasmtime', required=True, type=Path)
    parser.add_argument('--jobs', type=int, choices=[2, 4], default=4)
    parser.add_argument('--cpus', default='4,5,6,7')
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=False)
    before = guard(args.work)
    os.environ['UWVM_PYTHON_BASE_FREE'] = str(before['disk_free'])
    cpus = {int(v) for v in args.cpus.split(',')}
    assert cpus <= os.sched_getaffinity(0)
    os.sched_setaffinity(0, cpus)
    own = Path(__file__).resolve().parent
    original_path = args.original / 'build-receipt.json'
    original = json.loads(original_path.read_text())
    assert original['passed']
    originals = {value['path']: value['sha256'] for value in original['artifacts'].values()}
    for path, expected in originals.items():
        assert digest(path) == expected
    report = {'passed': False, 'before': before,
              'original_receipt': str(original_path), 'original_receipt_sha256': digest(original_path),
              'configuration': 'C-O2 -g debug fixture with 35 optional builtins disabled; '
                               'unchanged CPython feature/assertion macros and Python bytecode optimize0',
              'optimization': 'C-O2 -g debug-minimal Python-bytecode-optimize0',
              'debug_scope': 'Wasm/native CPython debug fixture; no full-stdlib or full-O2-module DBG claim',
              'inputs': {p.name: digest(p) for p in [own / 'debug_minimal_build.py',
                         own / 'embedded_main.c', own / 'semantics.py', own / 'hll_common.py']},
              'commands': [], 'original_artifacts': originals}
    output_receipt = args.work / 'build-receipt.json'

    def save():
        output_receipt.write_text(json.dumps(report, indent=2) + '\n')

    def run(name, argv, **kwargs):
        row = invoke(argv, args.work / 'commands' / name, **kwargs)
        report['commands'].append(row)
        save()
        return row

    save()
    try:
        archive = next(Path(row['path']) for row in original['downloads']
                       if row['path'].endswith('Python-3.14.8.tar.xz'))
        expected = original['versions']['cpython']['source_sha256']
        assert digest(archive) == expected
        report['source_archive'] = {'path': str(archive), 'sha256': expected}
        sources = args.work / 'sources'
        sources.mkdir()
        with tarfile.open(archive) as stream:
            stream.extractall(sources, filter='data')
        source = sources / 'Python-3.14.8'
        native = Path(original['native_python']['path'])
        assert digest(native) == original['native_python']['sha256']
        cross = source / 'cross-build'
        cross.mkdir()
        # Official configure-host reuses its fixed build-python directory; the
        # native tree is immutable and already qualified, so link only that tree.
        (cross / native.parent.name).symlink_to(native.parent, target_is_directory=True)
        setup_stdlib = Path(original['wasi_build']) / 'Modules/Setup.stdlib'
        disabled = [line.split()[0] for line in setup_stdlib.read_text().splitlines()
                    if line.strip() and not line.lstrip().startswith(('#', '*'))]
        assert len(disabled) == 35 and len(set(disabled)) == 35
        setup_local = source / 'Modules/Setup.local'
        setup_local.write_text('# Task-owned debug fixture profile; official Setup syntax.\n'
                               '*disabled*\n' + '\n'.join(disabled) + '\n')
        report['disabled_builtins'] = disabled
        report['original_setup_stdlib_sha256'] = digest(setup_stdlib)
        report['setup_local'] = {'path': str(setup_local), 'sha256': digest(setup_local)}
        sdk_clang = Path(original['sdk_clang']['path'])
        assert digest(sdk_clang) == original['sdk_clang']['sha256']
        sdk = sdk_clang.parent.parent
        env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON')}
        env.update(PATH=str(args.wasmtime.parent) + ':' + env['PATH'],
                   WASI_SDK_PATH=str(sdk), SOURCE_DATE_EPOCH='1790726400',
                   CFLAGS='-O2 -g', PYTHONHASHSEED='0')
        run('configure-wasi', [sys.executable, source / 'Tools/wasm/wasi', 'configure-host',
                              '--wasi-sdk', sdk, '--', '--without-ensurepip',
                              '--disable-test-modules'], cwd=source, env=env)
        wasi = cross / 'wasm32-wasip1'
        # VPATH makesetup consumes the BUILD directory's Setup.local. Configure
        # creates an empty one; a source-tree file alone does not override it.
        build_setup_local = wasi / 'Modules/Setup.local'
        build_setup_local.write_bytes(setup_local.read_bytes())
        report['build_setup_local'] = {'path': str(build_setup_local),
                                       'sha256': digest(build_setup_local)}
        run('make-wasi', ['make', '-C', wasi, '-j', str(args.jobs), 'all'], env=env)
        for path in ['pyconfig.h', 'Makefile', 'config.log']:
            report.setdefault('configuration_files', {})[path] = digest(wasi / path)
        # Record macro state explicitly. Do not add --with-pydebug or change GC.
        macro_names = {'Py_DEBUG', 'Py_TRACE_REFS', 'Py_GIL_DISABLED', 'WITH_PYMALLOC',
                       'WITH_MIMALLOC', 'NDEBUG'}
        report['macro_lines'] = {}
        for label, directory in [('original', Path(original['wasi_build'])), ('debug', wasi)]:
            lines = (directory / 'pyconfig.h').read_text().splitlines()
            report['macro_lines'][label] = [line for line in lines
                if any(('define ' + name + ' ') in line or ('undef ' + name + ' ') in line
                       for name in macro_names)]
        assert report['macro_lines']['original'] == report['macro_lines']['debug']
        config = wasi / 'Modules/config.c'
        original_config = Path(original['wasi_build']) / 'Modules/config.c'
        builtin_pattern = r'\{"([^"]+)",\s*([^}]+)\}'
        original_builtins = dict(re.findall(builtin_pattern, original_config.read_text()))
        actual_builtins = dict(re.findall(builtin_pattern, config.read_text()))
        assert actual_builtins == {k: v for k, v in original_builtins.items() if k not in disabled}
        report['builtin_table'] = actual_builtins
        report['config_c'] = {'path': str(config), 'sha256': digest(config)}
        report['bootstrap_setup'] = {'path': str(wasi / 'Modules/Setup.bootstrap'),
                                    'sha256': digest(wasi / 'Modules/Setup.bootstrap')}
        frozen = args.work / 'frozen-semantics'
        shutil.copytree(args.original / 'frozen-semantics', frozen)
        frozen_header = frozen / 'hll_frozen.h'
        assert digest(frozen_header) == digest(args.original / 'frozen-semantics/hll_frozen.h')
        report['frozen_header'] = {'path': str(frozen_header), 'sha256': digest(frozen_header),
                                   'identical_to_qualified_O2': True}
        variables = make_vars(wasi / 'Makefile')
        report['link_variables'] = variables
        artifact_dir = args.work / 'artifacts'
        artifact_dir.mkdir()
        output = artifact_dir / 'python-semantics-debug-minimal-O2.wasm'
        command = shlex.split(variables.get('LINKCC', variables['CC']))
        command += shlex.split(variables.get('PY_CORE_LDFLAGS', ''))
        command += ['-O2', '-g', '-I' + str(wasi), '-I' + str(source / 'Include'),
                    '-I' + str(frozen), str(own / 'embedded_main.c'),
                    str(wasi / variables['LIBRARY'])]
        for key in ['LIBS', 'MODLIBS', 'SYSLIBS', 'LINKFORSHARED']:
            command += shlex.split(variables.get(key, ''))
        command += ['-o', str(output)]
        run('link-semantics-wasi', command, cwd=wasi, env=env)
        report['source'] = str(source)
        report['wasi_build'] = str(wasi)
        report['artifact'] = {'path': str(output), 'sha256': digest(output),
                              'bytes': output.stat().st_size, 'external_runtime_assets': [],
                              'preopens': []}
        report['function_shape'] = shape(output)
        for path, expected in originals.items():
            assert digest(path) == expected, 'original artifact changed'
        report['original_artifacts_unchanged'] = True
        report['passed'] = True
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        report['after'] = guard(args.work)
        save()


if __name__ == '__main__':
    main()
