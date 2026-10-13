#!/usr/bin/env python3
"""Prepare and oracle-check a same-ABI cross-function EH replacement for Win11."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


NAME = 'eh-hot-replace-cross-function'
OLD = 'i32.const 29\n    throw $event'
NEW = 'i32.const 30\n    throw $event'
CPUS = sorted([0, 2, 4, 6, *range(16, 32)])


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def cgroup(scope: str) -> dict[str, object]:
    require(bool(re.fullmatch(r'docker-[0-9a-f]{64}\.scope', scope)), 'invalid Docker cgroup')
    base = Path('/sys/fs/cgroup/system.slice') / scope
    require((base / 'memory.max').read_text().strip() == '68719476736' and
            (base / 'memory.swap.max').read_text().strip() == '0' and
            sorted(os.sched_getaffinity(0)) == CPUS and
            str(os.getpid()) in (base / 'cgroup.procs').read_text().splitlines(),
            'EH replacement oracle must run inside the 64 GiB/20 CPU cgroup')
    return {'scope': scope, 'memory_max': '68719476736', 'swap_max': '0', 'cpus': CPUS}


def uleb(data: bytes, at: int, end: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        require(at < end, 'truncated Wasm LEB')
        byte = data[at]
        at += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, at
    raise ValueError('overlong Wasm LEB')


def sections(data: bytes) -> dict[int, bytes]:
    require(data[:8] == b'\0asm\x01\0\0\0', 'invalid Wasm module header')
    result = {}
    at = 8
    while at < len(data):
        section = data[at]
        at += 1
        size, at = uleb(data, at, len(data))
        end = at + size
        require(end <= len(data), 'truncated Wasm section')
        if section != 0:
            require(section not in result, 'duplicate Wasm section')
            result[section] = data[at:end]
        at = end
    return result


def exports(payload: bytes) -> dict[str, tuple[int, int]]:
    at = 0
    count, at = uleb(payload, at, len(payload))
    result = {}
    for _ in range(count):
        length, at = uleb(payload, at, len(payload))
        end = at + length
        require(end <= len(payload), 'truncated Wasm export')
        name = payload[at:end].decode('utf-8')
        at = end
        require(at < len(payload), 'truncated Wasm export kind')
        kind = payload[at]
        at += 1
        index, at = uleb(payload, at, len(payload))
        result[name] = (kind, index)
    require(at == len(payload), 'trailing Wasm export bytes')
    return result


def bodies(payload: bytes) -> list[bytes]:
    at = 0
    count, at = uleb(payload, at, len(payload))
    result = []
    for _ in range(count):
        size, at = uleb(payload, at, len(payload))
        end = at + size
        require(end <= len(payload), 'truncated Wasm code body')
        result.append(payload[at:end])
        at = end
    require(at == len(payload), 'trailing Wasm code bytes')
    return result


def run(command: list[str], output: Path, name: str, expected: bytes | None = None) -> dict[str, object]:
    process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    stdout = output / f'{name}.stdout'
    stderr = output / f'{name}.stderr'
    stdout.write_bytes(process.stdout)
    stderr.write_bytes(process.stderr)
    require(process.returncode == 0 and (expected is None or process.stdout == expected),
            f'{name}: exit {process.returncode}, stdout {process.stdout!r}, stderr {process.stderr!r}')
    return {'command': command, 'exit_code': process.returncode,
            'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    group = cgroup(args.docker_scope)
    wat = args.source_root.resolve(strict=True) / 'test/0017.runtime/fixtures' / (NAME + '.wat')
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or output.is_relative_to(Path('/dev/shm')),
            'EH replacement oracle output must be temporary')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    source = wat.read_text()
    require(source.count(OLD) == 1, 'replacement target must be unique')
    replacement_wat = output / (NAME + '-30.wat')
    replacement_wat.write_text(source.replace(OLD, NEW, 1))
    original = output / (NAME + '.wasm')
    replacement = output / (NAME + '-30.wasm')
    checks = [run([str(wasm_tools), 'parse', str(wat), '-o', str(original)], output, 'parse-original'),
              run([str(wasm_tools), 'parse', str(replacement_wat), '-o', str(replacement)], output, 'parse-replacement'),
              run([str(wasm_tools), 'validate', '--features', 'all', str(original)], output, 'validate-original'),
              run([str(wasm_tools), 'validate', '--features', 'all', str(replacement)], output, 'validate-replacement'),
              run([str(wasmtime), 'run', '-C', 'cache=n', '-W', 'exceptions=y', str(original)],
                  output, 'wasmtime-original', b'eh-replaced-value=29\n'),
              run([str(wasmtime), 'run', '-C', 'cache=n', '-W', 'exceptions=y', str(replacement)],
                  output, 'wasmtime-replacement', b'eh-replaced-value=30\n')]
    before = sections(original.read_bytes())
    after = sections(replacement.read_bytes())
    require(before.keys() == after.keys() and 7 in before and 10 in before and
            all(before[key] == after[key] for key in before if key != 10),
            'replacement changed a Wasm module section beyond code')
    public = exports(before[7])
    require(public.get('hot_target') == (0, 1) and public.get('_start') == (0, 3),
            'replacement target or entry public function index changed')
    old_bodies = bodies(before[10])
    new_bodies = bodies(after[10])
    require(len(old_bodies) == len(new_bodies) == 3 and old_bodies[1:] == new_bodies[1:] and
            old_bodies[0] == b'\x00\x41\x1d\x08\x00\x0b' and
            new_bodies[0] == b'\x00\x41\x1e\x08\x00\x0b',
            'same-ABI EH replacement body differs from the qualified 29-to-30 throw')
    body = output / (NAME + '-30.bin')
    body.write_bytes(new_bodies[0])
    files = {path.name: sha(path) for path in sorted(output.iterdir()) if path.is_file()}
    manifest = {'schema': 1, 'status': 'wasm-tools-and-wasmtime-oracle-passed',
                'cgroup': group, 'source_wat_sha256': sha(wat),
                'wasm_tools_sha256': sha(wasm_tools), 'wasmtime_sha256': sha(wasmtime),
                'target_function_index': 1, 'entry_function_index': 3,
                'replacement_generation': 1,
                'files_sha256': files, 'checks': checks}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'status': manifest['status'], 'source_wat_sha256': manifest['source_wat_sha256'],
                      'original_wasm_sha256': files[original.name], 'body_sha256': files[body.name]}, sort_keys=True))


if __name__ == '__main__':
    main()
