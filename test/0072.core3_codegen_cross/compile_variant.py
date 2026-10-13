#!/usr/bin/env python3
"""Build and execute one exact interpreter macro set under the shared guard."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import subprocess
import time
from run_matrix import digest, execute, save, verify


def execute_build(argv, log, cwd, timeout, address_space_bytes):
    def limits():
        resource.setrlimit(resource.RLIMIT_AS, (address_space_bytes, address_space_bytes))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    started = time.monotonic()
    with Path(log).open('xb') as stream:
        completed = subprocess.run(argv, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                   timeout=timeout, check=False, preexec_fn=limits)
    return dict(argv=argv, exit=completed.returncode, seconds=time.monotonic() - started,
                log=str(log), log_sha256=digest(log),
                compiler_address_space_limit_bytes=address_space_bytes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    a = parser.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    # Merely omitting UWVM_USE_LLVM_JIT does not disable the product's JIT
    # configuration check. Explicitly disable it in every standalone int TU.
    for label, argv in config['build_commands']:
        assert '-DUWVM_DISABLE_JIT' in argv, (label, 'interpreter-only build must explicitly disable JIT')
        if '-c' in argv:
            if any(flag.startswith('--target=mips') for flag in argv):
                assert any(argv[index:index + 2] == ['-mllvm', '-mips-tail-calls']
                           for index in range(len(argv) - 1)), (
                    label, 'LLVM MIPS requires its tail-call backend option for musttail dispatch')
            macro_axes = {'UWVM_ENABLE_UWVM_INT_COMBINE_OPS', 'UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS',
                          'UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS',
                          'UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT', 'UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY'}
            actual = {flag[2:].split('=', 1)[0] for flag in argv if flag.startswith('-D')} & macro_axes
            assert actual == set(config['macros']), (label, actual, config['macros'])
            assert [flag for flag in argv if flag in ('-O0', '-O1', '-O2', '-O3', '-Os', '-Oz', '-Ofast')] == [
                '-' + config['host_optimization']], (label, 'host optimization differs from the declared build')
    verify(config['pins'])
    manifest = json.loads(Path(config['source_manifest']).read_text())
    source_pins = {str(Path(config['source_root']) / p): h
                   for p, h in manifest['files'].items() if p.startswith(config['repo'] + '/')}
    verify(source_pins)
    out = Path(config['output'])
    out.mkdir(parents=True, exist_ok=False)
    builds = []
    compiler_as_gib = config.get('compiler_as_gib', 12)
    assert isinstance(compiler_as_gib, int) and 1 <= compiler_as_gib <= 12
    for label, argv in config['build_commands']:
        row = execute_build(argv, out / (label + '.log'), out,
                            config.get('build_timeout', 900), compiler_as_gib << 30)
        builds.append(row)
        save(out / 'build-checkpoint.json', dict(state='running' if row['exit'] == 0 else 'failed', rows=builds))
        assert row['exit'] == 0, row
    dependencies = {}
    for path in out.glob('*.d'):
        text = path.read_text().replace('\\\n', ' ')
        for name in shlex.split(text.split(':', 1)[1]):
            dependency = Path(name)
            assert dependency.is_file(), name
            dependencies[str(dependency)] = digest(dependency)
    assert dependencies, 'Missing actual compiler dependencies'
    verify(source_pins)
    verify(config['pins'])
    product = str(out / 'uwvm')
    product_hash = digest(product)
    execution = {**config['execution'], 'product': product,
                 'pins': {**config['pins'], product: product_hash}}
    execution_config = out / 'execution-config.json'
    save(execution_config, execution)
    row = execute(['/usr/bin/python3', config['matrix_runner'], str(execution_config), str(out / 'semantics')],
                  out / 'semantics.log', out, config.get('run_timeout', 1200))
    assert row['exit'] == 0, row
    verify(source_pins)
    verify(config['pins'])
    verify(dependencies)
    assert digest(product) == product_hash
    summary = json.loads((out / 'semantics/summary.json').read_text())
    assert summary['passed'] and summary['completed'] == summary['planned']
    save(out / 'qualification.json', dict(passed=True, repo=config['repo'], variant=config['variant'],
        macros=config['macros'], host_optimization=config['host_optimization'],
        source_identity=manifest['identities'][config['repo']], product_sha256=product_hash,
        build_commands=builds, actual_postbuild_dependency_pins=dependencies,
        execution=row, semantic_runs=summary['completed'],
        semantic_summary_sha256=digest(out / 'semantics/summary.json'),
        configuration_sha256=digest(a.config),
        selected_fusion_qualified=False,
        scope='Exact interpreter-only macro build and real CLI semantics; fusion selection and host speed need separate evidence'))
    # These objects were created by this command, are not reused or published,
    # and the linked product plus commands/dependencies are already bound above.
    retired = []
    for path in sorted(out.glob('*.o')):
        assert path.parent == out and path.is_file() and not path.is_symlink() and path.stat().st_nlink == 1
        retired.append(dict(path=str(path), bytes=path.stat().st_size, sha256=digest(path)))
        path.unlink()
    save(out / 'retired-intermediates.json', retired)
    print('PASS exact interpreter macro build', config['repo'], config['variant'], summary['completed'], flush=True)


if __name__ == '__main__':
    main()
