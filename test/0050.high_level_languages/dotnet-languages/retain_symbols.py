#!/usr/bin/env python3
"""Relink .NET 8.0.15 WASI output with native symbols, for either configuration."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_dir', type=Path)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('assembly')
    parser.add_argument('configuration', choices=('Debug', 'Release'))
    args = parser.parse_args()
    build = args.build_dir.resolve(strict=True)
    compiler = args.compiler.resolve(strict=True)
    response = build / 'obj' / args.configuration / 'net8.0/wasi-wasm/wasm/for-publish/clang-compile.rsp'
    arguments = shlex.split(response.read_text())
    if arguments.count('-Wl,-s') != 1 or arguments.count('-o') != 1:
        raise ValueError('Pinned .NET WASI linker recipe changed; inspect before relinking')
    arguments.remove('-Wl,-s')
    arguments.insert(0, '-g')
    arguments[arguments.index('-o') + 1] = str(build / 'AppBundle' / (args.assembly + '.wasm'))
    (build / 'symbol-link-command.json').write_text(json.dumps([str(compiler)] + arguments, indent=2) + '\n')
    subprocess.run([str(compiler)] + arguments, check=True)
    for symbols in (build / 'bin' / args.configuration / 'net8.0/wasi-wasm').glob(args.assembly + '.pdb'):
        shutil.copy2(symbols, build / 'AppBundle' / symbols.name)


if __name__ == '__main__':
    main()
