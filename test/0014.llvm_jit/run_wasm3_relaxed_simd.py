#!/usr/bin/env python3
"""Run all assertions in the pinned official relaxed-SIMD WAST files in actual backends.

wast2json must first convert each file with --enable-all. The wrapper preserves the
original module and exported functions, adds only a _start assertion function, and
accepts the complete `either` result set and canonical/arithmetic NaN patterns.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

HASHES = {
'i16x8_relaxed_q15mulr_s':'fa3885a95e4818e8aa9782dc8d9fb76a94b511114730ffc0b31d471ddab5b068',
'i32x4_relaxed_trunc':'8f93d17c35f0a4076df7c32219f029ec4fc18a17562f783618d522c67f282126',
'i8x16_relaxed_swizzle':'df195a731037cb57f51ecd06be6216be3b00490672656cf9ae350669a6cb6e30',
'relaxed_dot_product':'5de2c0cd58e7303f0e568d7484574385b5a697e3fbd06e71017840babe7ec66d',
'relaxed_laneselect':'8bb3de6928e82faacdfe33f965fa02cffb85b9a1b91f2909f16dcdd84c8d9e58',
'relaxed_madd_nmadd':'e59780f08472752eff7d33bc9f803283bb4b81d61e7b42198ce462e2c8b3b5e4',
'relaxed_min_max':'388bfaded12558cc7b5c4e7e4636d6020fbe3c67d3c0affc9ffbe3656f5f513c'}

def leb(n):
    out=bytearray()
    while n>=128:out.append((n&127)|128);n>>=7
    return bytes(out+bytes([n]))
def read_leb(data,p):
    value=shift=0
    while True:
        byte=data[p];p+=1;value|=(byte&127)<<shift
        if byte<128:return value,p
        shift+=7
        if shift>=35:raise ValueError('invalid u32')
def simd(n):return b'\xfd'+leb(n)
def vec(v):return simd(12)+v

def pattern(value):
    assert value['type']=='v128'
    kind=value['lane_type'];bits=int(kind[1:]);size=bits//8;data=bytearray();mask=bytearray()
    for lane in value['value']:
        full=(1<<bits)-1
        if lane.startswith('nan:'):
            assert kind in ('f32','f64')
            canonical=0x7fc00000 if bits==32 else 0x7ff8000000000000
            n=canonical;m=canonical if lane=='nan:arithmetic' else full>>1
        else:n=int(lane);m=full
        data.extend((n&full).to_bytes(size,'little'));mask.extend(m.to_bytes(size,'little'))
    assert len(data)==len(mask)==16
    return bytes(data),bytes(mask)

def wrap(data,commands):
    assert data[:8]==b'\0asm\1\0\0\0'
    sections={};p=8
    while p<len(data):
        ident=data[p];length,start=read_leb(data,p+1);p=start+length
        if ident:sections[ident]=data[start:p]
    assert not any(x in sections for x in (2,8,9,11,12,13)), 'unexpected imports, start, or segments in pinned modules'
    types,p=read_leb(sections[1],0);sections[1]=leb(types+1)+sections[1][p:]+b'\x60\x00\x00'
    functions,p=read_leb(sections[3],0);sections[3]=leb(functions+1)+sections[3][p:]+leb(types)
    exports,p=read_leb(sections[7],0);mapping={}
    for _ in range(exports):
        length,p=read_leb(sections[7],p);name=sections[7][p:p+length].decode();p+=length
        kind=sections[7][p];index,p=read_leb(sections[7],p+1)
        if kind==0:mapping[name]=index
    assert '_start' not in mapping
    _,p=read_leb(sections[7],0);sections[7]=leb(exports+1)+sections[7][p:]+b'\x06_start\x00'+leb(functions)
    body=b'\x01\x01\x7b' # one v128 temporary; invocation occurs once, before candidate comparisons
    for cmd in commands:
        assert cmd['type']=='assert_return' and cmd['action']['type']=='invoke'
        for arg in cmd['action']['args']:
            value,mask=pattern(arg);assert mask==b'\xff'*16;body+=vec(value)
        body+=b'\x10'+leb(mapping[cmd['action']['field']])+b'\x21\x00\x41\x00'
        candidates=cmd.get('either',cmd.get('expected'))
        if 'either' not in cmd:assert len(candidates)==1
        for candidate in candidates:
            value,mask=pattern(candidate)
            body+=b'\x20\x00'+vec(value)+simd(0x51)+vec(mask)+simd(0x4e)+simd(0x53)+b'\x45\x72'
        body+=b'\x45\x04\x40\x00\x0b'
    body+=b'\x0b';count,p=read_leb(sections[10],0);assert count==functions
    sections[10]=leb(count+1)+sections[10][p:]+leb(len(body))+body
    return data[:8]+b''.join(bytes([i])+leb(len(s))+s for i,s in sorted(sections.items()))

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('spec',type=Path);ap.add_argument('output',type=Path)
    ap.add_argument('--uwvm',type=Path);ap.add_argument('--ros',action='store_true');ap.add_argument('--wasmtime',type=Path)
    ap.add_argument('--focused',type=Path,help='UWVM fixed-projection fixture (not a cross-engine equality oracle)')
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    fixtures=[]
    for name,digest in HASHES.items():
        source=args.spec/(name+'.wast');assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
        commands=json.loads((args.spec/(name+'.json')).read_text())['commands'];groups=[]
        for cmd in commands:
            if cmd['type']=='module':groups.append([cmd,[]])
            else:assert cmd['type']=='assert_return';groups[-1][1].append(cmd)
        for index,(module,assertions) in enumerate(groups):
            path=args.output/f'{name}-{index}.wasm';path.write_bytes(wrap((args.spec/module['filename']).read_bytes(),assertions));fixtures.append((path,len(assertions)))
    configurations=[]
    if args.wasmtime:configurations.append(('wasmtime', [str(args.wasmtime),'-C','cache=n','-W','relaxed-simd=y']))
    if args.uwvm:
        for backend in ('int','jit','tiered'):
            if args.ros and backend=='tiered':continue
            modes=('full',) if args.ros else (('lazy','lazy+verification') if backend=='tiered' else ('full','lazy','lazy+verification'))
            for mode in modes:
                policies=('instruction','unwind') if backend=='jit' else ('instruction',)
                variants=((),('-Rtiered-disable-t0',),('-Rtiered-disable-t2',),('-Rtiered-disable-t0','-Rtiered-disable-t2')) if backend=='tiered' else ((),)
                for policy in policies:
                    for variant in variants:
                        label=f'{backend}-{mode}-{policy}'+''.join(variant)
                        cmd=[str(args.uwvm)]+(['-Raot' if backend=='jit' else '-Rint'] if args.ros else ['-Rcc',backend,'-Rcm',mode])
                        cmd+=list(variant)
                        if backend!='int':cmd+=['-Rllvm-cache-path','disable','-Rllvm-call-stack',policy]
                        configurations.append((label,cmd+['-WFE-relaxed-simd','--run']))
    rows=[]
    for label,cmd in configurations:
        selected=fixtures+([(args.focused,25)] if args.focused and label!='wasmtime' else [])
        for fixture,count in selected:
            run=subprocess.run(cmd+[str(fixture)],capture_output=True,timeout=60)
            log=args.output/(label+'-'+fixture.stem+'.log');log.write_bytes(run.stdout+run.stderr)
            rows.append(dict(configuration=label,fixture=fixture.name,assertions=count,exit=run.returncode))
            if run.returncode:raise RuntimeError(f'{label}: {fixture}: {run.returncode}: {log.read_text()}')
    if args.uwvm:
        for flags in ([],['-WFD-relaxed-simd'],['-WFE-relaxed-simd','-WFD-simd']):
            cmd=[str(args.uwvm),'-m','validation',*flags,'--run',str(fixtures[0][0])]
            run=subprocess.run(cmd,capture_output=True,timeout=60);out=run.stdout+run.stderr
            expected = b'Illegal Value Type' if '-WFD-simd' in flags else b'--wasm-feature-enable-relaxed-simd'
            if run.returncode==0 or expected not in out:raise RuntimeError(f'feature rejection failed: {flags}: {out!r}')
            (args.output/('gate-'+str(len(flags))+'.log')).write_bytes(out)
        run=subprocess.run([str(args.uwvm),'-WFE-relaxed-simd','-WFD-relaxed-simd','--run',str(fixtures[0][0])],capture_output=True)
        if run.returncode==0 or b'conflicts' not in run.stdout+run.stderr:raise RuntimeError('feature ownership conflict accepted')
    summary=dict(official_assertions=sum(n for _,n in fixtures),official_modules=len(fixtures),configurations=len(configurations),runs=len(rows),cases=rows)
    (args.output/'results.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k!='cases'}))

if __name__=='__main__':main()
