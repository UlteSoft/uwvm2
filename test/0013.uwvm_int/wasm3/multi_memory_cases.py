#!/usr/bin/env python3
"""Core 3 multi-memory fixtures with independently computed scalar expectations."""
import argparse
import json
from pathlib import Path

def leb(n):
    result=bytearray()
    while n >= 128:
        result.append((n & 127) | 128); n >>= 7
    result.append(n); return bytes(result)
def sleb(n):
    result=bytearray()
    while True:
        part=n&127; n >>= 7
        more=not ((n==0 and not(part&64)) or (n==-1 and part&64))
        result.append(part | (128 if more else 0))
        if not more:return bytes(result)
def sec(i,b):return bytes([i])+leb(len(b))+b
def const(x):return b'\x41'+sleb(x)
def arg(align, index, offset=0):return leb(align|64)+leb(index)+leb(offset)
def load(index,offset=0,opcode=0x28,align=2):return bytes([opcode])+arg(align,index,offset)
def store(index,offset=0,opcode=0x36,align=2):return bytes([opcode])+arg(align,index,offset)
def simd(code):return b'\xfd'+leb(code)
def module(code, memories=2, expected=None):
    body=b'\x01\x02\x7f'+code+b'\x0b'
    mems=b''.join(b'\x01'+leb(2 if i==1 else 1)+b'\x04' for i in range(memories))
    # Active data uses the explicit memory-index form, alongside passive data for memory.init.
    data=b'\x03'+b'\x02\x00'+const(0)+b'\x0b\x04\x11\x22\x33\x44'+b'\x02\x01'+const(0)+b'\x0b\x04\xaa\xbb\xcc\xdd'+b'\x01\x04ABCD'
    types=b'\x01\x60\x00\x01\x7f'; funcs=b'\x01\x00'; exports=b'\x01\x03run\x00\x00'; bodies=b'\x01'+leb(len(body))+body
    if expected is not None:
        types=b'\x02\x60\x00\x01\x7f\x60\x00\x00'; funcs=b'\x02\x00\x01'; exports=b'\x02\x03run\x00\x00\x06_start\x00\x01'
        signed=expected if expected < 0x80000000 else expected-0x100000000
        wrapper=b'\x00\x10\x00'+const(signed)+b'\x47\x04\x40\x00\x0b\x0b'
        bodies=b'\x02'+leb(len(body))+body+leb(len(wrapper))+wrapper
    return b'\x00asm\x01\x00\x00\x00'+sec(1,types)+sec(3,funcs)+sec(5,leb(memories)+mems)+sec(7,exports)+sec(12,b'\x03')+sec(10,bodies)+sec(11,data)
def cases():
    rows=[]
    def add(name,code,expected=0,valid=True,disabled_valid=False,memories=2):
        rows.append(dict(name=name,code=code,wasm=module(code,memories),expected=expected&0xffffffff,valid=valid,disabled_valid=disabled_valid,memories=memories))
    add('load-memory0',const(0)+load(0),0x44332211)
    add('load-memory1',const(0)+load(1),0xddccbbaa)
    add('size-memory1',b'\x3f\x01',2)
    add('grow-memory1',const(1)+b'\x40\x01\x1a\x3f\x01',3)
    add('grow-keeps-memory0',const(1)+b'\x40\x01\x1a\x3f\x00',1)
    add('grow-fails-at-selected-limit',const(3)+b'\x40\x01',-1)
    add('store-does-not-touch-memory0',const(0)+const(123)+store(1)+const(0)+load(0),0x44332211)
    add('alternate-memory-selection',const(8)+const(123)+store(1)+const(8)+const(456)+store(0)+const(8)+load(1)+const(8)+load(0)+b'\x6a',579)
    for op,align,expected in [(0x2c,0,-86),(0x2d,0,170),(0x2e,1,-17494),(0x2f,1,0xbbaa)]:add(f'load-{op:x}',const(0)+load(1,opcode=op,align=align),expected)
    for op,align,conversion in [(0x29,3,b'\xa7'),(0x2a,2,b'\xbc'),(0x2b,3,b'\xbd\xa7'),(0x30,0,b'\xa7'),(0x31,0,b'\xa7'),(0x32,1,b'\xa7'),(0x33,1,b'\xa7'),(0x34,2,b'\xa7'),(0x35,2,b'\xa7')]:
        value={0x30:-86,0x31:170,0x32:-17494,0x33:0xbbaa}.get(op,0xddccbbaa)
        add(f'load-{op:x}',const(0)+load(1,opcode=op,align=align)+conversion,value)
    for op,align,value,expect in [(0x36,2,const(123),123),(0x3a,0,const(0x1234),0x34),(0x3b,1,const(0x123456),0x3456),(0x37,3,b'\x42'+sleb(0x123456),0x123456),(0x38,2,b'\x43\x12\x34\x56\x78',0x78563412),(0x39,3,b'\x44\x12\x34\x56\x78\x00\x00\x00\x00',0x78563412),(0x3c,0,b'\x42'+sleb(0x1234),0x34),(0x3d,1,b'\x42'+sleb(0x123456),0x3456),(0x3e,2,b'\x42'+sleb(0x123456),0x123456)]:
        add(f'store-{op:x}',const(8)+value+store(1,opcode=op,align=align)+const(8)+load(1),expect)
    add('copy-memory1-to-0',const(8)+const(0)+const(4)+b'\xfc\x0a\x00\x01'+const(8)+load(0),0xddccbbaa)
    add('copy-memory0-to-1',const(8)+const(0)+const(4)+b'\xfc\x0a\x01\x00'+const(8)+load(1),0x44332211)
    add('copy-overlap',const(1)+const(0)+const(3)+b'\xfc\x0a\x01\x01'+const(0)+load(1),0xccbbaaaa)
    add('copy-zero-at-end',const(65536)+const(131072)+const(0)+b'\xfc\x0a\x00\x01'+const(9),9)
    add('fill-memory1',const(8)+const(0x55)+const(4)+b'\xfc\x0b\x01'+const(8)+load(1),0x55555555)
    add('init-memory1',const(8)+const(0)+const(4)+b'\xfc\x08\x02\x01'+const(8)+load(1),0x44434241)
    add('simd-load',const(0)+simd(0)+arg(4,1)+simd(27)+b'\x00',0xddccbbaa)
    add('simd-load32-zero',const(0)+simd(92)+arg(2,1)+simd(27)+b'\x00',0xddccbbaa)
    add('simd-load8-splat',const(0)+simd(7)+arg(0,1)+simd(27)+b'\x00',0xaaaaaaaa)
    add('simd-store',const(8)+simd(12)+bytes(range(16))+simd(11)+arg(4,1)+const(8)+load(1),0x03020100)
    add('simd-load-lane',const(0)+simd(12)+bytes(16)+simd(86)+arg(2,1)+b'\x01'+simd(27)+b'\x01',0xddccbbaa)
    add('simd-store-lane',const(8)+simd(12)+bytes(range(16))+simd(90)+arg(2,1)+b'\x01'+const(8)+load(1),0x07060504)
    add('u16-copy-candidate-different-memory',const(8)+b'\x21\x00'+const(0)+b'\x21\x01\x20\x00\x20\x01'+const(1)+b'\x74'+load(1,opcode=0x2f,align=1)+store(0,opcode=0x3b,align=1)+const(8)+load(0),0xbbaa)
    add('index-129-memarg',const(8)+const(37)+store(129)+const(8)+load(129),37,memories=130)
    add('index-129-size',b'\x3f'+leb(129),1,memories=130)
    add('padded-index-size',b'\x3f\x81\x00',2)
    add('padded-index-memarg',const(0)+b'\x28\x42\x81\x00\x00',0xddccbbaa)
    add('legacy-implicit-memory0',const(0)+b'\x28\x02\x00',0x44332211,disabled_valid=True)
    for name,code in [('index-out-of-range',const(0)+load(2)),('size-out-of-range',b'\x3f\x02'),('grow-out-of-range',const(0)+b'\x40\x02'),('index-overflow',const(0)+b'\x28\x42\xff\xff\xff\xff\x1f\x00'),('flag-bit7',const(0)+b'\x28\x82\x01\x00'),('alignment-too-large',const(0)+load(1,align=3)),('offset-too-large',const(0)+b'\x28\x42\x01'+leb(0x100000000)),('fill-out-of-range',const(0)*3+b'\xfc\x0b\x02'),('copy-source-out-of-range',const(0)*3+b'\xfc\x0a\x00\x02'),('copy-destination-out-of-range',const(0)*3+b'\xfc\x0a\x02\x00'),('init-out-of-range',const(0)*3+b'\xfc\x08\x02\x02'),('simd-out-of-range',const(0)+simd(0)+arg(4,2)+simd(27)+b'\x00')]:
        add(name,code,valid=False)
    return rows

def main():
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    rows=cases();lines=['#pragma once','#include <cstddef>','struct multi_memory_case {char const* name; unsigned char const* data; std::size_t size; unsigned expected; bool valid, disabled_valid;};']
    for i,row in enumerate(rows):
        lines.append(f'inline constexpr unsigned char mm_{i}[]{{'+','.join(map(str,row['wasm']))+'};')
        (a.output/(row['name']+'.wasm')).write_bytes(row['wasm'])
        (a.output/(row['name']+'-exec.wasm')).write_bytes(module(row['code'],row['memories'],row['expected']))
    lines.append('inline constexpr multi_memory_case multi_memory_cases[]{')
    for i,row in enumerate(rows):lines.append('{"'+row['name']+f'",mm_{i},sizeof(mm_{i}),'+str(row['expected'])+','+str(row['valid']).lower()+','+str(row['disabled_valid']).lower()+'},')
    lines.append('};');(a.output/'multi_memory_vectors.h').write_text('\n'.join(lines)+'\n')
    (a.output/'multi-memory-cases.json').write_text(json.dumps([{k:v for k,v in row.items() if k not in ('code','wasm')} for row in rows],indent=2)+'\n')
if __name__=='__main__':main()
