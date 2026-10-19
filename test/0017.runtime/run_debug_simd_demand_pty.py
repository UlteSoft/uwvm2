#!/usr/bin/env python3
"""Real cold SIMD stops, complete bit patterns, GC coexistence and policy changes.

Execute only in the verified Linux cgroup; fixture authorship is not execution.
"""
import argparse, hashlib, json, os, re, struct, subprocess, sys
from pathlib import Path
from run_debug_run_cli import Console
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '0018.debugger'))
from run_wasm_operand_preview_cli import markers


def vector(data): return 'v128 = bytes=' + data.hex()
def ints(values, bits=32):
    return vector(b''.join((x & ((1 << bits)-1)).to_bytes(bits//8, 'little') for x in values))
def floats(values, bits=32): return vector(struct.pack('<'+('f' if bits==32 else 'd')*len(values), *values))
A='v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15'
B='v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0'
a_bytes=bytes(range(16));b_bytes=bytes(reversed(range(16)))
CASES=[
 ('const','', 'v128.const i32x4 1 -2 3 -4',lambda i:[],lambda i:ints([1,-2,3,-4])),
 ('splat','local.get 0','i32x4.splat',lambda i:['i32 = '+str(i)],lambda i:ints([i]*4)),
 ('add','local.get 1 v128.const i32x4 1 2 3 4','i32x4.add',lambda i:[ints([i]*4),ints([1,2,3,4])],lambda i:ints([i+1,i+2,i+3,i+4])),
 ('mul64','v128.const i64x2 -3 4 v128.const i64x2 7 -8','i64x2.mul',lambda i:[ints([-3,4],64),ints([7,-8],64)],lambda i:ints([-21,-32],64)),
 ('shuffle',A+' '+B,'i8x16.shuffle 0 16 1 17 2 18 3 19 4 20 5 21 6 22 7 23',lambda i:[vector(a_bytes),vector(b_bytes)],lambda i:vector(bytes([0,15,1,14,2,13,3,12,4,11,5,10,6,9,7,8]))),
 ('bitselect',A+' '+B+' v128.const i32x4 -1 0 -1 0','v128.bitselect',lambda i:[vector(a_bytes),vector(b_bytes),ints([-1,0,-1,0])],lambda i:vector(a_bytes[:4]+b_bytes[4:8]+a_bytes[8:12]+b_bytes[12:])),
 ('floatadd','v128.const f32x4 1 2 3 4 v128.const f32x4 0.5 -1 2 4','f32x4.add',lambda i:[floats([1,2,3,4]),floats([.5,-1,2,4])],lambda i:floats([1.5,1,5,8])),
 ('sqrt64','v128.const f64x2 1 4','f64x2.sqrt',lambda i:[floats([1,4],64)],lambda i:floats([1,2],64)),
 ('saturate','v128.const f32x4 nan inf -inf -7.75','i32x4.trunc_sat_f32x4_s',lambda i:None,lambda i:ints([0,2147483647,-2147483648,-7])),
 ('relaxedswizzle',A+' '+B,'i8x16.relaxed_swizzle',lambda i:[vector(a_bytes),vector(b_bytes)],lambda i:vector(b_bytes)),
 ('relaxedmadd','v128.const f32x4 1 2 3 4 v128.const f32x4 2 3 4 5 v128.const f32x4 3 4 5 6','f32x4.relaxed_madd',lambda i:[floats([1,2,3,4]),floats([2,3,4,5]),floats([3,4,5,6])],lambda i:floats([5,10,17,26])),
 ('relaxedtrunc','v128.const f64x2 1.25 -7.75','i32x4.relaxed_trunc_f64x2_s_zero',lambda i:[floats([1.25,-7.75],64)],lambda i:ints([1,-7,0,0])),
 ('relaxedselect',A+' '+B+' v128.const i32x4 -1 0 -1 0','i32x4.relaxed_laneselect',lambda i:[vector(a_bytes),vector(b_bytes),ints([-1,0,-1,0])],lambda i:vector(a_bytes[:4]+b_bytes[4:8]+a_bytes[8:12]+b_bytes[12:])),
]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm','wasm-tools','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
    result={'passed':False,'cases':[],'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'cgroup':Path('/proc/self/cgroup').read_text().strip(),'affinity':sorted(os.sched_getaffinity(0))}
    for gc in (False,True):
      for name,inputs,operation,before,after in CASES:
        dest=a.out/(name+('-gc' if gc else '-numeric'));dest.mkdir()
        extra='(type $s (struct (field (mut i32))))' if gc else ''
        local='(local (ref $s))' if gc else ''
        initialize='i32.const 7 struct.new $s local.set 2' if gc else ''
        wat=dest/'fixture.wat';wasm=dest/'fixture.wasm'
        wat.write_text(f'''(module {extra}
         (func (export "_start") (local i32) (local v128) {local}
          nop {initialize} loop
           local.get 0 i32x4.splat local.set 1
           {inputs} nop {operation} local.set 1
           local.get 0 i32.const 1 i32.add local.tee 0 i32.const 100 i32.lt_u br_if 0
          end local.get 1 nop drop))''')
        for command in ([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],[str(a.wasm_tools),'validate',str(wasm)]):subprocess.run(command,check=True)
        sites,_=markers(a.wasm_tools,wasm);assert len(sites[0])==3,sites
        target=sites[0][1]+1;final=sites[0][2];actions=[]
        c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rclog','err','-WFE-simd','-WFE-relaxed-simd',*(['-WFE-reference-types','-WFE-function-references','-WFE-gc'] if gc else []),'--run',str(wasm)])
        def ask(text):
            at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,90)
            assert b'error:' not in reply,reply[-4000:];actions.append({'command':text,'reply':reply.decode(errors='replace')});return reply
        def operands(want):
            reply=ask('operands 1 0 0 16');got=re.findall(rb'^operand [0-9]+ (.*)\r?$',reply,re.M)
            if want is not None:assert [v.decode().strip() for v in got]==want,(name,want,reply)
            else:assert len(got)==1 and got[0].startswith(b'v128 = bytes='),reply
        def values(i,offset):
            reply=ask('status');assert f'function=0 byte-offset={offset} '.encode() in reply,reply
            reply=ask('locals wasm 1 0 0 '+('3' if gc else '2'))
            assert f'local 0 i32 = {i}'.encode() in reply and ('local 1 '+ints([i]*4)).encode() in reply,reply
            if gc:
                assert b'local 2 (ref type-index=0 module=0) = struct #' in reply,reply
                assert b'  0 i32 = 7 mutable' in ask('members locals 1 0 0 0 2 0 1')
            operands(before(i))
        try:
            c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
            assert Path(f'/proc/{c.pid}/cgroup').read_text().strip()==result['cgroup'] and sorted(os.sched_getaffinity(c.pid))==result['affinity']
            ask('startw');bp=re.search(rb'breakpoint (\d+)',ask(f'bw 0 0 {target}'))[1].decode();finish=re.search(rb'breakpoint (\d+)',ask(f'bw 0 0 {final}'))[1].decode()
            ask('ignore '+bp+' 39');ask('c');values(39,target)
            ask('condition '+bp+' 0');reply=ask('c');assert b'breakpoint-condition' in reply and b'unavailable' in reply,reply;values(40,target)
            ask('condition '+bp);ask('nw');operands([after(40)])
            ask('ni');ask('nw');ask('disable '+bp);ask('c')
            assert f'function=0 byte-offset={final} '.encode() in ask('status');operands([after(99)])
            ask('disable '+finish);ask('c');assert b'Program exited normally.' in ask('status')
            c.finish();row={'name':name,'live_gc_local':gc,'passed':True,'target_offset':target,'wasm_sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'actions':actions};result['cases'].append(row)
            print(name,'GC' if gc else 'numeric','PASS',flush=True)
        finally:(dest/'console.bin').write_bytes(c.log);(dest/'actions.json').write_text(json.dumps(actions,indent=2));c.close();(a.out/'result.json').write_text(json.dumps(result,indent=2))
    result['passed']=len(result['cases'])==2*len(CASES);(a.out/'result.json').write_text(json.dumps(result,indent=2));assert result['passed']

if __name__=='__main__':main()
