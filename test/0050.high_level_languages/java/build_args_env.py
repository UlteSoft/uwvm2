#!/usr/bin/env python3
"""Build optional TeaVM WASI environment adaptation and strict reference oracles."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

from build_dwarf_fixed import CORE, JAR, SOURCES, ORIGINAL_JAR_SHA256, sha


ORIGINAL_WASM = {
    'wasi/java-semantics.wasm': '356043bf711df30ec65745f9221a6606b28819fd6528844d57115f532277fe95',
    'wasi-debug/java-semantics.wasm': 'dde9d9545d89238f40a826bd60cf0eda422da4b012d9f29aafcb6d16e813d24c',
    'perf/java-perf.wasm': 'b88646a578405b9064cbcd9211e1755ef693bbb8c93188109cc595c371669b3b',
    'known-gap/java-implicit-arithmetic.wasm': '09648c26a2cca6f6da55b8758696a0de7b4f522ee1c130e5bab55244cdf030a6',
    'wasm-gc/java-semantics.wasm': '0c02dc7eb478ecc6e10e663ae6b74ca7f059660e5794f40383beab57714a6cd6',
    'perf-timed/java-perf-timed.wasm': '52505d5526e09f85c7e4b7e9781f38aabcbe7c3c35c3ca7642a280572e6757ee',
    'perf-profile-gated/java-perf-profile.wasm': '390f6195be3fc3443b7daa76f9568124b1583fc381881f7ae2ce38962e688933',
}
VALUES = {'UWVM_HLL_EMPTY': '', 'UWVM_HLL_UTF8': '雪=🙂', 'UWVM_HLL_EQUALS': 'a=b=c'}
ARGUMENTS = [[], ['', '雪=🙂', 'a=b=c', '=']]
MAP_LINE = 'java env map: cached=true readonly=true named=true null-name=true null-key=true null-value=true wrong-type=true\n'


def imports(path):
    data, at = Path(path).read_bytes(), 8
    def leb():
        nonlocal at
        result = shift = 0
        while True:
            byte = data[at]
            at += 1
            result |= (byte & 127) << shift
            if not byte & 128:
                return result
            shift += 7
            assert shift <= 35
    def string():
        nonlocal at
        length = leb()
        result = data[at:at + length].decode()
        at += length
        return result
    while at < len(data):
        kind = data[at]
        at += 1
        size = leb()
        end = at + size
        if kind == 2:
            rows = []
            for _ in range(leb()):
                module, name = string(), string()
                assert data[at] == 0, 'this Java fixture should import only functions'
                at += 1
                rows.append({'module': module, 'name': name, 'type_index': leb()})
            assert at == end
            return rows
        at = end
    return []


def describe(name, value):
    if value is None:
        return name + '=NULL\n'
    raw = value.encode('utf-16-le')
    units = [str(int.from_bytes(raw[i:i + 2], 'little')) for i in range(0, len(raw), 2)]
    return name + '=len:' + str(len(units)) + ',utf16:' + ','.join(units) + '\n'


def expected(arguments, environment, adapted):
    text = f'java args/env: argc={len(arguments)}\n'
    text += ''.join(describe(f'arg[{i}]', value) for i, value in enumerate(arguments))
    text += ''.join(describe('env[' + key + ']', environment.get(key))
                    for key in [*VALUES, 'UWVM_HLL_MISSING'])
    return text + (MAP_LINE if adapted else '')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--maven', type=Path, required=True)
    parser.add_argument('--original-m2', type=Path, required=True)
    parser.add_argument('--patched-m2', type=Path, required=True)
    parser.add_argument('--toolchain-receipt', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output-tag', default='', help='Optional new output suffix, preserving earlier produced artifacts')
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    assert re.fullmatch(r'(?:-[A-Za-z0-9_.-]+)?', args.output_tag)
    assert sys.platform == 'linux' and os.sched_getaffinity(0) == {4, 5}
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission
    guard = admission()
    project = args.project.resolve(strict=True)
    original, patched = args.original_m2.resolve(strict=True), args.patched_m2.resolve(strict=True)
    toolchain = json.loads(args.toolchain_receipt.read_text())
    assert toolchain['passed'] and toolchain['wasi_environ_mapping']
    assert sha(original / CORE / JAR) == ORIGINAL_JAR_SHA256
    assert sha(patched / CORE / JAR) == toolchain['patched_jar_sha256']
    for path, value in ORIGINAL_WASM.items():
        assert sha(project / 'target' / path) == value, 'original fixture changed: ' + path
    preserved = {str(path): sha(path) for path in (project / 'target').rglob('*.wasm')}
    for directory in ['perf', 'perf-timed', 'perf-profile-gated']:
        for path in (project / 'target' / directory).rglob('*'):
            if path.is_file():
                preserved[str(path)] = sha(path)
    for path in (project / 'src/main/java').rglob('*.java'):
        preserved[str(path)] = sha(path)
    for name in ['perf-timed-cases.json', 'perf-profile-cases.json']:
        preserved[str(project / name)] = sha(project / name)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    record = {'passed': False, 'optional_tooling_only': True, 'admission_before': guard,
              'runtime_or_java_mode_flag_added': False, 'uwvm_run': False,
              'original_compiler_sha256': ORIGINAL_JAR_SHA256,
              'patched_compiler_sha256': toolchain['patched_jar_sha256'],
              'toolchain_receipt_sha256': sha(args.toolchain_receipt),
              'classlib_sha256': sha(original / 'org/teavm/teavm-classlib/0.13.1/teavm-classlib-0.13.1.jar'),
              'builder_sha256': sha(__file__), 'preserved_before': preserved,
              'commands': [], 'reference_runs': []}

    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')

    def run(command, name, environment=None):
        command = list(map(str, command))
        row = {'argv': command, 'cwd': str(work), 'log': name,
               'launcher_environment': environment if environment is not None else 'inherited'}
        record['commands'].append(row)
        save()
        result = subprocess.run(command, cwd=work, env=environment, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=600)
        (work / (name + '.stdout')).write_bytes(result.stdout)
        (work / (name + '.stderr')).write_bytes(result.stderr)
        row['exit'] = result.returncode
        save()
        assert result.returncode == 0, (name, result.stderr[-6000:], result.stdout[-6000:])
        return result.stdout.decode('utf-8')

    try:
        text = []
        for repository in (original, patched):
            with zipfile.ZipFile(repository / CORE / SOURCES) as archive:
                source = archive.read('org/teavm/backend/wasm/runtime/WasiSupport.java').decode()
                text.append(source[source.index('    public static String[] getArgs() {'):source.index('    public static double random() {')])
        assert text[0] == text[1], 'existing argv provider changed'
        record['argv_provider_source_unchanged'] = True
        shutil.copy2(project / 'pom.xml', work / 'pom.xml')
        for name in ['ArgsEnv.java', 'ArgsEnvAdapted.java']:
            destination = work / 'src/main/java/uwvm/hll' / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(project / 'src/main/java/uwvm/hll' / name, destination)
        os.environ['MAVEN_OPTS'] = '-Xmx2g -XX:ActiveProcessorCount=2'
        for variant, repository, main in [('original', original, 'ArgsEnv'), ('adapted', patched, 'ArgsEnvAdapted')]:
            run([args.maven.resolve(strict=True), '-o', '--batch-mode', '--no-transfer-progress', 'process-classes',
                 '-Dmaven.repo.local=' + str(repository), '-Dteavm.target=WEBASSEMBLY_WASI',
                 '-Dteavm.output=' + variant, '-Dteavm.optimization=FULL',
                 '-Dfixture.main=uwvm.hll.' + main, '-Dfixture.file=java-args-env.wasm'], 'maven-' + variant)
        artifacts = {name: work / 'target' / name / 'java-args-env.wasm' for name in ('original', 'adapted')}
        record['imports'] = {name: imports(path) for name, path in artifacts.items()}
        for name, rows in record['imports'].items():
            assert all(row['module'] == 'wasi_snapshot_preview1' for row in rows), rows
            environment_imports = {row['name'] for row in rows if row['name'].startswith('environ_')}
            assert environment_imports == ({'environ_sizes_get', 'environ_get'} if name == 'adapted' else set())
        java = Path(os.environ.get('JAVA_HOME', '/usr/lib/jvm/java-21-openjdk-amd64')) / 'bin/java'
        for argv_index, arguments in enumerate(ARGUMENTS):
            for env_index, environment in enumerate([{}, VALUES]):
                name = f'args-{argv_index}-env-{env_index}'
                launcher = {'LC_ALL': 'C.UTF-8', **environment}
                jvm = run([java, '-Xmx512m', '-XX:ActiveProcessorCount=1', '-cp', work / 'target/classes',
                           'uwvm.hll.ArgsEnvAdapted', *arguments], name + '-jvm', launcher)
                flags = [item for key, value in environment.items() for item in ['--env', key + '=' + value]]
                wasm = run([args.wasmtime.resolve(strict=True), 'run', '-C', 'cache=n,parallel-compilation=n',
                            *flags, artifacts['adapted'], *arguments], name + '-wasmtime', {'LC_ALL': 'C.UTF-8'})
                wanted = expected(arguments, environment, True)
                assert jvm == wasm == wanted, (name, jvm, wasm, wanted)
                record['reference_runs'].append({'name': name, 'args': arguments, 'guest_environment': environment,
                                                 'jvm_launcher_extra': {'LC_ALL': 'C.UTF-8'},
                                                 'strict_stdout': wanted, 'jvm_wasmtime_identical': True})
                save()
        # Also run the JVM with a genuinely empty process environment; no UTF-8
        # argv is involved, so native launcher locale does not affect arguments.
        empty = run([java, '-cp', work / 'target/classes', 'uwvm.hll.ArgsEnvAdapted'], 'jvm-empty-process-env', {})
        assert empty == expected([], {}, True)
        negative_flags = [item for key, value in VALUES.items() for item in ['--env', key + '=' + value]]
        negative = run([args.wasmtime.resolve(strict=True), 'run', '-C', 'cache=n,parallel-compilation=n',
                        *negative_flags, artifacts['original'], *ARGUMENTS[1]], 'original-stub-negative', {'LC_ALL': 'C.UTF-8'})
        original_jvm = run([java, '-cp', work / 'target/classes', 'uwvm.hll.ArgsEnv', *ARGUMENTS[1]],
                           'original-fixture-jvm', {'LC_ALL': 'C.UTF-8', **VALUES})
        assert negative == expected(ARGUMENTS[1], {}, False)
        assert original_jvm == expected(ARGUMENTS[1], VALUES, False) and negative != original_jvm
        record['original_stub_negative'] = {'retained': True, 'jvm_wasm_environment_mismatch': True,
                                            'argv_lines_identical': True}
        record['artifacts'] = {}
        for variant, path in artifacts.items():
            output = project / 'target' / ('args-env-' + variant + args.output_tag)
            shutil.copytree(path.parent, output)
            record['artifacts'][variant] = {'path': str(output / path.name), 'sha256': sha(output / path.name)}
        shutil.copytree(work / 'target/classes', project / 'target' / ('args-env-adapted' + args.output_tag) / 'classes')
    except Exception as error:
        record['error'] = repr(error)
    finally:
        record['preserved_after'] = {path: sha(path) for path in preserved}
        record['admission_after'] = admission()
        record['passed'] = ('error' not in record and record['preserved_before'] == record['preserved_after']
                            and guard == record['admission_after'])
        save()
    print(json.dumps({'passed': record['passed'], 'error': record.get('error'), 'artifacts': record.get('artifacts'),
                      'receipt': str(work / 'receipt.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
