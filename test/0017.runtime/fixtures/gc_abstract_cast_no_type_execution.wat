;; Core 3 abstract GC instructions with only an implicit legacy function type.
;; No recursive group, struct, array, or defined heap type is present.
(module
  (func (export "_start")
    i32.const 7
    ref.i31
    ref.test (ref i31)
    i32.const 1
    i32.ne
    if unreachable end

    ref.null any
    ref.test (ref i31)
    if unreachable end

    ref.null any
    ref.test (ref null i31)
    i32.const 1
    i32.ne
    if unreachable end

    i32.const 31
    ref.i31
    ref.cast (ref i31)
    i31.get_u
    i32.const 31
    i32.ne
    if unreachable end

    ref.null any
    ref.cast (ref null i31)
    ref.is_null
    i32.eqz
    if unreachable end)

  (func (export "cast_null_failure")
    ref.null any
    ref.cast (ref i31)
    drop))
