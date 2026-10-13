import json,subprocess
from pathlib import Path
work=Path(__file__).parent
receipt=json.loads((work/'ros-config9.json').read_text())
if receipt['returncode'] != 0 or receipt['stop_reason'] is not None: raise RuntimeError('LLVM source build did not complete; do not blindly retry the same failed build')
import os
os.environ["XMAKE_CONFIGDIR"]='/tmp/uwvm2-xmake-local-20261008/uwvm2-ros'
commands=[['xmake', 'f', '-y', '-v', '-o', '/Users/liyinan/Documents/MacroModel/src/uwvm2-ros/build/xmake-local-20261008-bundled', '--build-source-id=none', '--llvm-build-jobs=1', '--llvm-runtimes-provider=bundled', '--ccache=n', '--use-llvm-compiler=y', '--enable-lto=n', '--execution-int=uwvm-int', '--execution-jit=llvm', '--cc=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang', '--cxx=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--ld=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--sysroot=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/aarch64-apple-darwin24', '--target=detect', '--llvm-target=detect'], ['xmake', '-j1', '-v']]
for command in commands:
 print('COMMAND',command,flush=True)
 subprocess.run(command,check=True)
