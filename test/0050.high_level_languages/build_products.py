#!/usr/bin/env python3
"""Build immutable source snapshots in the existing SSH Linux 64 GiB cgroup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess
import time


def digest(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def admission():
    assert os.uname().sysname == 'Linux'
    membership = Path('/proc/self/cgroup').read_text().strip()
    assert membership.startswith('0::/system.slice/docker-')
    cg = Path('/sys/fs/cgroup') / membership[3:].lstrip('/')
    assert str(os.getpid()) in (cg / 'cgroup.procs').read_text().split()
    assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return {'membership': membership, 'cpus': (cg / 'cpuset.cpus.effective').read_text().strip(),
            'memory_events': (cg / 'memory.events').read_text()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--only', choices=['llvm', 'uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    a = p.parse_args()
    record = {'admission': admission(), 'commands': []}
    a.arena = a.arena.resolve()
    out = a.arena / 'products'
    out.mkdir(exist_ok=True)
    sdk_lib = a.sdk / 'lib/x86_64-unknown-linux-gnu'
    os.environ['LD_LIBRARY_PATH'] = str(sdk_lib) + ':' + str(a.sdk / 'lib')
    cxx, cc = str(a.sdk / 'bin/clang++'), str(a.sdk / 'bin/clang')
    link_flags = ['-stdlib=libc++', '-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind',
                  '-L' + str(sdk_lib), '-Wl,-rpath,' + str(sdk_lib)]

    def run(argv, log, cwd=None, timeout=3600):
        argv = [str(v) for v in argv]
        print(shlex.join(argv), flush=True)
        start = time.monotonic()
        with (out / log).open('wb') as f:
            q = subprocess.run(argv, cwd=cwd, stdout=f, stderr=subprocess.STDOUT, timeout=timeout)
        record['commands'].append({'argv': argv, 'cwd': str(cwd), 'exit': q.returncode,
                                   'seconds': time.monotonic() - start, 'log': log})
        (out / ('build-' + (a.only or 'all') + '.json')).write_text(json.dumps(record, indent=2))
        if q.returncode:
            raise RuntimeError((out / log).read_text(errors='replace')[-8000:])

    ros = a.arena / 'uwvm2-ros'
    llvm = a.arena / 'llvm'
    if a.only in (None, 'llvm'):
        run(['sha256sum', '-c', '--quiet', 'sources.sha256'], 'llvm-source.log', ros / 'third-parties/llvm')
        (llvm / '.cmake/api/v1/query').mkdir(parents=True, exist_ok=True)
        (llvm / '.cmake/api/v1/query/codemodel-v2').touch()
        run(['cmake', '-S', ros / 'third-parties/llvm/llvm', '-B', llvm, '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PROJECT_LLVM_INCLUDE=' + str(ros / 'xmake/llvm/contract.cmake'),
             '-DCMAKE_C_COMPILER=' + cc, '-DCMAKE_CXX_COMPILER=' + cxx,
             '-DCMAKE_CXX_FLAGS=-stdlib=libc++', '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(link_flags),
             '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(link_flags), '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
             '-DLLVM_TARGETS_TO_BUILD=X86', '-DLLVM_VERSION_SUFFIX=-uwvm-ros.13', '-DLLVM_APPEND_VC_REV=OFF',
             '-DLLVM_ENABLE_PROJECTS=', '-DLLVM_ENABLE_RUNTIMES=', '-DLLVM_INCLUDE_TESTS=OFF',
             '-DLLVM_INCLUDE_BENCHMARKS=OFF', '-DLLVM_INCLUDE_EXAMPLES=OFF', '-DLLVM_INCLUDE_DOCS=OFF',
             '-DLLVM_ENABLE_BINDINGS=OFF', '-DLLVM_BUILD_TOOLS=OFF', '-DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF',
             '-DLLVM_BUILD_UTILS=OFF', '-DLLVM_BUILD_LLVM_DYLIB=OFF', '-DLLVM_LINK_LLVM_DYLIB=OFF',
             '-DBUILD_SHARED_LIBS=OFF', '-DLLVM_ENABLE_ZLIB=OFF', '-DLLVM_ENABLE_ZSTD=OFF',
             '-DLLVM_ENABLE_LIBXML2=OFF', '-DLLVM_ENABLE_CURL=OFF', '-DLLVM_ENABLE_LIBEDIT=OFF',
             '-DLLVM_ENABLE_ASSERTIONS=OFF', '-DLLVM_ENABLE_EH=OFF', '-DLLVM_ENABLE_RTTI=OFF',
             '-DLLVM_ENABLE_LTO=OFF', '-DLLVM_PARALLEL_LINK_JOBS=1'], 'llvm-configure.log')
        run(['cmake', '--build', llvm, '--parallel', '12', '--target', 'llvm-libraries'], 'llvm-build.log', timeout=7200)
        assert '#define LLVM_VERSION_STRING "23.1.1-uwvm-ros.13"' in (llvm / 'include/llvm/Config/llvm-config.h').read_text()
        reply = llvm / '.cmake/api/v1/reply'
        index = json.loads(max(reply.glob('index-*.json')).read_text())
        model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
        target = next(t for t in model['configurations'][0]['targets'] if t['name'] == 'uwvm_ros_llvm_contract')
        data = json.loads((reply / target['jsonFile']).read_text())
        tokens = []
        for fragment in data['link']['commandFragments']:
            for token in shlex.split(fragment['fragment']):
                if token.endswith('.a'):
                    path = (llvm / token).resolve(strict=True)
                    assert path.is_relative_to(llvm)
                    token = str(path)
                tokens.append(token)
        (llvm / 'consumer-link.rsp').write_text('\n'.join('"' + t.replace('\\', '\\\\').replace('"', '\\"') + '"' for t in tokens))
    if a.only == 'llvm':
        return
    for name in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        if a.only and a.only != name:
            continue
        root = a.arena / name
        dest = out / name
        dest.mkdir(exist_ok=True)
        fp = root / 'tools/ci/wasm3_source_fingerprint.py'
        before = subprocess.check_output(['python3', fp, root, dest / 'sources-before.json'], text=True).strip()
        flags = ['-std=c++26', '-stdlib=libc++', '-fno-rtti', '-fasynchronous-unwind-tables', '-O2', '-g1',
                 '-Wno-undefined-inline', '-DUWVM=2', '-DUWVM_USE_LLVM_JIT', '-DUWVM_DISABLE_INT',
                 '-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519', '-DUWVM_VERSION_X=2',
                 '-DUWVM_ENABLE_WASIP1_SOCKET',
                 '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
                 '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"']
        if (root / 'src/uwvm2/runtime/compiler/llvm_jit/pinned_version.h').exists():
            flags += ['-I' + str(llvm / 'include'), '-I' + str(ros / 'third-parties/llvm/llvm/include')]
            libraries = ['@' + str(llvm / 'consumer-link.rsp')]
        else:
            flags += ['-I' + str(a.sdk / 'include')]
            libraries = shlex.split(subprocess.check_output([str(a.sdk / 'bin/llvm-config'), '--ldflags', '--libs',
                                      '--system-libs'], text=True))
            libraries += ['-Wl,-rpath,' + str(a.sdk / 'lib')]
        for inc in ['src', 'third-parties/bizwen/include', 'third-parties/fast_io/include', 'third-parties/boost_unordered/include']:
            flags += ['-I' + str(root / inc)]
        units = ['src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', 'src/uwvm2/uwvm/main.default.cpp', 'src/uwvm2/uwvm/host_api.default.cpp']
        objects = []
        for i, unit in enumerate(units):
            obj = dest / f'{i}.o'
            run([cxx, *flags, '-c', root / unit, '-o', obj], name + f'-{i}.log', timeout=2400)
            objects.append(obj)
        run([cxx, *link_flags, *objects, *libraries, '-lssl', '-lcrypto', '-ldl', '-pthread', '-o', dest / 'uwvm'], name + '-link.log')
        after = subprocess.check_output(['python3', fp, root, dest / 'sources-after.json'], text=True).strip()
        assert before == after
        (dest / 'identity.json').write_text(json.dumps({'source_id': before, 'binary_sha256': digest(dest / 'uwvm'),
                                                        'compiler_sha256': digest(cxx), 'admission': admission()}, indent=2))
        for obj in objects:
            obj.unlink()


if __name__ == '__main__':
    main()
