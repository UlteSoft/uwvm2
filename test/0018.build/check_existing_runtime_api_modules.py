#!/usr/bin/env python3
"""Replay the real runtime API/host API module commands with an ownership negative.

The input is a JSON report with rows containing compiler commands from a module
semantic preflight. Existing dependency BMIs must still match the compiler and
source configuration. New outputs stay in --out, never the production BMI cache.
This checks declarations, not linking, VM execution, or arbitrary module graphs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--commands-json', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    repo = args.source_root.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = json.loads(args.commands_json.read_text())['rows']
    api_source = 'src/uwvm2/runtime/lib/uwvm_runtime.cppm'
    api = next(row['command'] for row in rows if row['passed'] and row['command'][-1] == api_source)
    host = next(row['command'] for row in rows if row['passed'] and
                row['command'][-1] == 'src/uwvm2/uwvm/host_api.module.cpp')
    assert '--precompile' in api and '-fsyntax-only' in host
    results = []

    def run(command, label, error=None):
        result = subprocess.run(command, cwd=repo, capture_output=True, timeout=180)
        (out/(label+'.log')).write_bytes(result.stdout+result.stderr)
        passed = result.returncode == 0 if error is None else result.returncode != 0 and error in result.stderr
        results.append(dict(name=label, command=command, returncode=result.returncode, passed=passed))
        (out/'results.json').write_text(json.dumps(dict(note=__doc__, rows=results,
            inputs={path: hashlib.sha256((repo/path).read_bytes()).hexdigest() for path in [
                api_source, 'src/uwvm2/runtime/lib/uwvm_runtime.h', 'src/uwvm2/uwvm/wasm/type/preload_api.h']},
            commands_sha256=hashlib.sha256(args.commands_json.read_bytes()).hexdigest()), indent=2)+'\n')
        print(label, 'PASS' if passed else 'FAIL', flush=True)
        if not passed:
            print(result.stderr.decode(errors='replace')[-8000:], flush=True)
            raise SystemExit(1)

    def api_command(source, output):
        command = list(api)
        command[command.index('-o')+1] = str(output)
        command[-1] = str(source)
        command.insert(1, '-iquote'+str((repo/api_source).parent))
        return command

    def host_command(bmi):
        prefix = '-fmodule-file=uwvm2.runtime='
        assert sum(arg.startswith(prefix) for arg in host) == 1
        return [prefix+str(bmi) if arg.startswith(prefix) else arg for arg in host]

    positive = out/'runtime-positive.pcm'
    run(api_command(repo/api_source, positive), 'api-positive')
    run(host_command(positive), 'host-positive')

    text = (repo/api_source).read_text(encoding='utf-8')
    owner_import = 'import uwvm2.uwvm.wasm.type;'
    assert text.count(owner_import) == 1
    negative_source = out/'runtime-negative.cppm'
    # Restore the old module-owned forward declaration, not a different layout.
    # Its BMI alone is valid; the defect appears only when both owners are used.
    negative_source.write_text(text.replace(owner_import,
        'export namespace uwvm2::uwvm::wasm::type { struct uwvm_preload_memory_descriptor_t; }'))
    negative = out/'runtime-negative.pcm'
    run(api_command(negative_source, negative), 'api-negative-setup')
    run(host_command(negative), 'host-ownership-negative', b'cannot be attached to other modules')

    # Also retain the cheap incomplete-type API in textual builds, in either
    # include order. This must not depend on importing a named module at all.
    flags = []
    skip = False
    for arg in api[:-1]:
        if skip:
            skip = False
        elif arg in ('-o', '-x'):
            skip = True
        elif arg not in ('-c', '--precompile', '-DUWVM_MODULE') and not arg.startswith('-fmodule-file='):
            flags.append(arg)
    headers = ['uwvm2/runtime/lib/uwvm_runtime.h', 'uwvm2/uwvm/wasm/type/preload_api.h']
    for label, order in [('header-api-first', headers), ('header-type-first', headers[::-1])]:
        source = out/(label+'.cpp')
        source.write_text(''.join('#include <'+path+'>\n' for path in order)+
            'using descriptor = uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t;\n'
            'using api_type = bool (*)(std::size_t, descriptor*) noexcept;\n'
            'static_assert(__is_same(decltype(&uwvm2::runtime::lib::preload_memory_descriptor_at_host_api), api_type));\n')
        run([*flags, '-fsyntax-only', '-x', 'c++', str(source)], label)


if __name__ == '__main__':
    main()
