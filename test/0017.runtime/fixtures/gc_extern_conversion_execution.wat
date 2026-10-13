;; Core 3 external conversions retain null, i31, aggregate values and identity.
(module
  (type $record (struct (field i32)))
  (type $numbers (array (mut i32)))
  (table $extern_table 1 externref)
  (global $extern_global (mut externref) (ref.null extern))

  (func $i31_roundtrip (result i32)
    i32.const 31
    ref.i31
    extern.convert_any
    any.convert_extern
    ref.cast (ref i31)
    i31.get_u)

  (func $struct_roundtrip (result i32)
    i32.const 42
    struct.new $record
    extern.convert_any
    any.convert_extern
    ref.cast (ref $record)
    struct.get $record 0)

  (func $array_roundtrip (result i32)
    i32.const 3
    i32.const 4
    array.new_fixed $numbers 2
    extern.convert_any
    any.convert_extern
    ref.cast (ref $numbers)
    array.len)

  (func $null_roundtrip (result i32)
    ref.null any
    extern.convert_any
    any.convert_extern
    ref.is_null)

  (func $identity_roundtrip (result i32) (local $external externref)
    i32.const 7
    ref.i31
    extern.convert_any
    local.set $external
    local.get $external
    any.convert_extern
    ref.cast (ref i31)
    local.get $external
    any.convert_extern
    ref.cast (ref i31)
    ref.eq)

  (func $table_roundtrip (result i32)
    i32.const 0
    i32.const 9
    ref.i31
    extern.convert_any
    table.set $extern_table
    i32.const 0
    table.get $extern_table
    any.convert_extern
    ref.cast (ref i31)
    i31.get_u)

  (func $global_roundtrip (result i32)
    i32.const 11
    ref.i31
    extern.convert_any
    global.set $extern_global
    global.get $extern_global
    any.convert_extern
    ref.cast (ref i31)
    i31.get_u)

  (func (export "_start")
    call $i31_roundtrip
    i32.const 31
    i32.ne
    if unreachable end
    call $struct_roundtrip
    i32.const 42
    i32.ne
    if unreachable end
    call $array_roundtrip
    i32.const 2
    i32.ne
    if unreachable end
    call $null_roundtrip
    i32.const 1
    i32.ne
    if unreachable end
    call $identity_roundtrip
    i32.const 1
    i32.ne
    if unreachable end
    call $table_roundtrip
    i32.const 9
    i32.ne
    if unreachable end
    call $global_roundtrip
    i32.const 11
    i32.ne
    if unreachable end))
