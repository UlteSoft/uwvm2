#!/usr/bin/env python3
"""Apply short LEB decoding to a private, pinned LLVM 23.1.1 source copy."""
import argparse, hashlib, json, pathlib, subprocess
p=argparse.ArgumentParser(__doc__)
p.add_argument('source',type=pathlib.Path,help='LLVM source root containing libunwind')
p.add_argument('--check',action='store_true')
a=p.parse_args();root=pathlib.Path(__file__).resolve().parent
m=json.loads((root/'short-leb.json').read_text());f=a.source.resolve()/m['path']
patch=root/'short-leb.patch'
if hashlib.sha256(patch.read_bytes()).hexdigest()!=m['patch_sha256']:
 raise SystemExit('patch identity mismatch')
sha=hashlib.sha256(f.read_bytes()).hexdigest()
if sha==m['patched_sha256']:
 print('already patched:',f);raise SystemExit(0)
if sha!=m['upstream_sha256']:raise SystemExit('refusing unknown source preimage: '+str(f))
if a.check:print('verified upstream preimage:',f);raise SystemExit(0)
f.chmod(f.stat().st_mode|0o200)
subprocess.run(['patch','--batch','--forward','-p1','-i',str(patch)],cwd=a.source,check=True)
if hashlib.sha256(f.read_bytes()).hexdigest()!=m['patched_sha256']:
 raise SystemExit('patched source identity mismatch')
print('applied:',f)
