#!/usr/bin/env python3
"""Run the pinned current Wasmtime multi-memory WAST, including imported memories.

wast2json --enable-multi-memory converts the ORIGINAL named-memory text syntax. Wrappers
append assertion code without rewriting its instructions or memory definitions.
The pinned import group writes both providers before every checked load; fresh
provider instances therefore reproduce the WAST's state at those assertions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from run_wasm3_multi_memory import configurations

SOURCE_SHA256='621d637c894bae3814a522e33923e88012262d93b818db85a75c3118f727fde0'
def leb(n):
    out=bytearray()
    while n>=128:out.append(n&127|128);n>>=7
    out.append(n);return bytes(out)
def signed_leb(n):
    out=bytearray()
    while True:
        b=n&127;n>>=7;more=not((n==0 and not(b&64))or(n==-1 and b&64));out.append(b|(128 if more else 0))
        if not more:return bytes(out)
def read_leb(data,p):
    value=shift=0
    while True:
        b=data[p];p+=1;value|=(b&127)<<shift
        if b<128:return value,p
        shift+=7
        if shift>=35:raise ValueError('invalid u32')
def constant(value):
    bits={'i32':32,'i64':64}[value['type']];n=int(value['value'])
    if n>=1<<(bits-1):n-=1<<bits
    return bytes([0x41 if bits==32 else 0x42])+signed_leb(n)
def wrap(data,commands):
    assert data[:8]==b'\0asm\1\0\0\0';sections={};p=8
    while p<len(data):
        ident=data[p];size,start=read_leb(data,p+1);p=start+size
        if ident:sections[ident]=data[start:p]
    assert 8 not in sections
    count,p=read_leb(sections[1],0);sections[1]=leb(count+1)+sections[1][p:]+b'\x60\x00\x00';new_type=count
    count,p=read_leb(sections[3],0);sections[3]=leb(count+1)+sections[3][p:]+leb(new_type);new_func=count
    # The pinned source imports memories only, so defined and global function indices agree.
    count,p=read_leb(sections[7],0);mapping={}
    for _ in range(count):
        size,p=read_leb(sections[7],p);name=sections[7][p:p+size].decode();p+=size;kind=sections[7][p];index,p=read_leb(sections[7],p+1)
        if kind==0:mapping[name]=index
    _,p=read_leb(sections[7],0);sections[7]=leb(count+1)+sections[7][p:]+b'\x06_start\x00'+leb(new_func)
    body=b'\x00'
    for command in commands:
        action=command['action'];assert action['type']=='invoke'
        body+=b''.join(constant(v) for v in action['args'])+b'\x10'+leb(mapping[action['field']])
        expected=command['expected']
        if command['type']=='assert_return':
            assert len(expected)==1;value=expected[0]
            body+=constant(value)+bytes([0x47 if value['type']=='i32' else 0x52])+b'\x04\x40\x00\x0b'
        else:assert command['type']=='action' and not expected
    body+=b'\x0b';count,p=read_leb(sections[10],0);assert count==new_func
    sections[10]=leb(count+1)+sections[10][p:]+leb(len(body))+body
    order=(1,2,3,4,5,13,6,7,8,9,12,10,11)
    return data[:8]+b''.join(bytes([i])+leb(len(sections[i]))+sections[i] for i in order if i in sections)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('spec',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--ros',action='store_true');p.add_argument('--configuration');p.add_argument('--linking',type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    assert hashlib.sha256((a.spec/'simple.wast').read_bytes()).hexdigest()==SOURCE_SHA256
    groups=[];named={}
    for cmd in json.loads((a.spec/'simple.json').read_text())['commands']:
        if cmd['type']=='module':
            groups.append((cmd,[]))
            if 'name' in cmd:named[cmd['name']]=groups[-1]
        else:
            assert cmd['type'] in ('action','assert_return')
            target=named[cmd['action']['module']] if 'module' in cmd['action'] else groups[-1]
            target[1].append(cmd)
    fixtures=[]
    for index,(module,commands) in enumerate(groups):
        path=a.output/f'official-{index}.wasm';path.write_bytes(wrap((a.spec/module['filename']).read_bytes(),commands))
        fixtures.append((index,path,sum(cmd['type']=='assert_return' for cmd in commands)))
    configs=configurations(a.uwvm,a.ros) if a.uwvm else []
    if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime),'-C','cache=n','-W','multi-memory=y']))
    if a.configuration:configs=[c for c in configs if c[0]==a.configuration]
    rows=[];linking_rows=[]
    for name,base in configs:
        for index,fixture,count in fixtures:
            command=base[:]
            if index==3:
                preload=[]
                for provider,file in (('a','simple.1.wasm'),('b','simple.2.wasm')):
                    preload+=(['--preload',provider+'='+str(a.spec/file)] if name=='wasmtime' else ['-Wpre',str(a.spec/file),provider])
                command=command+preload if name=='wasmtime' else command[:-1]+preload+command[-1:]
            run=subprocess.run(command+[str(fixture)],capture_output=True,timeout=60);out=run.stdout+run.stderr
            (a.output/f'{name}-{index}.log').write_bytes(out)
            if run.returncode:raise RuntimeError(f'{name} module {index}: {run.returncode}: {out.decode(errors="replace")}')
            rows.append(dict(configuration=name,module=index,assertions=count,passed=True))
        if a.linking:
            command=base[:];preload=[]
            for provider,file in (('a','simple.1.wasm'),('b','simple.2.wasm')):
                preload+=(['--preload',provider+'='+str(a.spec/file)] if name=='wasmtime' else ['-Wpre',str(a.spec/file),provider])
            command=command+preload if name=='wasmtime' else command[:-1]+preload+command[-1:]
            run=subprocess.run(command+[str(a.linking)],capture_output=True,timeout=60);out=run.stdout+run.stderr
            (a.output/f'{name}-linking.log').write_bytes(out)
            if run.returncode:raise RuntimeError(f'{name} alias/linking: {run.returncode}: {out.decode(errors="replace")}')
            linking_rows.append(dict(configuration=name,fixture=str(a.linking),passed=True))
        print(f'PASS {name}: 7 official modules, 17 assertions, named and imported memory syntax',flush=True)
    summary=dict(source_sha256=SOURCE_SHA256,official_modules=len(fixtures),official_assertions=sum(f[2] for f in fixtures),configurations=len(configs),runs=len(rows),linking_cases=linking_rows,cases=rows)
    (a.output/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
if __name__=='__main__':main()
