(module
  (type $record (struct (field i32)))

  (func $test_struct (export "test_struct") (result i32)
    i32.const 7
    struct.new $record
    ref.test (ref $record))

  (func $test_i31 (export "test_i31") (result i32)
    i32.const 7
    ref.i31
    ref.test (ref $record))

  (func $test_null_nonnullable (export "test_null_nonnullable") (result i32)
    ref.null any
    ref.test (ref $record))

  (func $test_null_nullable (export "test_null_nullable") (result i32)
    ref.null any
    ref.test (ref null $record))

  (func $cast_struct (export "cast_struct") (result i32)
    i32.const 42
    struct.new $record
    ref.cast (ref $record)
    struct.get $record 0)

  (func $cast_null_nullable (export "cast_null_nullable") (result i32)
    ref.null any
    ref.cast (ref null $record)
    ref.is_null)

  (func (export "cast_i31_trap") (result i32)
    i32.const 7
    ref.i31
    ref.cast (ref $record)
    struct.get $record 0)

  (func (export "_start")
    call $test_struct
    i32.const 1
    i32.ne
    if unreachable end
    call $test_i31
    if unreachable end
    call $test_null_nonnullable
    if unreachable end
    call $test_null_nullable
    i32.const 1
    i32.ne
    if unreachable end
    call $cast_struct
    i32.const 42
    i32.ne
    if unreachable end
    call $cast_null_nullable
    i32.const 1
    i32.ne
    if unreachable end)
)
