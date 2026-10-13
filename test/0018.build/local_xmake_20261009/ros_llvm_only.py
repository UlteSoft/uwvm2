import os,json,subprocess
from pathlib import Path
w=Path(__file__).parent
r=json.loads((w/"ros-config9.json").read_text())
assert r["returncode"]==0 and r["stop_reason"] is None
os.environ["XMAKE_CONFIGDIR"]='/tmp/uwvm2-xmake-local-20261008/ros-llvm-only'
commands=[['xmake', 'f', '-y', '-v', '-o', '/Users/liyinan/Documents/MacroModel/src/uwvm2-ros/build/xmake-local-20261008-bundled-llvm-only', '--build-source-id=none', '--llvm-build-jobs=1', '--llvm-runtimes-provider=bundled', '--ccache=n', '--use-llvm-compiler=y', '--enable-lto=n', '--execution-int=none', '--execution-jit=llvm', '--cc=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang', '--cxx=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--ld=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--sysroot=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/aarch64-apple-darwin24', '--target=detect', '--llvm-target=detect', '--macos-4gib-test=y', '--use-cxx-module=n'], ['xmake', '-j1', '-v']]
for command in commands:
 print("COMMAND",command,flush=True)
 subprocess.run(command,check=True)
