;; Core 3 GC: the low 31 bits survive ref.i31; signed and unsigned reads differ.
(module
  (func (export "_start")
    i32.const -1
    ref.i31
    i31.get_s
    i32.const -1
    i32.ne
    if unreachable end

    i32.const -1
    ref.i31
    i31.get_u
    i32.const 2147483647
    i32.ne
    if unreachable end))
