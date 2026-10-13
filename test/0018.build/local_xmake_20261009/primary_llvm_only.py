import os,subprocess
os.environ["XMAKE_CONFIGDIR"]='/tmp/uwvm2-xmake-local-20261008/uwvm2-llvm-only'
commands=[['/opt/homebrew/bin/xmake', 'f', '-y', '-v', '-o', 'build/xmake-local-20261008-external-llvm-only', '--build-source-id=none', '--execution-int=none', '--execution-jit=llvm', '--use-llvm-compiler=y', '--enable-lto=n', '--use-cxx-module=n', '--ccache=n', '--cc=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang', '--cxx=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--ld=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++', '--sysroot=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/aarch64-apple-darwin24', '--unwindlib=force-libunwind', '--openssl-root=/opt/homebrew/opt/openssl@3', '--toolchain=clang', '--macos-4gib-test=y'], ['/opt/homebrew/bin/xmake', '-j1', '-v']]
for command in commands:
 print("COMMAND",command,flush=True)
 subprocess.run(command,check=True)
