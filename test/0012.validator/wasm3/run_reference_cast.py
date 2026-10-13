#!/usr/bin/env python3
"""Check Core 3 casts/conversions and function-label cast branches against Wasmtime."""
from run_reference_validation import main
CASES={}
def case(name,valid,signature,body,types=''):
    CASES[name]=valid,f'{types} (func {signature} {body})'
for op in ('ref.test','ref.cast'):
 for nullable in (False,True):
  target='(ref null $s)' if nullable else '(ref $s)'
  result='i32' if op=='ref.test' else target
  case(op+'-sibling-'+str(nullable),True,f'(param (ref null $a)) (result {result})',f'local.get 0 {op} {target}','(type $s (struct)) (type $a (array i32))')
  case(op+'-cross-hierarchy-'+str(nullable),False,f'(param externref) (result {result})',f'local.get 0 {op} {target}','(type $s (struct))')
case('test-typed-function',True,'(param funcref) (result i32)','local.get 0 ref.test (ref $f)','(type $f (func))')
case('test-extern-bottom',True,'(param externref) (result i32)','local.get 0 ref.test (ref null noextern)')
case('test-exn',True,'(param exnref) (result i32)','local.get 0 ref.test (ref noexn)')
case('test-unknown-type',False,'(param anyref) (result i32)','local.get 0 ref.test (ref 99)')
case('cast-result-not-input',False,'(param (ref i31)) (result (ref i31))','local.get 0 ref.cast (ref any)')
case('cast-nullable-not-input',False,'(param (ref i31)) (result (ref i31))','local.get 0 ref.cast (ref null i31)')
case('cast-bottom-still-reference',False,'(result i32)','unreachable ref.cast (ref i31)')
case('test-numeric',False,'(result i32)','i32.const 0 ref.test (ref i31)')
for extern_to_any in (False,True):
 op='any.convert_extern' if extern_to_any else 'extern.convert_any'
 src,dst=('extern','any') if extern_to_any else ('any','extern')
 for nullable in (False,True):
  src_type=f'(ref {"null " if nullable else ""}{src})';dst_type=f'(ref {"null " if nullable else ""}{dst})'
  case(op+str(nullable),True,f'(param {src_type}) (result {dst_type})',f'local.get 0 {op}')
 case(op+'-nullable-to-nonnull',False,f'(param (ref null {src})) (result (ref {dst}))',f'local.get 0 {op}')
 case(op+'-bottom',True,f'(result (ref {dst}))',f'unreachable {op}')
 case(op+'-bottom-numeric',False,'(result i32)',f'unreachable {op}')
 case(op+'-wrong-hierarchy',False,'(param funcref)',f'local.get 0 {op} drop')
case('convert-principal-any',False,'(result (ref i31))','unreachable any.convert_extern')
case('convert-principal-extern',False,'(result (ref noextern))','unreachable extern.convert_any')
case('convert-global-bottom',True,'(result (ref extern))','unreachable ref.as_non_null extern.convert_any')
for op in ('br_on_cast','br_on_cast_fail'):
 case(op+'-basic',True,'(param anyref) (result anyref)',f'local.get 0 {op} 0 (ref null any) (ref i31)')
 case(op+'-nullable',True,'(param anyref) (result anyref)',f'local.get 0 {op} 0 (ref null any) (ref null i31)')
 case(op+'-not-downcast',False,'(param eqref) (result anyref)',f'local.get 0 {op} 0 (ref null eq) (ref any)')
 case(op+'-cross-hierarchy',False,'(param anyref) (result anyref)',f'local.get 0 {op} 0 (ref null any) (ref extern)')
 case(op+'-empty-label',False,'(param anyref)',f'local.get 0 {op} 0 (ref null any) (ref i31) drop')
 case(op+'-missing-label',False,'(param anyref) (result anyref)',f'local.get 0 {op} 1 (ref null any) (ref i31)')
 case(op+'-numeric-label',False,'(param anyref) (result i32)',f'local.get 0 {op} 0 (ref null any) (ref i31) drop i32.const 0')
 case(op+'-label-prefix',True,'(param anyref) (result i32 anyref)',f'i32.const 17 local.get 0 {op} 0 (ref null any) (ref i31)')
 case(op+'-reified-unreachable-prefix',False,'(param f64) (result i32 anyref)',f'unreachable {op} 0 (ref null any) (ref i31) drop local.set 0 i32.const 0 ref.null any')
case('br-cast-null-excluded',True,'(param anyref) (result anyref) (local (ref any))','local.get 0 br_on_cast 0 (ref null any) (ref null i31) local.tee 1')
case('br-cast-null-retained',False,'(param anyref) (result anyref) (local (ref any))','local.get 0 br_on_cast 0 (ref null any) (ref i31) local.tee 1')
case('br-fail-result-refined',True,'(param anyref) (result anyref) (local (ref i31))','local.get 0 br_on_cast_fail 0 (ref null any) (ref i31) local.tee 1')
case('br-fail-result-nullable',False,'(param anyref) (result anyref) (local (ref i31))','local.get 0 br_on_cast_fail 0 (ref null any) (ref null i31) local.tee 1')
case('br-fail-label-null-excluded',True,'(param anyref) (result (ref any))','local.get 0 br_on_cast_fail 0 (ref null any) (ref null i31) ref.as_non_null')
case('br-fail-label-null-retained',False,'(param anyref) (result (ref any))','local.get 0 br_on_cast_fail 0 (ref null any) (ref i31)')
if __name__=='__main__':main(CASES,'cast/conversion/function-label branch typing subset')
