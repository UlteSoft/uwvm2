#!/usr/bin/env python3
"""Compare aggregate/i31/reference-equality typing on exact new-syntax bytes with Wasmtime."""
from run_reference_validation import main
CASES = {}
def case(name, valid, types, signature, body, suffix=''):
    CASES[name] = valid, f'{types} (func {signature} {body}) {suffix}'
case('struct-new-mixed',True,'(type $s (struct (field i32) (field i8) (field (ref null eq))))','(result (ref $s))','i32.const 1 i32.const -1 ref.null i31 struct.new $s')
case('struct-new-order',False,'(type $s (struct (field i32) (field f64)))','','f64.const 0 i32.const 1 struct.new $s drop')
case('struct-new-default',True,'(type $s (struct (field i16) (field externref)))','(result (ref $s))','struct.new_default $s')
case('struct-new-nondefault',False,'(type $s (struct (field (ref extern))))','','struct.new_default $s drop')
case('struct-new-array-type',False,'(type $s (array i32))','','struct.new $s drop')
case('struct-new-empty',True,'(type $s (struct))','(result (ref $s))','struct.new $s')
for packed in ('i8','i16'):
 for signed in ('s','u'):
  case(f'struct-get-{packed}-{signed}',True,f'(type $s (struct (field {packed})))','(param (ref null $s)) (result i32)',f'local.get 0 struct.get_{signed} $s 0')
 case(f'struct-get-unpacked-{packed}',False,f'(type $s (struct (field {packed})))','(param (ref null $s)) (result i32)','local.get 0 struct.get $s 0')
case('struct-get-numeric',True,'(type $s (struct (field f64)))','(param (ref null $s)) (result f64)','local.get 0 struct.get $s 0')
case('struct-get-signed-numeric',False,'(type $s (struct (field i32)))','(param (ref null $s)) (result i32)','local.get 0 struct.get_s $s 0')
case('struct-get-bad-field',False,'(type $s (struct))','(param (ref null $s)) (result i32)','local.get 0 struct.get $s 0')
case('struct-get-wrong-ref',False,'(type $s (struct (field i32)))','(param anyref) (result i32)','local.get 0 struct.get $s 0')
case('struct-get-subtype',True,'(type $s (sub (struct (field anyref)))) (type $t (sub $s (struct (field eqref) (field i32))))','(param (ref null $t)) (result anyref)','local.get 0 struct.get $s 0')
case('struct-get-declared-type',False,'(type $s (sub (struct (field anyref)))) (type $t (sub $s (struct (field eqref))))','(param (ref null $t)) (result eqref)','local.get 0 struct.get $s 0')
for mutable in (False,True):
 field='(mut i16)' if mutable else 'i16'
 case('struct-set-'+str(mutable),mutable,f'(type $s (struct (field {field})))','(param (ref null $s))','local.get 0 i32.const 17 struct.set $s 0')
case('array-new',True,'(type $a (array i8))','(result (ref $a))','i32.const -1 i32.const 3 array.new $a')
case('array-new-count-type',False,'(type $a (array i32))','','i32.const 0 f64.const 1 array.new $a drop')
case('array-default',True,'(type $a (array externref))','(result (ref $a))','i32.const 0 array.new_default $a')
case('array-nondefault',False,'(type $a (array (ref extern)))','','i32.const 0 array.new_default $a drop')
case('array-fixed',True,'(type $a (array i16))','(result (ref $a))','i32.const 1 i32.const 2 array.new_fixed $a 2')
case('array-fixed-missing',False,'(type $a (array i32))','','i32.const 1 array.new_fixed $a 2 drop')
case('array-fixed-zero',True,'(type $a (array (ref extern)))','(result (ref $a))','array.new_fixed $a 0')
case('array-fixed-not-array',False,'(type $a (func))','','array.new_fixed $a 0 drop')
for storage in ('i8','i16','i32','f64'):
 for suffix in ('','_s','_u'):
  valid=(storage in ('i8','i16')) == bool(suffix)
  result='f64' if storage=='f64' else 'i32'
  case('array-get-'+storage+suffix,valid,f'(type $a (array {storage}))',f'(param (ref null $a)) (result {result})',f'local.get 0 i32.const 0 array.get{suffix} $a')
for mutable in (False,True):
 field='(mut i8)' if mutable else 'i8'
 case('array-set-'+str(mutable),mutable,f'(type $a (array {field}))','(param (ref null $a))','local.get 0 i32.const 0 i32.const 1 array.set $a')
 case('array-fill-'+str(mutable),mutable,f'(type $a (array {field}))','(param (ref null $a))','local.get 0 i32.const 0 i32.const 1 i32.const 0 array.fill $a')
case('array-len-bottom',True,'','(result i32)','ref.null none array.len')
case('array-len-eq',False,'','(param eqref) (result i32)','local.get 0 array.len')
copy='local.get 0 i32.const 0 local.get 1 i32.const 0 i32.const 0 array.copy $d $s'
for name,dst,src,valid in [('packed-same','i8','i8',True),('packed-width','i8','i16',False),('packed-unpacked','i32','i8',False),('reference-covariance','anyref','eqref',True),('reference-contravariance','eqref','anyref',False)]:
 case('array-copy-'+name,valid,f'(type $d (array (mut {dst}))) (type $s (array {src}))','(param (ref null $d) (ref null $s))',copy)
case('array-copy-immutable',False,'(type $d (array i32)) (type $s (array i32))','(param (ref null $d) (ref null $s))',copy)
for storage in ('i8','i16','i32','f64','v128','externref'):
 case('array-new-data-'+storage,storage!='externref',f'(type $a (array {storage}))','(result (ref $a))','i32.const 0 i32.const 0 array.new_data $a 0','(data "abc")')
case('array-new-data-missing',False,'(type $a (array i8))','','i32.const 0 i32.const 0 array.new_data $a 0 drop')
for mutable in (False,True):
 field='(mut i8)' if mutable else 'i8'
 case('array-init-data-'+str(mutable),mutable,f'(type $a (array {field}))','(param (ref null $a))','local.get 0 i32.const 0 i32.const 0 i32.const 0 array.init_data $a 0','(data "abc")')
for storage,element,valid in [('anyref','eqref',True),('eqref','anyref',False),('externref','externref',True),('i8','externref',False)]:
 heap={'eqref':'eq','anyref':'any','externref':'extern'}[element]
 case('array-new-elem-'+storage,valid,f'(type $a (array {storage}))','(result (ref $a))','i32.const 0 i32.const 0 array.new_elem $a 0',f'(elem {element} (ref.null {heap}))')
case('array-init-elem',True,'(type $a (array (mut anyref)))','(param (ref null $a))','local.get 0 i32.const 0 i32.const 0 i32.const 0 array.init_elem $a 0','(elem eqref (ref.null eq))')
case('array-init-elem-immutable',False,'(type $a (array anyref))','(param (ref null $a))','local.get 0 i32.const 0 i32.const 0 i32.const 0 array.init_elem $a 0','(elem eqref (ref.null eq))')
case('ref-i31',True,'','(result (ref i31))','i32.const -1 ref.i31')
case('ref-i31-wrong-input',False,'','','f64.const 0 ref.i31 drop')
for signed in ('s','u'):
 case('i31-get-'+signed,True,'','(param (ref null i31)) (result i32)',f'local.get 0 i31.get_{signed}')
case('i31-get-eq',False,'','(param eqref) (result i32)','local.get 0 i31.get_u')
case('ref-eq',True,'','(result i32)','ref.null none i32.const 1 ref.i31 ref.eq')
case('ref-eq-func',False,'','(result i32)','ref.null func ref.null func ref.eq')
case('array-fixed-unreachable-concrete',False,'(type $a (array i32))','','unreachable f64.const 0 array.new_fixed $a 2 drop')
if __name__ == '__main__':
    main(CASES, 'aggregate/i31/reference-equality typing subset')
