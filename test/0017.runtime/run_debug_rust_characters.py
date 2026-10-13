#!/usr/bin/env python3
"""Wasm-target Rust const witnesses plus seeded production-parser comparisons. Run only in test cgroup."""
from pathlib import Path
import argparse,hashlib,json,random,subprocess,time,sys
sys.dont_write_bytecode=True
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--binary',type=Path,required=True);ap.add_argument('--binary-sha256',required=True)
 ap.add_argument('--rustc',type=Path,required=True);ap.add_argument('--linker',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 assert sha(a.binary)==a.binary_sha256
 a.out.mkdir(mode=0o700);witness=root/'test/0017.runtime/debug_rust_character_witness.rs'
 paths=[a.binary,a.rustc.resolve(),a.linker.resolve(),witness,Path(__file__)]
 for p in (a.rustc.parent.parent/'lib').rglob('*'):
  if p.is_file() and not p.is_symlink():paths.append(p)
 pins={str(p):sha(p) for p in paths};result=dict(passed=False,pins_before=pins,commands=[],cases=0,seed=20261008,full_language_parity=False)
 def run(label,cmd,want=0):
  started=time.monotonic();done=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
  log=a.out/(label+'.log');log.write_bytes(done.stdout)
  result['commands'].append(dict(label=label,argv=cmd,returncode=done.returncode,expected=want,log_sha256=sha(log),seconds=time.monotonic()-started))
  assert done.returncode==want,(label,done.stdout[-5000:])
  return done.stdout
 run('rust-valid-build',[str(a.rustc),'--edition=2024','--target=wasm32-unknown-unknown','--crate-type=lib','--emit=metadata',str(witness),'-o',str(a.out/'rust-witness.rmeta')])
 invalid=[r"'\x1'",r"'\x80'",r"'\123'",r"'\a'",r"'\u{_41}'",r"'\u{0000000}'",r"'\u{d800}'",r"'\u{110000}'",
 "'a'+1","+'a'","-'a'","!'a'","'a' & 'b'","'a' << 1","'a' == 97u32","'a' as f32","'a' as bool",
 "65u32 as char","65i8 as char","true as char","1.0 as char","char(65u8)","false && ('a'+1 == 0)","true || ('a' as f32 == 0.0)"]
 for i,expression in enumerate(invalid):
  source=a.out/f'invalid-{i:02}.rs';source.write_text('fn main(){let _ = '+expression+';}\n')
  log=run(f'rust-invalid-{i:02}',[str(a.rustc),'--edition=2024','--target=wasm32-unknown-unknown','--emit=metadata',str(source),'-o',str(a.out/f'invalid-{i:02}.rmeta')],1)
  assert b'error' in log
 rng=random.Random(20261008);cases=[]
 def scalar():
  while True:
   n=rng.randrange(0x110000)
   if not 0xd800<=n<=0xdfff:return n
 for i in range(2000):
  n=scalar();m=scalar();raw=f'{n:x}';sep='_'.join(raw)+'__';literal="'\\u{"+(sep if i%2 else raw)+"}'"
  cases.extend([(literal,n,32,True,'Rust char'),(literal+' as u8',n&255,8,True,'unsigned integer'),
   (literal+' as i8',n&255,8,False,'signed integer'),(literal+' as u32',n,32,True,'unsigned integer'),
   (literal+" < '\\u{"+f'{m:x}'+"}'",int(n<m),8,True,'bool')])
 for start in range(0,len(cases),100):
  batch=cases[start:start+100];out=run(f'property-{start//100:03}',[str(a.binary),*(c[0] for c in batch)])
  rows=out.decode().splitlines();assert len(rows)==len(batch)
  for i,(row,c) in enumerate(zip(rows,batch)):
   fields=row.split('\t');assert len(fields)==6,(row,c)
   actual=(int(fields[0]),int(fields[1]),int(fields[2]),int(fields[3]),fields[4]=='1',fields[5])
   expected=(i,0,c[1],c[2],c[3],c[4])
   assert actual==expected,(c,actual,expected)
 result.update(passed=True,cases=len(cases),invalid_compiler_witnesses=len(invalid),pins_after={p:sha(p) for p in pins})
 assert pins==result['pins_after']
 (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print('PASS Rust char compiler/production parser',len(cases),'cases',len(invalid),'negative compiler witnesses',flush=True)
if __name__=='__main__':main()
