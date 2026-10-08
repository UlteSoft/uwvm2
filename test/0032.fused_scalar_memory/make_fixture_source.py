import hashlib,json,pathlib
T=pathlib.Path(__file__).parent
F=T/'fixtures';F.mkdir(exist_ok=True)
def write(name,text): (F/name).write_text(text)
# Independent fixture producer: byte data and golden integers are calculated
# here without executing C++/Wasm or consulting the product validator.
loads=[('i32.load','i32',4,False),('i64.load','i64',8,False),('f32.load','f32',4,False),('f64.load','f64',8,False),
 ('i32.load8_s','i32',1,True),('i32.load8_u','i32',1,False),('i32.load16_s','i32',2,True),('i32.load16_u','i32',2,False),
 ('i64.load8_s','i64',1,True),('i64.load8_u','i64',1,False),('i64.load16_s','i64',2,True),('i64.load16_u','i64',2,False),
 ('i64.load32_s','i64',4,True),('i64.load32_u','i64',4,False)]
stores=[('i32.store','i32',4),('i64.store','i64',8),('f32.store','f32',4),('f64.store','f64',8),('i32.store8','i32',1),('i32.store16','i32',2),('i64.store8','i64',1),('i64.store16','i64',2),('i64.store32','i64',4)]
pattern=bytes.fromhex('81 80 03 84 05 06 07 08')
escaped=''.join('\\'+format(x,'02x') for x in pattern)
lines=[';; Core3 memory64+mixed memory32/memory64 scalar normalized event gate.','(module $scalar',
 '(memory $m32 1 1)','(memory $m64 i64 1 1)',f'(data (memory $m32) (i32.const 32) "{escaped}")',f'(data (memory $m64) (i64.const 32) "{escaped}")']
oracles=[]
for memory,address in (('$m32','i32'),('$m64','i64')):
 for index,(op,value,width,signed) in enumerate(loads):
  name=f'{address}_load_{index}';result='i32' if value in ('i32','f32') else 'i64'
  reinterpret='' if value in ('i32','i64') else f' {result}.reinterpret_{value}'
  lines.append(f'(func (export "{name}") (result {result}) {address}.const 32 {op} {memory}{reinterpret})')
  expected=int.from_bytes(pattern[:width],'little',signed=signed)
  # WAST allows unsigned integer bits; use positive full-width form for f32/f64 bit reinterpretation.
  oracles.append((name,result,expected))
 for index,(op,value,width) in enumerate(stores):
  name=f'{address}_store_{index}';result='i32' if value in ('i32','f32') else 'i64'
  bits=0x8877665544332211 if result=='i64' else 0x88776655
  source=f'{result}.const {bits}'
  if value in ('f32','f64'):source+=f' {value}.reinterpret_{result}'
  load=f'{result}.load' if width==(8 if result=='i64' else 4) else f'{result}.load{width*8}_u'
  lines.append(f'(func (export "{name}") (result {result}) {address}.const 128 {source} {op} {memory} {address}.const 128 {load} {memory})')
  oracles.append((name,result,bits&((1<<(width*8))-1)))
lines.append(')')
for name,result,expected in oracles:lines.append(f'(assert_return (invoke $scalar "{name}") ({result}.const {expected}))')
write('scalar_memory32_memory64_all23.wast','\n'.join(lines)+'\n')
write('scalar_memory64_boundary.wast',''';; Valid wide offset in unreachable code; runtime overflow must trap, never wrap.
(module $wide
 (memory i64 1 1)
 (func (export "wide_unused") unreachable i32.load offset=18446744073709551615 drop)
 (func (export "overflow") (result i32) i64.const 1 i32.load offset=18446744073709551615)
 (func (export "oob") (result i32) i64.const 65533 i32.load))
(assert_trap (invoke $wide "overflow") "out of bounds memory access")
(assert_trap (invoke $wide "oob") "out of bounds memory access")
(assert_invalid (module (memory i64 1) (func i32.const 0 i32.load drop)) "type mismatch")
(assert_invalid (module (memory i64 1) (func i64.const 0 i64.const 0 i32.store)) "type mismatch")
(assert_invalid (module (memory i64 1) (func i64.const 0 i32.load align=8 drop)) "alignment must not be larger than natural")
(assert_invalid (module (memory 1) (func unreachable i32.load offset=4294967296 drop)) "offset out of range")
''')
# The 32-function shape prevents instruction-mode LLVM's <=16-function adjacent
# warmup from accidentally validating the invalid unused final function at startup.
unused=['(module $unused (func (export "entry"))']
unused += ['(func nop)' for _ in range(30)]
unused += ['(func $bad local.get 0 drop))']
write('unused_invalid_function_32.wat','\n'.join(unused)+'\n')
active=['(module $pre_effect (import "provider" "memory" (memory 1)) (data (i32.const 0) "X") (func (export "entry"))']
active += ['(func nop)' for _ in range(30)]
active += ['(func $bad local.get 0 drop))']
write('unused_invalid_before_imported_memory_effect_32.wat','\n'.join(active)+'\n')
write('unused_invalid_before_imported_memory_effect_32.contract.json',json.dumps({
 'source_only':True,'native_executed':False,'validation_result':'reject invalid local index even if function31 never called',
 'precondition':'real provider instance has exported memory with byte0 equal to 0x41',
 'postcondition_after_failed_instantiation':'provider byte0 remains 0x41; active data X must never be applied',
 'modes':'all public Core3 int/LLVM full/lazy/verification/tiered; ROS full only',
 'lazy_config':'threads=0; bad index31 beyond <=16 adjacent warmup; not an executable command contract',
 'old_validators_changed':False},indent=2)+'\n')
# Binary malformed memarg encoding producer. No text assembler can preserve these bytes.
def uleb(n):
 out=[]
 while True:
  b=n&127;n>>=7;out.append(b|(128 if n else 0))
  if not n:return bytes(out)
def section(n,payload):return bytes([n])+uleb(len(payload))+payload
def module(memarg,memory64=True):
 types=section(1,b'\x01\x60\x00\x00');funcs=section(3,b'\x01\x00')
 memories=section(5,b'\x01'+(b'\x04' if memory64 else b'\x00')+b'\x01')
 expr=b'\x00\x00\x28'+memarg+b'\x1a\x0b' # locals0, unreachable, i32.load, drop, end
 return b'\0asm\x01\0\0\0'+types+funcs+memories+section(10,b'\x01'+uleb(len(expr))+expr)
raw=[]
for name,argument,reason in [
 ('memarg_reserved_bit7',b'\x80\x01\x00','reserved alignment flag'),
 ('memarg_missing_optional_index',b'\x42','optional memory index truncated before offset'),
 ('memarg_bad_optional_index',b'\x42\x01\x00','selected memory index1 unavailable'),
 ('memarg_u64_overflow',b'\x02'+b'\xff'*9+b'\x02','offset exceeds u64'),
 ('memarg_u64_unterminated',b'\x02'+b'\x80'*10,'u64 offset never terminates')]:
 # For the missing-index case, terminate code before the optional field so body structure cannot donate bytes.
 data=module(argument)
 if name=='memarg_missing_optional_index':
  expr=b'\x00\x00\x28'+argument
  prefix=data[:data.index(b'\x0a',8)] if False else b'\0asm\x01\0\0\0'+section(1,b'\x01\x60\x00\x00')+section(3,b'\x01\x00')+section(5,b'\x01\x04\x01')
  data=prefix+section(10,b'\x01'+uleb(len(expr))+expr)
 (F/(name+'.wasm')).write_bytes(data);raw.append({'path':name+'.wasm','expect':'reject','reason':reason,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
# Independent semantic oracle claims are still pending actual reference/product execution.
manifest={'source_only':True,'native_executed':False,'expected_values_from_independent_integer_arithmetic':True,
 'all23_scalar_on_memory32_and_memory64':46,'load_store_assertions':[{'export':n,'type':t,'value':v} for n,t,v in oracles],
 'malformed_binary_cases':raw,'features':['memory64','multi-memory','Core3 u64 memarg'],
 'requires_actual_reference_tool_qualification':True,'runtime_vs_feature_off_contract':{
  'memory64-off':'all modules declaring i64 memory rejected before body',
  'multi-memory-off':'two-memory module and explicit nonzero selected memory rejected',
  'explicit scoped Core3 memory64-off memory32':'still accepts full Core3 u64 binary encoding when value<2^32; not legacy u32 reader'},
 'all_products_and_modes_native_pending':True}
(F/'oracle.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'fixture_files':len(list(F.iterdir())),'assertions':len(oracles),'source_only':True,'native_executed':False}))
