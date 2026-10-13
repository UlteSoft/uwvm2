#!/usr/bin/env python3
"""Regression for source-path provenance in isolated Wasm 3 CLI builds."""

from pathlib import Path
import tempfile

from requalify_wasm3_cli_from_provenance import bind_source_paths


UNITS = (
    'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp',
    'src/uwvm2/uwvm/main.default.cpp',
    'src/uwvm2/uwvm/host_api.default.cpp',
)


def create_source(root):
    for unit in UNITS:
        path = root / unit
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('// provenance fixture\n')


def run():
    with tempfile.TemporaryDirectory(prefix='uwvm-source-binding-') as directory:
        base = Path(directory)
        old, current, unrelated = (base / name for name in ('old', 'current', 'unrelated'))
        for root in (old, current, unrelated):
            create_source(root)

        runtime = ['clang++', '-I', str(old / 'src'), '-c', str(old / UNITS[0])]
        cli = ['clang++', '-I' + str(old / 'src'),
               str(old / UNITS[1]), str(old / UNITS[2])]
        assert bind_source_paths(runtime, cli, current) == str(old)
        assert str(current / UNITS[0]) in runtime
        assert str(current / UNITS[1]) in cli
        assert str(current / UNITS[2]) in cli
        assert str(current / 'src') in runtime
        assert '-I' + str(current / 'src') in cli
        assert not any(str(old) in token for token in runtime + cli)

        runtime = ['clang++', '-I', 'src', '-c', UNITS[0]]
        cli = ['clang++', '-I', 'src', UNITS[1], UNITS[2]]
        assert bind_source_paths(runtime, cli, current) is None
        assert UNITS[0] in runtime and UNITS[1] in cli

        runtime = ['clang++', '-c', str(old / UNITS[0])]
        cli = ['clang++', str(unrelated / UNITS[1]), str(old / UNITS[2])]
        try:
            bind_source_paths(runtime, cli, current)
        except RuntimeError as error:
            assert 'escapes selected source' in str(error)
        else:
            raise AssertionError('mixed old/unrelated translation units were accepted')

    print('PASS Wasm 3 compiler source binding: absolute, relative, mixed-source')


if __name__ == '__main__':
    run()
