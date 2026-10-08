;; Core 3 GC: every abstract aggregate null remains a real, nullable reference.
(module
  (func (export "_start")
    ref.null i31
    ref.is_null
    i32.eqz
    if unreachable end

    ref.null struct
    ref.is_null
    i32.eqz
    if unreachable end

    ref.null array
    ref.is_null
    i32.eqz
    if unreachable end

    ref.null any
    ref.is_null
    i32.eqz
    if unreachable end

    ref.null none
    ref.is_null
    i32.eqz
    if unreachable end))
