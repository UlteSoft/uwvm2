;; Core 3 GC ref.eq compares null, i31 values, and aggregate identity.
(module
  (type $empty (struct))
  (func (export "_start")
    ref.null none
    ref.null i31
    ref.eq
    i32.eqz
    if unreachable end

    i32.const 7
    ref.i31
    i32.const 7
    ref.i31
    ref.eq
    i32.eqz
    if unreachable end

    i32.const 7
    ref.i31
    i32.const 8
    ref.i31
    ref.eq
    if unreachable end

    struct.new $empty
    struct.new $empty
    ref.eq
    if unreachable end))
