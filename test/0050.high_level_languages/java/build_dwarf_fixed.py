#!/usr/bin/env python3
"""Build a private, pinned TeaVM compiler with corrected WASI DWARF emission.

The original Maven repository is read-only. This compiles upstream Java source,
repackages a private compiler JAR, and recompiles the original Java fixture.
It never rewrites a generated WebAssembly module or its debug sections.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


CORE = Path('org/teavm/teavm-core/0.13.1')
JAR = 'teavm-core-0.13.1.jar'
SOURCES = 'teavm-core-0.13.1-sources.jar'
ORIGINAL_JAR_SHA256 = 'b9fe1b008512c8f11ff3efbd9cf2f0565b4bfa76943a9f23910befc0812f7ebf'
ORIGINAL_SOURCES_SHA256 = '710b67c644e1e7a847f9bf692acc2d9e967783b61bbcc0fe2169840bf89940f4'
FILES = ['org/teavm/backend/wasm/render/WasmBinaryRenderer.java',
         'org/teavm/backend/wasm/render/WasmBinaryRenderingVisitor.java']


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def sections(path):
    data = Path(path).read_bytes()
    assert data[:8] == b'\0asm\x01\0\0\0'
    at = 8
    result = []
    while at < len(data):
        begin = at
        kind = data[at]
        at += 1
        size = shift = 0
        while True:
            byte = data[at]
            at += 1
            size |= (byte & 127) << shift
            if not byte & 128:
                break
            shift += 7
            assert shift <= 35
        end = at + size
        assert end <= len(data)
        if kind:
            result.append((kind, data[begin:end]))
        at = end
    return result


def repackage(original, replacements, destination):
    with zipfile.ZipFile(original) as source, zipfile.ZipFile(destination, 'w') as target:
        present = set()
        for entry in source.infolist():
            target.writestr(entry, replacements.get(entry.filename, source.read(entry)))
            present.add(entry.filename)
        for name in sorted(replacements.keys() - present):
            target.writestr(name, replacements[name], compress_type=zipfile.ZIP_DEFLATED)


def function_names(path):
    """Inspect producer bytes without rewriting them; retain encoded order."""
    data = Path(path).read_bytes()
    at = 8

    def leb():
        nonlocal at
        value = shift = 0
        while True:
            byte = data[at]
            at += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
            shift += 7
            assert shift <= 35

    while at < len(data):
        kind = data[at]
        at += 1
        size = leb()
        end = at + size
        if kind == 0:
            length = leb()
            name = data[at:at + length]
            at += length
            if name == b'name':
                while at < end:
                    subsection = data[at]
                    at += 1
                    length = leb()
                    subend = at + length
                    if subsection == 1:
                        result = []
                        count = leb()
                        for _ in range(count):
                            index = leb()
                            length = leb()
                            value = data[at:at + length].decode('utf-8')
                            at += length
                            result.append((index, value))
                        assert at == subend
                        return result
                    at = subend
        at = end
    raise AssertionError('producer name section/function map absent')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original-m2', type=Path, required=True)
    parser.add_argument('--maven', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--wasi-monotonic', action='store_true', help='Also bind System.nanoTime to WASI monotonic nanoseconds')
    parser.add_argument('--wasi-stdin', action='store_true', help='Also bind single-byte System.in.read to blocking WASI fd_read (requires --wasi-monotonic)')
    parser.add_argument('--wasi-environ', action='store_true', help='Isolated legacy-WASI System.getenv overloads (separate from clock/stdin toolchains)')
    parser.add_argument('--ordered-names', action='store_true', help='Emit function/local name maps in actual function-index order')
    parser.add_argument('--typed-locals', action='store_true', help='Emit direct local values at their actual zero-based Wasm indices')
    parser.add_argument('--fixture-output', default='wasi-debug-dwarf-fixed')
    parser.add_argument('--fixture-main', default='uwvm.hll.Semantics')
    parser.add_argument('--fixture-file', default='java-semantics.wasm')
    parser.add_argument('--upstream-output', default='wasi-debug', help='Existing original compiler SIMPLE output directory')
    args = parser.parse_args()
    assert sys.platform == 'linux', 'use the authorized SSH Linux cgroup'
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission
    guard = admission()
    project = args.project.resolve(strict=True)
    original = args.original_m2.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    jar, sources = original / CORE / JAR, original / CORE / SOURCES
    assert sha(jar) == ORIGINAL_JAR_SHA256, 'upstream compiler JAR identity mismatch'
    assert sha(sources) == ORIGINAL_SOURCES_SHA256, 'upstream compiler source identity mismatch'
    patch = Path(__file__).resolve().parent / 'patches/teavm-0.13.1-wasm-dwarf.patch'
    patches = [patch]
    files = list(FILES)
    if args.ordered_names:
        patches.append(patch.with_name('teavm-0.13.1-ordered-names.patch'))
    if args.typed_locals:
        patches.append(patch.with_name('teavm-0.13.1-typed-locals.patch'))
        files.append('org/teavm/backend/wasm/generate/DwarfFunctionGenerator.java')
    if args.wasi_monotonic:
        patches.append(patch.with_name('teavm-0.13.1-wasi-monotonic.patch'))
        files += ['org/teavm/backend/wasm/runtime/WasiSupport.java',
                  'org/teavm/backend/wasm/transformation/WasiSupportClassTransformer.java']
    if args.wasi_stdin:
        assert args.wasi_monotonic, '--wasi-stdin requires the isolated --wasi-monotonic source patch'
        patches.append(patch.with_name('teavm-0.13.1-wasi-stdin.patch'))
    if args.wasi_environ:
        assert not args.wasi_monotonic and not args.wasi_stdin, 'build the optional environ adapter in its own private compiler cache'
        patches.append(patch.with_name('teavm-0.13.1-wasi-environ.patch'))
        files += ['org/teavm/backend/wasm/runtime/WasiSupport.java',
                  'org/teavm/backend/wasm/transformation/WasiSupportClassTransformer.java']
    assert args.fixture_output and Path(args.fixture_output).name == args.fixture_output
    assert args.fixture_file and Path(args.fixture_file).name == args.fixture_file
    assert args.upstream_output and Path(args.upstream_output).name == args.upstream_output
    record = {'passed': False, 'admission_before': guard, 'commands': [],
              'official_jar': {'path': str(jar), 'sha256': sha(jar),
                               'url': 'https://repo.maven.apache.org/maven2/' + str(CORE / JAR)},
              'official_sources': {'path': str(sources), 'sha256': sha(sources),
                                   'url': 'https://repo.maven.apache.org/maven2/' + str(CORE / SOURCES)},
              'patch_sha256': {item.name: sha(item) for item in patches},
              'wasi_monotonic_mapping': args.wasi_monotonic, 'ordered_name_maps': args.ordered_names,
              'wasi_stdin_mapping': args.wasi_stdin,
              'wasi_environ_mapping': args.wasi_environ,
              'typed_local_values': args.typed_locals,
              'source_before': {}, 'source_after': {}}

    def save():
        (out / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')

    def run(argv, log, cwd=None):
        argv = [str(value) for value in argv]
        row = {'argv': argv, 'cwd': str(cwd), 'log': log}
        record['commands'].append(row)
        save()
        with (out / log).open('wb') as stream:
            completed = subprocess.run(argv, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        row['exit'] = completed.returncode
        save()
        if completed.returncode:
            raise RuntimeError((out / log).read_text(errors='replace')[-6000:])

    try:
        shutil.copy2(jar, out / JAR)
        shutil.copy2(sources, out / SOURCES)
        for item in patches:
            shutil.copy2(item, out / item.name)
        source_root = out / 'source'
        with zipfile.ZipFile(sources) as archive:
            for name in files:
                destination = source_root / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(archive.read(name))
                record['source_before'][name] = sha(destination)
        for index, item in enumerate(patches):
            run(['patch', '--batch', '--forward', '--fuzz=0', '-p1', '-i', item], f'patch-{index}.log', source_root)
        for name in files:
            record['source_after'][name] = sha(source_root / name)
        classes = out / 'classes'
        classes.mkdir()
        dependencies = [jar] + sorted(path for path in original.rglob('*.jar')
                                      if path != jar and not path.name.endswith('-sources.jar'))
        classpath = os.pathsep.join(map(str, dependencies))
        java = Path(os.environ.get('JAVA_HOME', '/usr/lib/jvm/java-21-openjdk-amd64')) / 'bin/java'
        record['java_version'] = subprocess.check_output([str(java), '-version'], stderr=subprocess.STDOUT, text=True)
        run([java, '-XX:ActiveProcessorCount=2', 'com.sun.tools.javac.Main', '-source', '17', '-target', '17',
             '-g', '-proc:none', '-cp', classpath, '-d', classes, *[source_root / name for name in files]], 'javac.log')
        replacements = {str(path.relative_to(classes)): path.read_bytes() for path in classes.rglob('*.class')}
        assert replacements and all(name.startswith('org/teavm/backend/wasm/') for name in replacements)
        variant = 'dwarf-clock-fixed' if args.wasi_monotonic else 'dwarf-fixed'
        if args.ordered_names:
            variant += '-ordered-names'
        if args.typed_locals:
            variant += '-typed-locals'
        if args.wasi_stdin:
            variant += '-stdin'
        if args.wasi_environ:
            variant += '-environ'
        patched = out / ('teavm-core-0.13.1-uwvm-' + variant + '.jar')
        repackage(jar, replacements, patched)
        record['patched_jar_sha256'] = sha(patched)
        record['replaced_classes'] = {name: hashlib.sha256(data).hexdigest() for name, data in replacements.items()}
        isolated = out / 'm2'
        shutil.copytree(original, isolated)
        shutil.copy2(patched, isolated / CORE / JAR)
        patched_sources = out / ('teavm-core-0.13.1-uwvm-' + variant + '-sources.jar')
        repackage(sources, {name: (source_root / name).read_bytes() for name in files}, patched_sources)
        shutil.copy2(patched_sources, isolated / CORE / SOURCES)
        for artifact in [isolated / CORE / JAR, isolated / CORE / SOURCES]:
            artifact.with_name(artifact.name + '.sha1').write_text(hashlib.sha1(artifact.read_bytes()).hexdigest())
        os.environ['MAVEN_OPTS'] = '-Xmx2g -XX:ActiveProcessorCount=2'
        run([args.maven.resolve(strict=True), '-o', '--batch-mode', '--no-transfer-progress', 'process-classes',
             '-Dmaven.repo.local=' + str(isolated), '-Dteavm.target=WEBASSEMBLY_WASI',
             '-Dteavm.output=' + args.fixture_output, '-Dteavm.optimization=SIMPLE',
             '-Dfixture.main=' + args.fixture_main, '-Dfixture.file=' + args.fixture_file], 'maven.log', project)
        upstream_wasm = project / 'target' / args.upstream_output / args.fixture_file
        patched_wasm = project / 'target' / args.fixture_output / args.fixture_file
        same_sections = sections(upstream_wasm) == sections(patched_wasm)
        # DWARF-only variants must not change executable code. Optional WASI
        # adapters replace guest runtime classes; their separate fixture builders
        # require strict JVM/Wasmtime functional oracles instead of code identity.
        assert same_sections or args.wasi_monotonic or args.wasi_environ, 'DWARF-only patch changed executable module sections'
        record['upstream_wasm_sha256'] = sha(upstream_wasm)
        record['patched_wasm'] = {'path': str(patched_wasm), 'sha256': sha(patched_wasm)}
        record['all_non_custom_sections_byte_identical'] = same_sections
        record['non_custom_identity_required'] = not (args.wasi_monotonic or args.wasi_environ)
        if args.ordered_names:
            before_names, after_names = function_names(upstream_wasm), function_names(patched_wasm)
            assert len(dict(before_names)) == len(before_names), 'duplicate original function index'
            assert after_names == sorted(before_names), 'name identities changed beyond ordering'
            record['function_names'] = {
                'count': len(after_names),
                'identities_unchanged_only_sorted': True,
                'original_strictly_increasing': all(a[0] < b[0] for a, b in zip(before_names, before_names[1:])),
                'patched_strictly_increasing': all(a[0] < b[0] for a, b in zip(after_names, after_names[1:])),
                'original_order': before_names,
                'patched_order': after_names,
            }
        assert sha(jar) == ORIGINAL_JAR_SHA256 and sha(sources) == ORIGINAL_SOURCES_SHA256
        record['original_artifacts_unchanged'] = True
        record['passed'] = True
    except Exception as error:
        record['error'] = repr(error)
    finally:
        record['admission_after'] = admission()
        if record['admission_after']['memory_events'] != guard['memory_events']:
            record.update(passed=False, memory_events_changed=True)
        save()
    print(json.dumps({'passed': record['passed'], 'error': record.get('error'),
                      'receipt': str(out / 'receipt.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
