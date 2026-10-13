import subprocess
from pathlib import Path
repo=Path('/Users/liyinan/Documents/MacroModel/src/uwvm2-ros')
w=Path(__file__).parent
subprocess.run(['python3',str(repo/'test/0018.build/check_macos_paired_runtime.py'),str(repo/'build/bundled-runtimes/c35e40c15c70e09862cc048ffa017c0e93551ec4a1d61fcf381bf2a14fe6499b'),str(w/'paired-runtime-final')],check=True)
