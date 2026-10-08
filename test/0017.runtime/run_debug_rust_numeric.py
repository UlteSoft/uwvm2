#!/usr/bin/env python3
"""Real wasm32 rustc const/type witnesses and seeded production-parser comparisons; cgroup only."""
from pathlib import Path
import argparse,hashlib,json,random,re,subprocess,sys,time
sys.dont_write_bytecode=True
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 ap=argparse.ArgumentParser()
 for n in ('binary','rustc','out'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--binary-sha256',required=True);a=ap.parse_args()
 root=Path(__file__).resolve().parents[2]
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 assert sha(a.binary)==a.binary_sha256
 a.out.mkdir(mode=0o700);casefile=root/'test/0018.debugger/dap_rust_numeric_cases.json'
 paths=[a.binary,a.rustc.resolve(),casefile,Path(__file__)]
 paths += [p for p in (a.rustc.parent.parent/'lib').rglob('*') if p.is_file() and not p.is_symlink()]
 pins={str(p):sha(p) for p in paths}
 result=dict(passed=False,pins_before=pins,commands=[],seed=20261008,full_language_parity=False,
             compiler_target='wasm32-unknown-unknown',native_linux_std_installed=False)
 def run(label,cmd,want=0):
  started=time.monotonic();done=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
  log=a.out/(label+'.log');log.write_bytes(done.stdout)
  result['commands'].append(dict(label=label,argv=cmd,returncode=done.returncode,expected=want,
                                log_sha256=sha(log),seconds=time.monotonic()-started))
  assert done.returncode==want,(label,done.stdout[-6000:])
  return done.stdout
 fixed=json.loads(casefile.read_text());witnesses=[]
 for expr,value,kind in fixed['valid']:
  if 'leaf_' in expr:continue
  typ=fixed.get('compiler_types',{}).get(expr) or ('bool' if kind=='bool' else 'char' if kind=='char' else 'f32' if kind=='float' else 'f64' if kind=='double' else re.search(r'[iu](?:size|8|16|32|64)',expr)[0])
  want=str(value) if kind=='bool' else str(value)+typ if typ in ('f32','f64') else str(value)
  test=f'assert!(actual as u32 == {value});' if typ=='char' else f'assert!(actual == {want});'
  witnesses.append(f'const _: () = {{ let actual: {typ} = {expr}; {test} }};')
 rng=random.Random(result['seed']);cases=[]
 for i in range(3000):
  width=rng.choice((8,16,32,64));uns=bool(rng.getrandbits(1));typ=('u' if uns else 'i')+str(width)
  val=rng.randrange(0,1<<width) if uns else rng.randrange(-(1<<(width-1)),1<<(width-1))
  mag=abs(val);raw=hex(mag)[2:] if i%3==0 else str(mag)
  sep='_'.join(raw)+'__' if i%2 else raw
  literal=('0x__' if i%3==0 else '')+sep+'_'+typ
  expr=('-(' + literal + ')') if val<0 else literal
  witnesses.append(f'const _: () = {{ let actual: {typ} = {expr}; assert!(actual == {val}); }};')
  cases.append((expr,val&((1<<width)-1),width,uns,'unsigned integer' if uns else 'signed integer'))

 literal_count=len(cases)
 # Every i8 bit pattern/count/direction and 2000 seeded wider/count-type cases.
 # Python floor division gives an oracle independent of guest sign-fill.
 shift_inputs=[(8,False,v,c,op) for v in range(-128,128) for c in range(8) for op in ('<<','>>')]
 for i in range(2000):
  width=rng.choice((8,16,32,64));uns=bool(rng.getrandbits(1))
  val=rng.randrange(0,1<<width) if uns else rng.randrange(-(1<<(width-1)),1<<(width-1))
  shift_inputs.append((width,uns,val,rng.randrange(width),rng.choice(('<<','>>'))))
 for i,(width,uns,val,count,op) in enumerate(shift_inputs):
  typ=('u' if uns else 'i')+str(width);ctyp=rng.choice(('i8','u8','i16','u16','i32','u32','i64','u64','isize','usize'))
  expr=f'({val}{typ}) {op} {count}{ctyp}'
  bits=(val<<count if op=='<<' else val//(1<<count))&((1<<width)-1)
  want=bits if uns or bits<(1<<(width-1)) else bits-(1<<width)
  witnesses.append(f'const _: () = {{ let actual: {typ} = {expr}; assert!(actual == {want}); }};')
  cases.append((expr,bits,width,uns,'unsigned integer' if uns else 'signed integer'))

 # Constraint anchors occur before, after and inside nested trees. The typed
 # oracle has the explicit root type, independently of literal token order.
 compound_start=len(cases)
 for i in range(2000):
  width=rng.choice((8,16,32,64));uns=bool(rng.getrandbits(1));typ=('u' if uns else 'i')+str(width)
  x,y,z=[rng.randrange(0,10) for _ in range(3)]
  forms=[(f'({x} + {y}) + {z}{typ}',x+y+z),
         (f'{z}{typ} + ({x} + {y})',x+y+z),
         (f'({x} + {y}) + ({z} + 0{typ})',x+y+z),
         (f'(({x} + {y}) << (1u8 + 1)) + 0{typ}',(x+y)<<2),
         (f'!({x} + {y}) + 0{typ}',~(x+y)),
         (f'(({x} + {y}) as {typ}) + {z}{typ}',x+y+z),
         (f'({x}.0 + {y}.0) + {z}.0f32',float(x+y+z))]
  expr,want=forms[i%len(forms)]
  floating=i%len(forms)==6
  if floating:
   import struct
   typ='f32';width=32;uns=False;bits=struct.unpack('<I',struct.pack('<f',want))[0];kind='float'
  else:bits=want&((1<<width)-1);kind='unsigned integer' if uns else 'signed integer'
  if not floating and uns:want=bits
  suffix='f32' if floating else ''
  witnesses.append(f'const _: () = {{ let actual: {typ} = {expr}; assert!(actual == {want}{suffix}); }};')
  cases.append((expr,bits,width,uns,kind))
 witnesses.append('const _: () = { let actual: f32 = (1.0000000596046448 + 0.0) + 0f32; assert!(actual.to_bits() == 1065353217); };')
 cases.append(('(1.0000000596046448 + 0.0) + 0f32',1065353217,32,False,'float'))
 source=a.out/'valid.rs';source.write_text('\n'.join(witnesses)+'\n')
 run('rust-valid-type-value-build',[str(a.rustc),'--edition=2024','--target=wasm32-unknown-unknown','--crate-type=lib','--emit=metadata',str(source),'-o',str(a.out/'valid.rmeta')])
 invalid=[x for x in fixed['invalid'] if x not in ('1u128','1i128')]
 for i,expr in enumerate(invalid):
  source=a.out/f'invalid-{i:02}.rs';source.write_text('const _: () = { let _ = '+expr+'; };\n')
  run(f'rust-invalid-{i:02}',[str(a.rustc),'--edition=2024','--target=wasm32-unknown-unknown','--emit=metadata',str(source),'-o',str(a.out/f'invalid-{i:02}.rmeta')],1)
 for start in range(0,len(cases),100):
  batch=cases[start:start+100];raw=run(f'property-{start//100:03}',[str(a.binary),*(c[0] for c in batch)])
  rows=raw.decode().splitlines();assert len(rows)==len(batch)
  for i,(row,c) in enumerate(zip(rows,batch)):
   fs=row.split('\t');actual=(int(fs[0]),int(fs[1]),int(fs[2]),int(fs[3]),fs[4]=='1',fs[5])
   expected=(i,0,c[1],c[2],c[3],c[4]);assert actual==expected,(c,actual,expected)
 result.update(passed=True,cases=len(cases),valid_compiler_type_value_witnesses=len(witnesses),
               invalid_compiler_witnesses=len(invalid),literal_cases=literal_count,shift_cases=len(shift_inputs),compound_cases=len(cases)-compound_start,exhaustive_i8_shift_cases=4096,pins_after={p:sha(p) for p in pins})
 assert pins==result['pins_after']
 (a.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print('PASS Rust numeric compiler/parser',len(cases),'seeded cases',flush=True)
if __name__=='__main__':main()
