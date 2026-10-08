#!/usr/bin/env python3
"""Generate independent bit-pattern expectations and executable Core 3 SIMD fixtures.

Uses the fixed projections documented in validation/standard/wasm3/relaxed_simd.h.
No generated expected value calls a UWVM semantic helper.
"""
import argparse
import json
from pathlib import Path
import struct

NAMES = ['i8x16.relaxed_swizzle', 'i32x4.relaxed_trunc_f32x4_s', 'i32x4.relaxed_trunc_f32x4_u',
         'i32x4.relaxed_trunc_f64x2_s_zero', 'i32x4.relaxed_trunc_f64x2_u_zero',
         'f32x4.relaxed_madd', 'f32x4.relaxed_nmadd', 'f64x2.relaxed_madd', 'f64x2.relaxed_nmadd',
         'i8x16.relaxed_laneselect', 'i16x8.relaxed_laneselect', 'i32x4.relaxed_laneselect', 'i64x2.relaxed_laneselect',
         'f32x4.relaxed_min', 'f32x4.relaxed_max', 'f64x2.relaxed_min', 'f64x2.relaxed_max',
         'i16x8.relaxed_q15mulr_s', 'i16x8.relaxed_dot_i8x16_i7x16_s', 'i32x4.relaxed_dot_i8x16_i7x16_add_s']

def lanes(fmt, *values): return struct.pack('<' + fmt * len(values), *values)
def u32(*v): return lanes('I', *(x & 0xffffffff for x in v))
def u16(*v): return lanes('H', *(x & 65535 for x in v))
def f32(*v): return lanes('f', *v)
def f64(*v): return lanes('d', *v)
def leb(n):
    out = bytearray()
    while n >= 128:
        out.append((n & 127) | 128)
        n >>= 7
    return bytes(out + bytes([n]))
def vec(v): return b'\xfd\x0c' + v
def simd(n): return b'\xfd' + leb(n)
def section(n, data): return bytes([n]) + leb(len(data)) + data

def cases():
    rows = []
    def add(i, args, expected, mask=b'\xff' * 16, suffix=''):
        assert all(len(x) == 16 for x in [*args, expected, mask])
        rows.append(dict(op=256+i, args=args, expected=expected, mask=mask, name=NAMES[i]+suffix))
    add(0, [bytes(range(16)), bytes([0,15,16,31,127,128,255,1,2,3,4,5,6,7,8,9])], bytes([0,15,0,0,0,0,0,1,2,3,4,5,6,7,8,9]))
    add(1, [f32(1.9,-1.9,float('inf'),float('nan'))], u32(1,-1,2147483647,0))
    add(2, [f32(-1.9,1.9,float('inf'),float('nan'))], u32(0,1,-1,0))
    add(3, [f64(-1.9,float('inf'))], u32(-1,2147483647,0,0))
    add(4, [f64(4294967295.0,float('nan'))], u32(-1,0,0,0))
    add(5, [f32(1.5,-2,0,-0.0),f32(2,3,-0.0,1),f32(.5,7,0,-0.0)], f32(3.5,1,0,-0.0))
    add(6, [f32(1.5,-2,0,-0.0),f32(2,3,1,1),f32(.5,7,0,-0.0)], f32(-2.5,13,0,0))
    add(7, [f64(1.5,-2),f64(2,3),f64(.5,7)], f64(3.5,1))
    add(8, [f64(1.5,-2),f64(2,3),f64(.5,7)], f64(-2.5,13))
    for i in range(9,13):
        a=bytes([0xaa,0x55,0xff,0]*4); b=bytes([0x55,0xaa,0,0xff]*4); m=bytes([0,0xff,0x5a,0x81]*4)
        add(i,[a,b,m],bytes((x&z)|(y&(~z&255)) for x,y,z in zip(a,b,m)))
    add(13,[f32(-0.0,0,2,-3),f32(0,-0.0,3,-4)],f32(-0.0,-0.0,2,-4))
    add(14,[f32(-0.0,0,2,-3),f32(0,-0.0,3,-4)],f32(0,0,3,-3))
    add(15,[f64(-0.0,2),f64(0,3)],f64(-0.0,2))
    add(16,[f64(-0.0,2),f64(0,3)],f64(0,3))
    add(17,[u16(-32768,16384,-16384,0,32767,1,-1,-32768),u16(-32768,16384,16384,32767,32767,32767,32767,1)],u16(32767,8192,-8192,0,32766,1,-1,-1))
    a=bytes([0x80,0x80,1,2,0xff,2,0x7f,0x80]*2);b=bytes([0x80,0x80,3,4,2,3,0x7f,0x7f]*2)
    add(18,[a,b],u16(32767,11,4,-127,32767,11,4,-127))
    add(19,[a,b,u32(2147483647,-2147483648,0xffffffff,123)],u32(2147483647+32778,-2147483648-123,32777,0))
    add(1,[f32(-float('inf'),-2147483648.0,2147483648.0,-0.0)],u32(-2147483648,-2147483648,2147483647,0),suffix='-limits')
    add(3,[f64(float('nan'),-float('inf'))],u32(0,-2147483648,0,0),suffix='-nan')
    # Multiply rounds to exactly one before subtracting one. Fused evaluation would be nonzero.
    add(5,[u32(*([0x3f800001]*4)),u32(*([0x3f7ffffe]*4)),f32(*([-1.0]*4))],bytes(16),suffix='-unfused')
    add(7,[lanes('Q',0x3ff0000000000001,0x3ff0000000000001),lanes('Q',0x3feffffffffffffe,0x3feffffffffffffe),f64(-1,-1)],bytes(16),suffix='-unfused')
    add(13,[u32(0x7f800001,0x7fc12345,0x3f800000,0xff800000),f32(1,2,float('nan'),0)],u32(0x7fc00000,0x7fc00000,0x7fc00000,0xff800000),u32(0x7fc00000,0x7fc00000,0x7fc00000,0xffffffff),suffix='-nan')
    return rows

def module(rows):
    types = [b'\x60'+leb(n)+b'\x7b'*n+b'\x01\x7b' for n in (1,2,3)] + [b'\x60\x00\x00']
    funcs=[];exports=[];indices=[]
    for idx,row in enumerate(rows):
        body=b'\x00'+b''.join(b'\x20'+leb(i) for i in range(len(row['args'])))+simd(row['op'])+b'\x0b'
        funcs.append(leb(len(body))+body);indices.append(len(row['args'])-1)
        name=row['name'].encode();exports.append(leb(len(name))+name+b'\x00'+leb(idx))
    body=b'\x00'
    for idx,row in enumerate(rows):
        body+=b''.join(vec(v) for v in row['args'])+b'\x10'+leb(idx)+vec(row['expected'])+simd(0x51)+vec(row['mask'])+simd(0x4e)+simd(0x53)+b'\x04\x40\x00\x0b'
    body+=b'\x0b';funcs.append(leb(len(body))+body);indices.append(3)
    exports.append(b'\x06_start\x00'+leb(len(rows)))
    return b'\x00asm\x01\x00\x00\x00'+section(1,leb(len(types))+b''.join(types))+section(3,leb(len(indices))+b''.join(leb(i) for i in indices))+section(7,leb(len(exports))+b''.join(exports))+section(10,leb(len(funcs))+b''.join(funcs))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path);args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    rows=cases();(args.output/'relaxed-simd.wasm').write_bytes(module(rows))
    def array(v):return '{'+','.join(str(x) for x in v)+'}'
    lines=['#pragma once','struct relaxed_case { unsigned opcode, arity; unsigned char args[3][16], expected[16], mask[16]; };','inline constexpr relaxed_case relaxed_cases[]{']
    for row in rows:
        operands=row['args']+[bytes(16)]*(3-len(row['args']))
        lines.append('{'+f"{row['op']},{len(row['args'])},"+ '{'+','.join(array(v) for v in operands)+'},'+array(row['expected'])+','+array(row['mask'])+'},')
    lines.append('};');(args.output/'relaxed_simd_vectors.h').write_text('\n'.join(lines)+'\n')
    (args.output/'relaxed-simd-cases.json').write_text(json.dumps([{'name':r['name'],'opcode':r['op'],'arity':len(r['args'])} for r in rows],indent=2)+'\n')

if __name__=='__main__':main()
