from pathlib import Path
import ctypes
class Vec(ctypes.Structure):
 _fields_=[('size',ctypes.c_size_t),('data',ctypes.POINTER(ctypes.c_ubyte))]
L=ctypes.CDLL('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so')
L.wasmtime_wat2wasm.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.POINTER(Vec)];L.wasmtime_wat2wasm.restype=ctypes.c_void_p
L.wasm_byte_vec_delete.argtypes=[ctypes.POINTER(Vec)]
L.wasmtime_error_message.argtypes=[ctypes.c_void_p,ctypes.POINTER(Vec)];L.wasmtime_error_delete.argtypes=[ctypes.c_void_p]
def wat_to_wasm(wat,wasm):
 w=Path(wat).read_bytes();v=Vec();err=L.wasmtime_wat2wasm(w,len(w),ctypes.byref(v))
 if err:
  msg=Vec();L.wasmtime_error_message(err,ctypes.byref(msg));text=bytes(msg.data[:msg.size]).decode();L.wasm_byte_vec_delete(ctypes.byref(msg));L.wasmtime_error_delete(err);raise AssertionError(text)
 Path(wasm).write_bytes(bytes(v.data[:v.size]));L.wasm_byte_vec_delete(ctypes.byref(v))
def leb(b,a):
 n=0
 for shift in range(0,70,7):
  v=b[a];a+=1;n|=(v&127)<<shift
  if v<128:return n,a
 raise AssertionError('fixture LEB bound')
def nop_markers(path):
 b=Path(path).read_bytes();assert b[:8]==b'\0asm\x01\0\0\0';a=8;result={}
 while a<len(b):
  tag=b[a];n,a=leb(b,a+1);end=a+n;assert end<=len(b)
  if tag==10:
   count,a=leb(b,a)
   for i in range(count):
    size,a=leb(b,a);finish=a+size;groups,a=leb(b,a)
    for _ in range(groups):
     _,a=leb(b,a);kind=b[a];a+=1
     if kind in (0x63,0x64):_,a=leb(b,a)
     else:assert kind in (0x7f,0x7e,0x7d,0x7c,0x7b)
    first=a;found=[]
    while a<finish:
     at=a;op=b[a];a+=1
     if op in (0x20,0x21,0x22,0xd2,0x14,0x10,0x41):_,a=leb(b,a)
     elif op==0x04:assert b[a]==0x40;a+=1
     else:assert op in (0x00,0x01,0x0b,0x47,0x6a),(op,at)
     if op==0x01:found.append(at-first)
    assert a==finish;result[i+1]=found
   assert a==end
  a=end
 assert set(result)=={1,2} and len(result[2])==1,result
 return result
