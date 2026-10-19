#!/usr/bin/env python3
"""Work around the pinned .NET 8 WASI target's unconditional stripping."""
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

build_dir = Path(sys.argv[1])
compiler = sys.argv[2]
assembly = sys.argv[3] if len(sys.argv) > 3 else 'highlevel-csharp'
configuration = sys.argv[4] if len(sys.argv) > 4 else 'Release'
if configuration not in ('Debug', 'Release'):
    raise ValueError('Select Debug or Release')
response_file = build_dir / ('obj/' + configuration + '/net8.0/wasi-wasm/wasm/for-publish/clang-compile.rsp')
arguments = shlex.split(response_file.read_text())
if arguments.count("-Wl,-s") != 1:
    raise RuntimeError("Pinned .NET WASI linker recipe changed; inspect strip flags before proceeding")
arguments.remove("-Wl,-s")
arguments.insert(0, "-g")
arguments[arguments.index("-o") + 1] = str(build_dir / "AppBundle" / (assembly + '.wasm'))
(build_dir / "symbol-link-command.json").write_text(json.dumps([compiler] + arguments, indent=2))
subprocess.run([compiler] + arguments, check=True)
for symbols in (build_dir / ('bin/' + configuration + '/net8.0/wasi-wasm')).glob(assembly + '.pdb'):
    shutil.copy2(symbols, build_dir / "AppBundle" / symbols.name)
