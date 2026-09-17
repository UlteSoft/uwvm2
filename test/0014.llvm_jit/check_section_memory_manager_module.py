#!/usr/bin/env python3
"""Build the real section-manager partition and its named-module dependencies."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root', required=True, type=Path)
p.add_argument('--cxx', required=True)
p.add_argument('--llvm-config', required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--sdk', type=Path)
p.add_argument('--negative-macho-control', action='store_true',
               help='on Darwin arm64, also verify that removing the required global-fragment include fails')
a = p.parse_args()
a.source_root = a.source_root.resolve()
a.out = a.out.resolve()
if a.negative_macho_control and (platform.system() != 'Darwin' or platform.machine() not in ('arm64', 'aarch64')):
    p.error('--negative-macho-control requires native Darwin arm64')
a.out.mkdir(parents=True, exist_ok=False)
env = {k: v for k, v in os.environ.items() if not k.startswith('DYLD_')}
llvm_flags = shlex.split(subprocess.check_output([a.llvm_config, '--cxxflags'], env=env, text=True))
llvm_flags = [x for x in llvm_flags if not x.startswith('-std=') and x != '-fno-exceptions']
flags = [*llvm_flags, '-std=c++26', '-O3', '-Wno-deprecated-declarations', '-DUWVM_MODULE',
         '-DUWVM_USE_UWVM_INT', '-DUWVM_USE_LLVM_JIT', '-DUWVM_USE_THREAD_LOCAL',
         '-I' + str(a.source_root / 'src'),
         '-I' + str(a.source_root / 'third-parties/fast_io/include'),
         '-I' + str(a.source_root / 'third-parties/bizwen/include'),
         '-I' + str(a.source_root / 'third-parties/boost_unordered/include')]
if a.sdk:
    flags += ['-isysroot', str(a.sdk)]
module_re = re.compile(r'^export module ([^;]+);', re.M)
import_re = re.compile(r'^(?:export )?import ([^;]+);', re.M)
sources = {}
for path in list((a.source_root / 'src').rglob('*.cppm')) + list((a.source_root / 'third-parties/fast_io/share').rglob('*.cppm')):
    match = module_re.search(path.read_text(encoding='utf-8-sig'))
    if match:
        sources[match[1]] = path
target = 'uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm:section_memory_manager'
built = {}
records = []

def run(name, source, output):
    cmd = [a.cxx, *flags, *[f'-fmodule-file={n}={b}' for n, b in built.items()],
           '-I' + str(sources[target].parent), '--precompile', str(source), '-o', str(output)]
    record = dict(module=name, source=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest(), command=cmd)
    with output.with_suffix('.log').open('w') as log:
        result = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    record['returncode'] = result.returncode
    records.append(record)
    (a.out / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print(name, result.returncode, flush=True)
    return result.returncode

def build(name):
    if name in built:
        return
    source = sources[name]
    content = source.read_text(encoding='utf-8-sig')
    for dep in import_re.findall(content):
        build(name.split(':')[0] + dep if dep.startswith(':') else dep)
    output = a.out / (name.replace(':', '-') + '.pcm')
    if name == target and a.negative_macho_control:
        required = '# if defined(__APPLE__) && defined(__aarch64__)\n#  include "macho_headers.h"\n# endif\n'
        if content.count(required) != 1:
            raise RuntimeError('expected one guarded Mach-O include in the global module fragment')
        before = a.out / 'section-before.cppm'
        before.write_text(content.replace(required, ''))
        if run(name + '-negative-control', before, a.out / 'section-before.pcm') == 0:
            raise RuntimeError('missing-header negative control unexpectedly compiled')
        log = (a.out / 'section-before.log').read_text()
        if 'MachOObjectFile' not in log:
            raise RuntimeError('negative control failed for an unrelated reason')
    if run(name, source, output):
        raise RuntimeError(f'failed module {name}; inspect {output.with_suffix(".log")}')
    built[name] = output

build(target)
summary = {'modules_built': len(built), 'target': target, 'negative_control': a.negative_macho_control}
(a.out / 'completed.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary), flush=True)
