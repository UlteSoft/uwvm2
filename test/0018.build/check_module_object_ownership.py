#!/usr/bin/env python3
"""Check production module ownership and a real Clang/xmake initializer link.

The small fixture is not a full ROS build. Its positive control checks exactly
one dynamic initializer through an object-library dependency, with different
consumer/runtime FP flags. Negative controls restore duplicate object owners
or hide the owner's interfaces. Each control gets a fresh build directory.

The exported, ODR-used initializer deliberately isolates ownership. It does not
catch Clang snapshots which discard unreferenced internal-variable initializers;
check_module_initializers.lua covers that separate compiler defect. Do not use
a pass here to bypass the production bootstrap-compiler check.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess


def check_registration(text):
    # This is intentionally a narrow guard for these target registrations, not
    # a Lua parser. The real compiler/linker controls below test their semantics.
    cli = text.split('target("uwvm")\n', 1)[1].split('\ntarget_end()', 1)[0]
    runtime = text.split('target("uwvm_runtime")\n', 1)[1].split('\ntarget_end()', 1)[0]
    code = lambda value: re.sub(r'--[^\n]*', '', value)
    cli, runtime = code(cli), code(runtime)
    assert '.cppm"' not in cli and 'uwvm_add_frontend_module_files(' not in cli
    assert 'add_deps("uwvm_runtime")' in cli
    assert 'set_kind("object")' in runtime
    assert 'public = is_debug_mode' not in runtime
    assert 'uwvm_add_frontend_module_files(true)' in runtime
    for group in ('uwvm_predefine', 'utils', 'object', 'imported', 'parser', 'validation'):
        assert f'add_files("src/uwvm2/{group}/**.cppm", {{ public = true }})' in runtime
    tests = text.split('-- test unit\n', 1)[1]
    assert tests.count('if enable_cxx_module and not test_uses_runtime then') == 2
    mirrors = tests.split('-- LLVM ', 1)[1]
    assert '.cppm"' not in code(mirrors)
    # ROS once silently excluded seven now-supported Wasm 1.1 suites. Keep the
    # same complete strict inventory as the ordinary product. This is a narrow
    # registration guard; actual execution remains a separate responsibility.
    assert 'local llvm_jit_strict_files = os.files("test/0013.uwvm_int/strict/**.cc")' in code(mirrors)


MODULE = '''module;
extern "C" void note_initializer();
export module ownership;
export int once = (note_initializer(), 0);
export int answer() { return 42 + once; }
'''
RUNTIME = '''import ownership;
int runtime_answer() { return answer(); }
'''
MAIN = '''import ownership;
int initializers;
extern "C" void note_initializer() { ++initializers; }
int runtime_answer();
int main() {
    // ODR-use the initialized entity before observing the side effect, so
    // deferred dynamic initialization is not misdiagnosed as a missing one.
    int sum = answer() + runtime_answer();
    return initializers == 1 && sum == 84 ? 0 : 1;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--xmake', default='xmake')
    parser.add_argument('--cc', required=True)
    parser.add_argument('--cxx', required=True)
    parser.add_argument('--runtime-libdir', action='append', default=[])
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()
    args.cc = shutil.which(args.cc)
    args.cxx = shutil.which(args.cxx)
    assert args.cc and args.cxx, 'Both supplied Clang executables must exist'
    source = args.source_root/'xmake.lua'
    text = source.read_text(encoding='utf-8-sig')
    check_registration(text)
    # Both old registration mistakes must be rejected, not merely absent from
    # today's source. These controls do not claim to execute omitted suites.
    negative_sources = {
        'duplicate ownership': text.replace('target("uwvm")\n', 'target("uwvm")\nadd_files("ownership.cppm")\n', 1),
        'filtered strict inventory': text.replace('local llvm_jit_strict_files = os.files("test/0013.uwvm_int/strict/**.cc")',
                                                 'local llvm_jit_strict_files = {}', 1),
    }
    for name, broken in negative_sources.items():
        try:
            check_registration(broken)
        except AssertionError:
            pass
        else:
            raise AssertionError('source guard accepted '+name)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = []
    report = dict(note=__doc__, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  source_positive=True, source_negative=True, source_negative_cases=list(negative_sources), rows=rows)

    def run(command, directory, name):
        process = subprocess.Popen(command, cwd=directory, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=os.name == 'posix')
        try:
            output, _ = process.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            if os.name == 'posix':
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            output, _ = process.communicate()
            (directory/(name+'.log')).write_bytes(output)
            raise AssertionError('timeout: '+name)
        (directory/(name+'.log')).write_bytes(output)
        rows.append(dict(name=directory.name+'/'+name, command=command, returncode=process.returncode))
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        return process.returncode, output

    for name in ('single-owner', 'duplicate-owner', 'private-owner'):
        directory = out/name
        directory.mkdir()
        public = 'true' if name == 'single-owner' else 'false'
        duplicate = 'add_files("ownership.cppm")' if name == 'duplicate-owner' else ''
        script = f'''set_project("ownership-check")
set_languages("cxx20")
set_policy("build.c++.modules", true)
set_policy("build.c++.modules.std", false)
-- A versioned Clang path is sufficient. xmake's stock "clang" toolchain
-- separately probes an unversioned PATH executable, which may be absent on
-- Debian/Ubuntu even when --cc/--cxx point to working clang-23 binaries.
-- Keep this fixture independent of user-global symlinks or SDK substitution.
toolchain("ownership-clang")
    set_kind("standalone")
    set_toolset("cc", {json.dumps(args.cc)})
    set_toolset("cxx", {json.dumps(args.cxx)})
    set_toolset("ld", {json.dumps(args.cxx)})
toolchain_end()
target("runtime")
    set_kind("object")
    add_cxxflags("-fno-math-errno", "-fno-trapping-math", "-fno-rounding-math", "-ffp-contract=off")
    add_files("ownership.cppm", {{public = {public}}})
    add_files("runtime.cpp")
target_end()
target("consumer")
    set_kind("binary")
    set_fpmodels("precise")
    set_targetdir("bin")
    add_deps("runtime")
    add_files("main.cpp")
    {duplicate}
'''
        for libdir in args.runtime_libdir:
            # These are bootstrap compiler runtime paths, never LLVM libraries.
            script += '    add_linkdirs('+json.dumps(libdir)+')\n'
            script += '    add_rpathdirs('+json.dumps(libdir)+')\n'
        script += 'target_end()\n'
        for filename, contents in [('xmake.lua', script), ('ownership.cppm', MODULE),
                                    ('runtime.cpp', RUNTIME), ('main.cpp', MAIN)]:
            (directory/filename).write_text(contents)
        command = [args.xmake, 'f', '-y', '-m', 'release', '--toolchain=ownership-clang',
                   '--cc='+args.cc, '--cxx='+args.cxx, '--ld='+args.cxx]
        rc, output = run(command, directory, 'configure')
        assert rc == 0, output.decode(errors='replace')[-4000:]
        rc, output = run([args.xmake, '-y', '-v', '-j1', 'consumer'], directory, 'build')
        if name == 'single-owner':
            assert rc == 0, output.decode(errors='replace')[-6000:]
            executable = directory/'bin'/('consumer.exe' if os.name == 'nt' else 'consumer')
            rc, output = run([str(executable)], directory, 'execute')
            assert rc == 0, ('initializer must run exactly once', rc, output.decode(errors='replace'))
        elif name == 'duplicate-owner':
            assert rc != 0 and re.search(rb'duplicate symbol|multiple definition', output, re.I), output[-6000:]
        else:
            assert rc != 0 and (b'missing ownership dependency for module main.cpp' in output or
                re.search(rb"module ['\"]ownership['\"] (not found|not available)", output, re.I)), output[-6000:]
        print(name, 'PASS', flush=True)
    report['passed'] = True
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
