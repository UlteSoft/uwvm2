#!/usr/bin/env python3
"""Actual debug-jit CLI: interactive instruction/EH control and guest input isolation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--uwvm',type=Path,required=True)
p.add_argument('--wasm-tools',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--ros',action='store_true')
p.add_argument('--source-root',type=Path)
p.add_argument('--source-fixture-dir',type=Path,help='bounded Linux C/C++ DWARF4/5 Wasm outputs')
p.add_argument('--rust-source-fixture',type=Path,help='4 GiB bounded macOS Rust -g Wasm output')
p.add_argument('--source-only',action='store_true',help='run real-product C/C++/Rust source into/over/out and DWARF rejection cases only')
a=p.parse_args();a.uwvm=a.uwvm.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
root=a.source_root.resolve() if a.source_root else Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
fixtures={
 'exception':'''(module
 (tag $e (param i32))
 (memory 1)
 (data (i32.const 32) "ABC")
 (func $leaf (result i32) i32.const 73 throw $e)
 (func $middle (result i32)
  (block $caught (result i32)
   (try_table (catch $e $caught) call $leaf drop) unreachable)
  i32.const 1 i32.add)
 (func (export "_start") call $middle i32.const 74 i32.ne if unreachable end))''',
 'stdio':'''(module
 (import "wasi_snapshot_preview1" "fd_read" (func $read (param i32 i32 i32 i32) (result i32)))
 (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
 (memory (export "memory") 1)
 (data (i32.const 0) "\\10\\00\\00\\00\\01\\00\\00\\00")
 (data (i32.const 16) "x")
 (data (i32.const 32) "stdio-isolated\\0a")
 (func $denied (param i32)
  local.get 0 i32.const 0 i32.const 1 i32.const 8 call $read
  i32.const 76 i32.ne if unreachable end)
 (func (export "_start")
  i32.const 0 call $denied i32.const 1 call $denied i32.const 2 call $denied
  i32.const 0 i32.const 32 i32.store i32.const 4 i32.const 15 i32.store
  i32.const 1 i32.const 0 i32.const 1 i32.const 8 call $write if unreachable end))''',
 'infinite':'''(module (func (export "_start") (loop $again br $again)))''',
 'dead_branch':'''(module
 (func (export "_start")
  i32.const 0
  if
   i32.const 1
   drop
  end))''',
 'memory64':'''(module
 (memory $m32 1)
 (memory $m64 i64 1)
 (data (memory $m32) (i32.const 32) "M32")
 (data (memory $m64) (i64.const 64) "M64")
 (func (export "_start") i32.const 0 drop))''',
 'source_importer':'''(module
  (import "p" "debug_source_c" (func $source (param i32) (result i32)))
  (func (export "_start")
    i32.const 5 call $source i32.const 36 i32.ne if unreachable end))''',
 'patchable_calls':'''(module
  (type $unary (func (param i32) (result i32)))
  (table 1 funcref)
  (elem (i32.const 0) $add)
  (func $add (type $unary) (param i32) (result i32)
    local.get 0 i32.const 1 i32.add)
  (func $tail (type $unary) (param i32) (result i32)
    local.get 0 i32.eqz
    if (result i32) i32.const 10
    else local.get 0 i32.const 1 i32.sub return_call $tail end)
  (func (export "_start")
    i32.const 1 call $add i32.const 2 i32.ne if unreachable end
    i32.const 9 i32.const 0 call_indirect (type $unary) i32.const 10 i32.ne if unreachable end
    i32.const 4 call $tail i32.const 10 i32.ne if unreachable end))''',
 'patchable_provider':'''(module
  (func $leaf (param i32) (result i32)
    local.get 0 i32.const 1 i32.add)
  (func (export "add") (param i32) (result i32)
    local.get 0 call $leaf))''',
 'patchable_importer':'''(module
  (type $unary (func (param i32) (result i32)))
  (import "p" "add" (func $add (type $unary)))
  (table 1 funcref)
  (elem (i32.const 0) $add)
  (func $twice (type $unary) (param i32) (result i32)
    local.get 0 call $add call $add)
  (func (export "_start")
    i32.const 6 call $add i32.const 7 i32.ne if unreachable end
    i32.const 8 i32.const 0 call_indirect (type $unary) i32.const 9 i32.ne if unreachable end
    i32.const 5 call $twice i32.const 7 i32.ne if unreachable end))''',
 'alias_provider':'''(module
  (type $unary (func (param i32) (result i32)))
  (table (export "tab") 2 funcref)
  (func $old (type $unary) (param i32) (result i32)
    local.get 0 i32.const 1 i32.add)
  (elem (i32.const 0) $old)
  (func (export "dispatch") (type $unary) (param i32) (result i32)
    local.get 0 i32.const 0 call_indirect (type $unary)))''',
 'alias_importer':'''(module
  (type $unary (func (param i32) (result i32)))
  (import "p" "tab" (table 2 funcref))
  (import "p" "dispatch" (func $dispatch (type $unary)))
  (func $new (type $unary) (param i32) (result i32)
    local.get 0 i32.const 2 i32.add)
  (elem (table 0) (i32.const 0) func $new)
  (func (export "_start")
    i32.const 6 call $dispatch i32.const 8 i32.ne if unreachable end
    i32.const 7 i32.const 0 call_indirect (type $unary) i32.const 9 i32.ne if unreachable end
    i32.const 1 ref.func $new table.set 0
    i32.const 5 i32.const 1 call_indirect (type $unary) i32.const 7 i32.ne if unreachable end
    ref.null func i32.const 1 table.grow 0 i32.const 2 i32.ne if unreachable end
    i32.const 2 ref.func $new table.set 0
    i32.const 4 i32.const 2 call_indirect (type $unary) i32.const 6 i32.ne if unreachable end))''',
 'locals':'''(module
 (tag $e (param i32))
 (func (export "_start") (local i32 i64 f32 f64 v128 funcref)
  i32.const 73 local.set 0
  i64.const -17 local.set 1
  f32.const 1.5 local.set 2
  f64.const 2.5 local.set 3
  v128.const i32x4 1 2 3 4 local.set 4
  ref.null func local.set 5
  block $caught (result i32)
    try_table (catch $e $caught)
      i32.const 9 throw $e
    end
    unreachable
  end
  drop))'''
}
for name,wat in fixtures.items():
 (a.out/(name+'.wat')).write_text(wat+'\n')
 subprocess.run([a.wasm_tools,'parse',a.out/(name+'.wat'),'-o',a.out/(name+'.wasm')],check=True)
 subprocess.run([a.wasm_tools,'validate',a.out/(name+'.wasm')],check=True)
source_stems=[]
if a.source_fixture_dir:
 for language in ('c','cpp'):
  for version in (4,5):
   stem=f'source-{language}-dwarf{version}'
   shutil.copyfile(a.source_fixture_dir/f'debug-{stem}.wasm',a.out/(stem+'.wasm'))
   source_stems.append(stem)
if a.rust_source_fixture:
 stem='source-rust-dwarf5'
 shutil.copyfile(a.rust_source_fixture,a.out/(stem+'.wasm'))
 source_stems.append(stem)

def wasm_uleb(data,at,end):
 value=0
 for shift in range(0,70,7):
  assert at<end,'truncated Wasm LEB'
  octet=data[at];at+=1;value|=(octet&127)<<shift
  if not octet&128:return value,at
 raise AssertionError('overlong Wasm LEB')

def wasm_export_function_index(path,name):
 data=path.read_bytes();assert data[:8]==b'\0asm\x01\0\0\0'
 at=8
 while at<len(data):
  section=data[at];at+=1
  size,at=wasm_uleb(data,at,len(data))
  end=at+size;assert end<=len(data)
  if section==7:
   count,at=wasm_uleb(data,at,end)
   for _ in range(count):
    length,at=wasm_uleb(data,at,end)
    assert length<=end-at
    label=data[at:at+length];at+=length
    assert at<end
    kind=data[at];at+=1
    index,at=wasm_uleb(data,at,end)
    if label==name.encode() and kind==0:return index
   break
  at=end
 raise AssertionError(f'{path}: no exported function {name}')

def duplicate_debug_line_section(path,output):
 data=path.read_bytes();at=8
 while at<len(data):
  begin=at;section=data[at];at+=1
  size,at=wasm_uleb(data,at,len(data))
  end=at+size;assert end<=len(data)
  if section==0:
   name_length,pos=wasm_uleb(data,at,end)
   assert name_length<=end-pos
   if data[pos:pos+name_length]==b'.debug_line':
    output.write_bytes(data+data[begin:end])
    subprocess.run([a.wasm_tools,'validate',output],check=True)
    return
  at=end
 raise AssertionError(f'{path}: no .debug_line custom section')

def out_of_code_debug_line_address(path,output):
 data=bytearray(path.read_bytes());at=8;code_size=None;line_begin=None;line_end=None
 while at<len(data):
  section=data[at];at+=1
  size,at=wasm_uleb(data,at,len(data))
  end=at+size;assert end<=len(data)
  if section==10:code_size=size
  elif section==0:
   name_length,pos=wasm_uleb(data,at,end)
   assert name_length<=end-pos
   if data[pos:pos+name_length]==b'.debug_line':
    assert line_begin is None,'duplicate input .debug_line'
    line_begin=pos+name_length;line_end=end
  at=end
 assert code_size is not None and code_size+1<2**32
 assert line_begin is not None and line_end-line_begin>=10
 unit_size=int.from_bytes(data[line_begin:line_begin+4],'little')
 assert unit_size!=0xffffffff and line_begin+4+unit_size<=line_end
 assert int.from_bytes(data[line_begin+4:line_begin+6],'little')==4
 header_size=int.from_bytes(data[line_begin+6:line_begin+10],'little')
 program_begin=line_begin+10+header_size
 unit_end=line_begin+4+unit_size
 assert program_begin<unit_end
 marker=data.find(b'\x00\x05\x02',program_begin,unit_end)
 assert marker>=program_begin and marker+7<=unit_end,'expected DWARF4 Wasm32 set_address'
 data[marker+3:marker+7]=(code_size+1).to_bytes(4,'little')
 output.write_bytes(data)
 subprocess.run([a.wasm_tools,'validate',output],check=True)
rows=[]
class Console:
 def __init__(self,name,policy,cache=None,tag=None,extra_args=()):
  self.log=bytearray();self.pending=bytearray();self.commands=[];self.name=name+'-'+policy+(('-'+tag) if tag else '')
  features=['-WFE-exceptions']
  if name=='memory64':features+=['-WFE-multi-memory','-WFE-memory64']
  if name=='patchable_calls':features+=['-WFE-tail-call']
  if name=='locals':features+=['-WFE-simd','-WFE-function-references']
  if name=='alias_importer':features+=['-WFE-function-references']
  cache_args=['-Rllvm-cache-path','disable'] if cache is None else [
      '-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(a.out/(self.name+'.compile.log'))]
  self.command=[str(a.uwvm),'-m','debug-jit','-Rct','0','-Rllvm-call-stack',policy,*cache_args,*features,*extra_args,'--run',str(a.out/(name+'.wasm'))]
  self.child=subprocess.Popen(self.command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  self.select=selectors.DefaultSelector();self.select.register(self.child.stdout,selectors.EVENT_READ)
  try:self.prompt()
  except BaseException:self.close(False);raise
 def prompt(self):
  limit=time.monotonic()+30;marker=b'(uwvm-debug) '
  while marker not in self.pending:
   assert time.monotonic()<limit,(self.name,'prompt timeout',bytes(self.log)[-4000:])
   ready=self.select.select(max(0,limit-time.monotonic()));assert ready,(self.name,'prompt timeout')
   chunk=os.read(self.child.stdout.fileno(),65536);assert chunk,(self.name,'early exit',self.child.poll(),bytes(self.log)[-6000:])
   self.pending.extend(chunk);self.log.extend(chunk)
  at=self.pending.index(marker)+len(marker);result=bytes(self.pending[:at]);del self.pending[:at];return result
 def send(self,command):
  self.commands.append(command);self.child.stdin.write(command.encode()+b'\n');self.child.stdin.flush();return self.prompt()
 def until(self,marker):
  limit=time.monotonic()+20
  while True:
   reply=self.send('status')
   if marker in reply:return reply
   assert time.monotonic()<limit,(self.name,'state timeout',marker,reply)
   time.sleep(.01)
 def close(self,finish=True):
  if finish:
   self.child.stdin.write(b'quit\n');self.child.stdin.flush();self.child.stdin.close()
   code=self.child.wait(timeout=10);self.log.extend(self.child.stdout.read());assert code==0,(self.name,code,self.log[-4000:])
  else:
   if self.child.poll() is None:self.child.kill()
   self.child.wait()
  (a.out/(self.name+'.log')).write_bytes(self.log)
  rows.append(dict(command=self.command,commands=self.commands,exit=self.child.returncode,passed=finish))
  (a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
  self.select.close()

if source_stems:
 for stem in source_stems:
   c=Console(stem,'unwind')
   try:
    assert b'prepared; no Wasm instruction executed' in c.send('status')
    entry=wasm_export_function_index(a.out/(stem+'.wasm'),'_start')
    assert b'breakpoint 1' in c.send(f'break 0 {entry} 0')
    c.send('continue')
    stopped=c.until(b'stopped: breakpoint')
    for _ in range(64):
     if b'  source ' in stopped:break
     stopped=c.send('step wasm 1')
     assert b'stopped: selected participant step' in stopped,stopped
    assert b'  source ' in stopped,stopped
    first=stopped.split(b'  source ',1)[1].split(b'\n',1)[0]
    stepped=c.send('step source 1 into')
    assert b'stopped: selected participant step' in stepped and b'  source ' in stepped,stepped
    second=stepped.split(b'  source ',1)[1].split(b'\n',1)[0]
    assert first!=second,(first,second,stepped)
    assert b'stopped: selected participant step' in c.send('status')
    c.send('continue');c.until(b'guest exited: 0')
    c.close()
    source_path,source_line,_column=first.rsplit(b':',2)
    source_spec=source_path.decode('utf-8')+':'+source_line.decode('ascii')
    c=Console(stem,'unwind',tag='source-break')
    try:
     assert b'breakpoint 1 source=' in c.send(f'break-source 0 {source_spec}')
     assert b'error: source stepping unavailable' not in c.send('status')
     c.send('continue');source_stop=c.until(b'stopped: breakpoint')
     assert b'  source '+source_path+b':'+source_line+b':' in source_stop,source_stop
     assert b'error: source stepping unavailable' in c.send(f'break-source 0 {source_path.decode("utf-8")}:99999999')
     assert b'breakpoint deleted' in c.send('delete 1')
     c.send('continue');c.until(b'guest exited: 0');c.close()
    except BaseException:
     c.close(False);raise
    if stem == 'source-c-dwarf4':
     helper=wasm_export_function_index(a.out/(stem+'.wasm'),'debug_source_c')
     replacement_body=a.out/'source-c-replacement.bin'
     replacement_body.write_bytes(b'\x00\x20\x00\x41\x07\x6a\x41\x03\x6c\x0b')
     c=Console(stem,'unwind',tag='source-replaced')
     try:
      assert b'function replaced; generation 2' in c.send(f'replace 0 {helper} 1 {replacement_body}')
      assert b'error: source stepping unavailable' in c.send(f'break-source 0 {source_path.decode("utf-8")}:13')
      assert b'error: Wasm byte offset has no emitted executable debug safe point' in c.send(f'break 0 {helper} 1')
      assert b'breakpoint 1' in c.send(f'break 0 {helper} 0')
      c.send('continue');c.until(b'stopped: breakpoint')
      # The replacement declares no locals; the original C body declares
      # several. A parked debugger must inspect the published generation's
      # layout rather than terminate on the stale original local count.
      replacement_locals=c.send('locals 1')
      assert b'local 0 i32=5' in replacement_locals and b'local 1 ' not in replacement_locals,replacement_locals
      assert b'breakpoint deleted' in c.send('delete 1')
      c.send('continue');c.until(b'guest exited: 0');c.close()
     except BaseException:
      c.close(False);raise
   except BaseException:
    c.close(False);raise
 # All three supported -g language frontends have real caller/callee frames.
 # `over` must skip the callee; `out` must return to the Wasm caller. Test
 # both DWARF4 and DWARF5 for C/C++, and the Rust DWARF5 fixture.
 for stem in source_stems:
  language=stem.split('-')[1]
  entry=wasm_export_function_index(a.out/(stem+'.wasm'),'_start')
  callee=wasm_export_function_index(a.out/(stem+'.wasm'),f'debug_source_{language}')
  for policy,origin,destination in (('over',entry,entry),('out',callee,entry)):
   c=Console(stem,'unwind',tag=f'source-{policy}')
   try:
    assert b'breakpoint 1' in c.send(f'break 0 {origin} 0')
    c.send('continue');stopped=c.until(b'stopped: breakpoint')
    for _ in range(64):
     assert f'function={origin}'.encode() in stopped,stopped
     if b'  source ' in stopped:break
     stopped=c.send('step wasm 1')
     assert b'stopped: selected participant step' in stopped,stopped
    else:raise AssertionError((stem,policy,'origin source location not reached'))
    stepped=c.send(f'step source 1 {policy}')
    assert b'stopped: selected participant step' in stepped and f'function={destination}'.encode() in stepped,stepped
    assert b'  source ' in stepped,stepped
    c.send('continue');c.until(b'guest exited: 0');c.close()
   except BaseException:
    c.close(False);raise
 if a.source_fixture_dir:
  c=Console('source_importer','unwind',extra_args=['-Wpre',str(a.out/'source-c-dwarf4.wasm'),'p'])
  try:
   provider_entry=wasm_export_function_index(a.out/'source-c-dwarf4.wasm','debug_source_c')
   for module in (0,1):
    assert b'breakpoint' in c.send(f'break {module} {provider_entry} 0')
   c.send('continue');stopped=c.until(b'stopped: breakpoint')
   assert f'function={provider_entry} byte-offset=0'.encode() in stopped,stopped
   assert b'debug_source_c.c:' in stopped and b'  source ' in stopped,stopped
   first=stopped.split(b'  source ',1)[1].split(b'\n',1)[0]
   stepped=c.send('step source 1 into')
   assert b'stopped: selected participant step' in stepped and b'  source ' in stepped,stepped
   second=stepped.split(b'  source ',1)[1].split(b'\n',1)[0]
   assert first!=second,(first,second,stepped)
   c.send('continue');c.until(b'guest exited: 0')
   c.close()
  except BaseException:
   c.close(False);raise
  stem='source-c-dwarf4-duplicate'
  duplicate_debug_line_section(a.out/'source-c-dwarf4.wasm',a.out/(stem+'.wasm'))
  c=Console(stem,'unwind')
  try:
   entry=wasm_export_function_index(a.out/(stem+'.wasm'),'_start')
   assert b'breakpoint 1' in c.send(f'break 0 {entry} 0')
   c.send('continue');c.until(b'stopped: breakpoint')
   rejected=c.send('step source 1 into')
   assert b'error: source stepping unavailable: DWARF or Code section boundary is invalid' in rejected,rejected
   rejected=c.send('break-source 0 debug_source_c.c:1')
   assert b'error: source stepping unavailable: DWARF or Code section boundary is invalid' in rejected,rejected
   assert b'stopped: breakpoint' in c.send('status')
   c.send('continue');c.until(b'guest exited: 0')
   c.close()
  except BaseException:
   c.close(False);raise
  stem='source-c-dwarf4-out-of-code'
  out_of_code_debug_line_address(a.out/'source-c-dwarf4.wasm',a.out/(stem+'.wasm'))
  c=Console(stem,'unwind')
  try:
   entry=wasm_export_function_index(a.out/(stem+'.wasm'),'_start')
   assert b'breakpoint 1' in c.send(f'break 0 {entry} 0')
   c.send('continue');c.until(b'stopped: breakpoint')
   rejected=c.send('step source 1 into')
   assert b'error: source stepping unavailable: DWARF or Code section boundary is invalid' in rejected,rejected
   assert b'stopped: breakpoint' in c.send('status')
   c.send('continue');c.until(b'guest exited: 0')
   c.close()
  except BaseException:
   c.close(False);raise

if a.source_only:
 assert source_stems,'--source-only requires C/C++ or Rust -g Wasm fixtures'
 print('PASS debug-jit product C/C++/Rust source into/over/out and fail-closed DWARF cases')
 raise SystemExit(0)

c=Console('dead_branch','instruction')
try:
 # Expression: i32.const 0; if; i32.const 1; drop; end; end.
 # Offset 4 is a valid Wasm opcode in a constant-dead arm. O3 removes its
 # bridge call, so it must not remain in the published executable-site map.
 assert b'error: Wasm byte offset has no emitted executable debug safe point' in c.send('break 0 0 4')
 assert b'breakpoint 1' in c.send('break 0 0 0')
 c.send('continue');c.until(b'stopped: breakpoint')
 assert b'breakpoint deleted' in c.send('delete 1')
 c.send('continue');c.until(b'guest exited: 0');c.close()
except BaseException:
 c.close(False);raise

for policy in ('instruction','unwind','none'):
 c=Console('exception',policy)
 try:
  assert b'prepared; no Wasm instruction executed' in c.send('status')
  assert b'error: Wasm byte offset has no emitted executable debug safe point' in c.send('break 0 0 1')
  assert b'error: breakpoint target is not a defined LLVM-full Wasm function' in c.send('break 0 9999 0')
  assert b'error: source stepping unavailable' in c.send('break-source 0 missing.c:1')
  assert b'registered' in c.send('break 0 0 0')
  assert b'breakpoint 1' in c.send('info breakpoints')
  c.send('continue');stopped=c.until(b'stopped: breakpoint')
  assert b'thread 1 module=0 function=0 byte-offset=0' in stopped,stopped
  assert b'memory: 41 42 43' in c.send('memory 0 0 32 3')
  assert b'error: memory module, index or byte range unavailable' in c.send('memory 0 0 65536 1')
  assert b'memory: <empty>' in c.send('memory 0 0 65536 0')
  assert b'error: memory module, index or byte range unavailable' in c.send('memory 0 0 65537 0')
  assert b'error: invalid command' in c.send('memory 0 0 32 257')
  assert b'error: invalid command' in c.send('memory 0 0 18446744073709551615 2')
  bt=c.send('bt 1')
  if policy=='none':assert b'backtrace unavailable' in bt,bt
  else:
   for i,f in enumerate((0,1,2)):assert ('#'+str(i)+' module=0 function='+str(f)).encode() in bt,bt
  help_text=c.send('help')
  assert b'step wasm THREAD' in help_text and b'step source THREAD [into|over|out]' in help_text and b'step asm THREAD' in help_text,help_text
  for source_command in ('step source 1','step source 1 into'):
   reply=c.send(source_command)
   assert b'error: source stepping unavailable' in reply and b'DWARF' in reply,reply
  for source_command in ('step source 1 over','step source 1 out'):
   reply=c.send(source_command)
   assert b'error: source stepping unavailable' in reply and b'DWARF' in reply,reply
  reply=c.send('step asm 1')
  assert b'native instruction 0x' in reply and b' bytes=' in reply and b'stopped: native instruction step' in reply,reply
  assert b'stopped: native instruction step' in c.send('status')
  for invalid_step in ('step source 0','step source 1 next','step asm 1 into','step wasm 0'):
   assert b'error: invalid command' in c.send(invalid_step),invalid_step
  step=c.send('step wasm 1');assert b'stopped: selected participant step' in step and b'function=0 byte-offset=3' in step,step
  # Wasm i32.const 73 uses signed LEB bytes c9 00, so throw begins at offset 3.
  step=c.send('step 1');assert b'stopped: selected participant step' in step and b'function=1' in step,step
  bt=c.send('bt 1')
  if policy!='none':
   assert b'#0 module=0 function=1' in bt and b'#1 module=0 function=2' in bt and b'#2 ' not in bt,bt
  assert b'breakpoint deleted' in c.send('delete 1')
  assert b'error: command exceeds 512 bytes' in c.send('x'*513)
  assert b'error: invalid command' in c.send('break 0 0 18446744073709551616')
  c.send('continue');c.until(b'guest exited: 0')
  assert b'error:' in c.send('memory 0 0 32 3')
  c.close()
 except BaseException:
  c.close(False);raise

c=Console('stdio','instruction')
try:
 c.send('continue');c.until(b'guest exited: 0');assert b'stdio-isolated\n' in c.log,c.log;c.close()
except BaseException:c.close(False);raise
c=Console('infinite','unwind')
try:
 c.send('continue');c.close() # Explicit CLI process termination must not try an unbounded guest join.
except BaseException:c.close(False);raise

c=Console('memory64','instruction')
try:
 assert b'registered' in c.send('break 0 0 0')
 c.send('continue');c.until(b'stopped: breakpoint')
 assert b'memory: 4d 33 32' in c.send('memory 0 0 32 3')
 assert b'memory: 4d 36 34' in c.send('memory 0 1 64 3')
 assert b'memory: <empty>' in c.send('memory 0 1 65536 0')
 assert b'error: memory module, index or byte range unavailable' in c.send('memory 0 1 65537 0')
 c.send('continue');c.until(b'guest exited: 0');c.close()
except BaseException:c.close(False);raise

for policy in ('instruction','unwind'):
 c=Console('patchable_calls',policy)
 try:
  assert b'prepared; no Wasm instruction executed' in c.send('status')
  c.send('continue');c.until(b'guest exited: 0');c.close()
 except BaseException:c.close(False);raise

# Two independent module records keep distinct fixed slot arrays. Provider
# local calls, importer local calls and an imported funcref/table alias all run.
for policy in ('instruction','unwind'):
 c=Console('patchable_importer',policy,extra_args=['-Wpre',str(a.out/'patchable_provider.wasm'),'p'])
 try:
  c.send('continue');c.until(b'guest exited: 0');c.close()
 except BaseException:c.close(False);raise

# The importer active element overwrites an exported provider table after
# debug-full precompilation. Both module aliases must observe later table.set
# and table.grow through one live table domain.
for policy in ('instruction','unwind'):
 c=Console('alias_importer',policy,extra_args=['-Wpre',str(a.out/'alias_provider.wasm'),'p'])
 try:
  c.send('continue');c.until(b'guest exited: 0');c.close()
 except BaseException:c.close(False);raise

# A code image containing a process-local typed-slot base must never be reused
# by a second process even when the host explicitly configures an object cache.
cache=a.out/'patchable-cache'
for phase in ('cold','warm'):
 c=Console('patchable_calls','instruction',cache,phase)
 try:
  c.send('continue');c.until(b'guest exited: 0');c.close()
  compile_log=(a.out/(c.name+'.compile.log')).read_text()
  assert 'object-cache-hit' not in compile_log,compile_log
  assert not any(path.is_file() for path in cache.rglob('*')),list(cache.rglob('*'))
 except BaseException:c.close(False);raise

# A module whose SIMD/reference signature cannot use the typed slot ABI
# still embeds process-local debugger observer addresses in generated IR.
for phase in ('fallback-cold','fallback-warm'):
 c=Console('locals','instruction',cache,phase)
 try:
  c.send('continue');c.until(b'guest exited: 0');c.close()
  compile_log=(a.out/(c.name+'.compile.log')).read_text()
  assert 'object-cache-hit' not in compile_log,compile_log
  assert not any(path.is_file() for path in cache.rglob('*')),list(cache.rglob('*'))
 except BaseException:c.close(False);raise

c=Console('locals','instruction')
try:
 assert b'error:' in c.send('locals 1') # Prepared state has no guest frame.
 assert b'registered' in c.send('break 0 0 0')
 c.send('continue');c.until(b'stopped: breakpoint')
 initial=c.send('locals 1')
 assert b'local 0 i32=0' in initial and b'local 1 i64=0' in initial,initial
 assert b'error:' in c.send('locals 999999')
 expected=(b'local 0 i32=73',b'local 1 i64=-17',b'local 2 f32 bits=0x3fc00000',
           b'local 3 f64 bits=0x4004000000000000',b'local 4 v128 bytes=01000000',
           b'local 5 funcref=null')
 seen=set()
 for _ in range(20):
  step=c.send('step 1');assert b'stopped: selected participant step' in step,step
  values=c.send('locals 1')
  seen.update(item for item in expected if item in values)
  if len(seen)==len(expected):break
 assert len(seen)==len(expected),(seen,expected,bytes(c.log)[-5000:])
 c.send('continue');c.until(b'guest exited: 0');c.close()
except BaseException:c.close(False);raise

incompatible=[['-Rint']] if a.ros else [['-Rcc','int','-Rcm','full'],['-Rcc','jit','-Rcm','lazy'],['-Rjit'],['-Rint'],['-Rcc','tiered']]
for i,args in enumerate(incompatible):
 cmd=[str(a.uwvm),'-m','debug-jit',*args,'--log-color','enable','--run',str(a.out/'infinite.wasm')]
 r=subprocess.run(cmd,input=b'quit\n',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
 (a.out/f'incompatible-{i}.log').write_bytes(r.stdout)
 assert r.returncode!=0 and b'[fatal]' in r.stdout and b'unsupported in the current mode' in r.stdout and b'\x1b[' in r.stdout and b'(uwvm-debug)' not in r.stdout,(cmd,r.returncode,r.stdout)
 rows.append(dict(command=cmd,exit=r.returncode,passed=True,expected='fatal incompatible mode'))
(a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,processes=len(rows),binary_sha256=hashlib.file_digest(a.uwvm.open('rb'),'sha256').hexdigest(),scope='Real CLI break/Wasm-step/bt, source-step fail-closed responses and exact native assembly step, typed stopped-frame locals across Core3 try_table/SIMD/references, and bounded stopped-memory reads across native throw/catch, none policy, parser bounds, isolated WASI stdin/stdout/stderr capabilities, bounded infinite-guest quit, cross-module live-table alias/active segment/set/grow routing, cold/warm cross-process debug cache isolation, colored incompatible-mode rejection.'),indent=2)+'\n')
print('PASS actual debug-jit CLI',len(rows),'processes')
