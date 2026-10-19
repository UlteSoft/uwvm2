#!/usr/bin/env python3
"""Build a real Java scalar debug oracle with original and private compilers."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from build_dwarf_fixed import function_names, sections, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--maven', type=Path, required=True)
    parser.add_argument('--original-m2', type=Path, required=True)
    parser.add_argument('--patched-m2', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', default='typed-debug-fixed')
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    assert sys.platform == 'linux', 'execute inside the authorized SSH Linux cgroup'
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission
    before = admission()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    project = args.project.resolve(strict=True)
    assert args.output and Path(args.output).name == args.output
    source = project / 'src/main/java/uwvm/hll/TypedDebug.java'
    record = {'passed': False, 'admission_before': before, 'source_sha256': sha(source),
              'builder_sha256': sha(__file__), 'commands': []}

    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')

    def run(argv, log):
        argv = [str(value) for value in argv]
        row = {'argv': argv, 'cwd': str(work), 'log': log}
        record['commands'].append(row)
        save()
        with (work / log).open('wb') as stream:
            result = subprocess.run(argv, cwd=work, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        row['exit'] = result.returncode
        save()
        assert result.returncode == 0, (work / log).read_text(errors='replace')[-6000:]
        return (work / log).read_text()

    try:
        shutil.copy2(project / 'pom.xml', work / 'pom.xml')
        destination = work / 'src/main/java/uwvm/hll/TypedDebug.java'
        destination.parent.mkdir(parents=True)
        shutil.copy2(source, destination)
        os.environ['MAVEN_OPTS'] = '-Xmx2g -XX:ActiveProcessorCount=2'
        for variant, repository in [('original', args.original_m2), ('fixed', args.patched_m2)]:
            run([args.maven.resolve(strict=True), '-o', '--batch-mode', '--no-transfer-progress', 'process-classes',
                 '-Dmaven.repo.local=' + str(repository.resolve(strict=True)), '-Dteavm.target=WEBASSEMBLY_WASI',
                 '-Dteavm.output=' + variant, '-Dteavm.optimization=SIMPLE',
                 '-Dfixture.main=uwvm.hll.TypedDebug', '-Dfixture.file=java-typed-debug.wasm'], variant + '-maven.log')
        original = work / 'target/original/java-typed-debug.wasm'
        fixed = work / 'target/fixed/java-typed-debug.wasm'
        assert sections(original) == sections(fixed), 'debug producer patch changed executable code'
        assert function_names(fixed) == sorted(function_names(original)), 'name identity changed'
        java = Path(os.environ.get('JAVA_HOME', '/usr/lib/jvm/java-21-openjdk-amd64')) / 'bin/java'
        jvm = run([java, '-cp', work / 'target/classes', 'uwvm.hll.TypedDebug', '17'], 'jvm-17.stdout')
        reference = run([args.wasmtime.resolve(strict=True), 'run', fixed, '17'], 'wasmtime-17.stdout')
        expected = 'java typed debug: counter=24\njava typed debug: counter=31\njava typed debug: final=31\n'
        assert jvm == reference == expected, (jvm, reference)
        output = project / 'target' / args.output
        shutil.copytree(fixed.parent, output)
        (output / 'expected.stdout').write_text(expected)
        record.update(passed=True, all_non_custom_sections_byte_identical=True,
                      original_wasm_sha256=sha(original), patched_wasm_sha256=sha(fixed),
                      artifact=str(output / fixed.name), oracle={
                          'argument': '17', 'expression': 'counter', 'before_line': 10,
                          'before_value': '17', 'after_line': 11, 'after_value': '24',
                          'source_path': 'uwvm/hll/TypedDebug.java',
                          'function_name': 'meth_uh_TypedDebug_main', 'expected_stdout': expected})
    except Exception as error:
        record['error'] = repr(error)
    finally:
        assert sha(source) == record['source_sha256']
        record['admission_after'] = admission()
        if record['admission_after']['memory_events'] != before['memory_events']:
            record.update(passed=False, memory_events_changed=True)
        save()
    print(json.dumps({'passed': record['passed'], 'error': record.get('error'),
                      'receipt': str(work / 'receipt.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
