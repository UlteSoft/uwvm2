#!/usr/bin/env python3
"""Independent SIMD arithmetic workload and scalar checksum, for the Linux cgroup.

WAT authorship is allowed locally. --compile performs all tool execution only
inside the required cgroup. Each kernel repetition resets the four vector lanes.
"""
import argparse, hashlib, json, subprocess
from pathlib import Path
from make_debug_core3_feature_workloads import PRELUDE, EPILOGUE


def checksum(iterations):
    lanes=[1,2,3,4]
    for _ in range(iterations):
        lanes=[(((v*1664525+1013904223)&0xffffffff)^(v>>13)) for v in lanes]
    return sum(lanes)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--iterations',type=int,default=10000)
    p.add_argument('--kernel-repeats',type=int,default=1)
    p.add_argument('--compile',action='store_true')
    p.add_argument('--live-gc',action='store_true',help='Retain an initialized struct local throughout the SIMD loop.')
    p.add_argument('--wasm-tools',type=Path)
    a=p.parse_args()
    if not 1<=a.iterations<=1000000 or not 1<=a.kernel_repeats<=100000:p.error('bounded iterations/repetitions required')
    if a.compile:
        if not a.wasm_tools:p.error('--compile requires --wasm-tools')
        subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    a.out.mkdir(parents=True,exist_ok=False);wat=a.out/'simd_integer_mix.wat';wasm=wat.with_suffix('.wasm')
    epilogue=EPILOGUE.replace('i32.const 96 i32.const 1 call $text)\n','i32.const 96 i32.const 1 call $text\n  call $profile_end)\n')
    declaration='(type $anchor (struct (field i32)))' if a.live_gc else ''
    local='(local $anchor (ref $anchor))' if a.live_gc else ''
    initialize='i32.const 7 struct.new $anchor local.set $anchor' if a.live_gc else ''
    finish='local.get $anchor struct.get $anchor 0 i64.extend_i32_u i64.add' if a.live_gc else ''
    wat.write_text(f'''(module {declaration} {PRELUDE}
      (global $iterations i32 (i32.const {a.iterations}))
      (global $kernel_repeats i32 (i32.const {a.kernel_repeats}))
      (func $kernel (export "kernel") (param $n i32) (result i64)
       (local $i i32) (local $vector v128) {local}
       {initialize}
       v128.const i32x4 1 2 3 4 local.set $vector
       loop $again
        local.get $vector v128.const i32x4 1664525 1664525 1664525 1664525 i32x4.mul
        v128.const i32x4 1013904223 1013904223 1013904223 1013904223 i32x4.add
        local.get $vector i32.const 13 i32x4.shr_u v128.xor local.set $vector
        local.get $i i32.const 1 i32.add local.tee $i local.get $n i32.lt_u br_if $again
       end
       local.get $vector i32x4.extract_lane 0 i64.extend_i32_u
       local.get $vector i32x4.extract_lane 1 i64.extend_i32_u i64.add
       local.get $vector i32x4.extract_lane 2 i64.extend_i32_u i64.add
       local.get $vector i32x4.extract_lane 3 i64.extend_i32_u i64.add {finish})
      {epilogue}
      (func $profile_end (export "profile_end") nop))
    ''')
    row={'name':'simd_integer_mix','feature':'Vector arithmetic','required_features':['simd',*(['gc','reference-types','function-references'] if a.live_gc else [])],'opcode_witness':'i32x4.mul','iterations':a.iterations,'kernel_repeats':a.kernel_repeats,'expected_checksum':(checksum(a.iterations)+(7 if a.live_gc else 0))*a.kernel_repeats,'oracle':'four independent scalar uint32 LCG/XOR recurrences; sum final lanes, optional struct field 7, and repetitions','live_gc_local':a.live_gc,'validation':'pending','wat_sha256':hashlib.sha256(wat.read_bytes()).hexdigest(),'notes':'No memory, calls or allocation inside the measured loop. Optional struct allocation/read occur once per kernel outside the SIMD loop. Same bytes in all engines. profile_end follows clock/result output.'}
    if a.compile:
        row['tool_runs']=[]
        for kind,argv in [('parse',[str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)]),('validate',[str(a.wasm_tools),'validate',str(wasm)]),('print',[str(a.wasm_tools),'print',str(wasm)])]:
            q=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60);(a.out/(kind+'.log')).write_bytes(q.stdout);row['tool_runs'].append({'argv':argv,'exit':q.returncode});assert q.returncode==0
        assert b'i32x4.mul' in q.stdout and b'profile_end' in q.stdout
        row['validation']='passed';row['wasm_sha256']=hashlib.sha256(wasm.read_bytes()).hexdigest()
    (a.out/'manifest.json').write_text(json.dumps([row],indent=2));print(json.dumps(row),flush=True)

if __name__=='__main__':main()
