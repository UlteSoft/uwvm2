#!/usr/bin/env python3
"""Isolated C-O0 CPython WASI build; original O2 artifacts stay unchanged."""
import argparse
import json
import os
from pathlib import Path
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
              'configuration': 'C-O0 -g, unchanged CPython feature/assertion macros; Python bytecode optimize0',
              'inputs': {p.name: digest(p) for p in [own / 'debug_build.py',
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
        shutil.copy2(Path(original['source']) / 'Modules/Setup.local', source / 'Modules/Setup.local')
        sdk_clang = Path(original['sdk_clang']['path'])
        assert digest(sdk_clang) == original['sdk_clang']['sha256']
        sdk = sdk_clang.parent.parent
        env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON')}
        env.update(PATH=str(args.wasmtime.parent) + ':' + env['PATH'],
                   WASI_SDK_PATH=str(sdk), SOURCE_DATE_EPOCH='1790726400',
                   CFLAGS='-O0 -g', PYTHONHASHSEED='0')
        run('configure-wasi', [sys.executable, source / 'Tools/wasm/wasi', 'configure-host',
                              '--wasi-sdk', sdk, '--', '--without-ensurepip',
                              '--disable-test-modules'], cwd=source, env=env)
        wasi = cross / 'wasm32-wasip1'
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
        output = artifact_dir / 'python-semantics-debug-O0.wasm'
        command = shlex.split(variables.get('LINKCC', variables['CC']))
        command += shlex.split(variables.get('PY_CORE_LDFLAGS', ''))
        command += ['-O0', '-g', '-I' + str(wasi), '-I' + str(source / 'Include'),
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
