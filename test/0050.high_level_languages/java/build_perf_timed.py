#!/usr/bin/env python3
"""Build a distinct guest-timed Java kernel and compare JVM/Wasmtime checksums."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def parse(text, iterations):
    match = re.fullmatch(r'java perf timed: iterations=(\d+)\nKernel ns: (\d+)\nchecksum=(-?\d+)\n', text)
    assert match and int(match[1]) == iterations, text
    return {'iterations': int(match[1]), 'kernel_ns': int(match[2]), 'checksum': int(match[3])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--maven', type=Path, required=True)
    parser.add_argument('--m2', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    assert sys.platform == 'linux', 'execute only inside the authorized SSH Linux cgroup'
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission, digest
    guard = admission()
    project, repository = args.project.resolve(strict=True), args.m2.resolve(strict=True)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    source = project / 'src/main/java/uwvm/hll/PerfTimed.java'
    preserved = [project / 'src/main/java/uwvm/hll/Perf.java', project / 'target/perf/java-perf.wasm']
    original_class = project / 'target/classes/uwvm/hll/Perf.class'
    if original_class.exists():
        preserved.append(original_class)
    before = {str(path): digest(path) for path in preserved}
    record = {'passed': False, 'admission_before': guard, 'preserved_before': before,
              'source_sha256': digest(source), 'pom_sha256': digest(project / 'pom.xml'),
              'builder_sha256': digest(Path(__file__)), 'maven_repository': str(repository),
              'compiler_jar_sha256': digest(repository / 'org/teavm/teavm-core/0.13.1/teavm-core-0.13.1.jar'),
              'commands': [], 'reference_runs': []}

    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')

    def run(argv, log, cwd=None):
        argv = [str(value) for value in argv]
        row = {'argv': argv, 'cwd': str(cwd), 'log': log}
        record['commands'].append(row)
        save()
        with (work / log).open('wb') as stream:
            result = subprocess.run(argv, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        row['exit'] = result.returncode
        save()
        assert result.returncode == 0, (work / log).read_text(errors='replace')[-6000:]
        return (work / log).read_text()

    try:
        shutil.copy2(project / 'pom.xml', work / 'pom.xml')
        destination = work / 'src/main/java/uwvm/hll/PerfTimed.java'
        destination.parent.mkdir(parents=True)
        shutil.copy2(source, destination)
        os.environ['MAVEN_OPTS'] = '-Xmx2g -XX:ActiveProcessorCount=2'
        run([args.maven.resolve(strict=True), '-o', '--batch-mode', '--no-transfer-progress', 'process-classes',
             '-Dmaven.repo.local=' + str(repository), '-Dteavm.target=WEBASSEMBLY_WASI',
             '-Dteavm.output=perf-timed', '-Dteavm.optimization=FULL',
             '-Dfixture.main=uwvm.hll.PerfTimed', '-Dfixture.file=java-perf-timed.wasm'], 'maven.log', work)
        artifact = work / 'target/perf-timed/java-perf-timed.wasm'
        java = Path(os.environ.get('JAVA_HOME', '/usr/lib/jvm/java-21-openjdk-amd64')) / 'bin/java'
        for iterations in (17, 1000000):
            jvm = parse(run([java, '-cp', work / 'target/classes', 'uwvm.hll.PerfTimed', iterations],
                            f'jvm-{iterations}.stdout'), iterations)
            reference = parse(run([args.wasmtime.resolve(strict=True), 'run', artifact, iterations],
                                  f'wasmtime-{iterations}.stdout'), iterations)
            assert jvm['checksum'] == reference['checksum'], (jvm, reference)
            assert jvm['kernel_ns'] > 0 and reference['kernel_ns'] > 0, (jvm, reference)
            if iterations == 1000000:
                assert jvm['checksum'] == -7218269286671609754, jvm
            record['reference_runs'].append({'jvm': jvm, 'wasmtime': reference})
        output = project / 'target/perf-timed'
        shutil.copytree(artifact.parent, output)
        shutil.copytree(work / 'target/classes', output / 'classes')
        record['artifact'] = {'path': str(output / artifact.name), 'sha256': digest(output / artifact.name)}
        record['jvm_class_directory'] = str(output / 'classes')
        record['passed'] = True
    except Exception as error:
        record['error'] = repr(error)
    finally:
        record['preserved_after'] = {path: digest(path) for path in before}
        if before != record['preserved_after']:
            record.update(passed=False, original_perf_inputs_changed=True)
        record['admission_after'] = admission()
        if record['admission_after']['memory_events'] != guard['memory_events']:
            record.update(passed=False, memory_events_changed=True)
        save()
    print(json.dumps({'passed': record['passed'], 'artifact': record.get('artifact'),
                      'error': record.get('error'), 'receipt': str(work / 'receipt.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
