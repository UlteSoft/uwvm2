#!/usr/bin/env python3
# v7 SOURCE successor; this is DATA ABI separation, not authenticated native cache replay.
"""Fixture cache DATA metadata separates mmap/moving/single-thread native-memory ABIs."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for backend in ['mmap','allocator','single']:p.add_argument('--'+backend,type=Path,required=True)
p.add_argument('--out',type=Path,required=True);a=p.parse_args()
subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False);rows=[]
binaries={key:getattr(a,key) for key in ['mmap','allocator','single']}
def run(backend,*args):
 command=[str(binaries[backend]),*map(str,args)];r=subprocess.run(command,capture_output=True,text=True,timeout=30)
 rows.append(dict(command=command,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
 (a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
 assert r.returncode==0,rows[-1]
 assert 'codec_data_only=1' in r.stdout and 'native_cache_replay=0' in r.stdout
 if args:assert 'standalone_persistent_refused=1' in r.stdout
 return dict(line.split('=',1) for line in r.stdout.splitlines() if '=' in line)
defaults={key:run(key) for key in binaries}
assert len({v['product'] for v in defaults.values()})==1
assert len({v['abi-hex'] for v in defaults.values()})==3
for backend,label in [('mmap','mmap'),('allocator','allocator-concurrent'),('single','allocator-single')]:
 abi=bytes.fromhex(defaults[backend]['abi-hex']);assert b'native-memory-backend' in abi and label.encode() in abi
for mode in ['native-signed','native-unsigned']:
 models={key:run(key,'write',a.out/mode/'shared',mode) for key in binaries}
 own={key:Path(model['path']) for key,model in models.items()}
 assert all(not path.exists() for path in own.values())
 assert len(set(own.values()))==3
 for key in binaries:
  for foreign in binaries:
   if key==foreign:continue
   result=run(key,'load-foreign',a.out/mode/key/foreign,mode,models[foreign]['blob-hex'],'none','context-mismatch')
   assert result['status']=='context-mismatch'
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,codec_data_only=True,native_cache_replay=False,invocations=len(rows),product=next(iter(defaults.values()))['product'],binary_sha256={k:hashlib.file_digest(v.open('rb'),'sha256').hexdigest() for k,v in binaries.items()}),indent=2)+'\n')
print('PASS 21 native memory ABI DATA checks: three builds, signed/unsigned DATA objects, all six DATA comparison directions')
