(module
  ;; The polymorphic operand isolates the ref.test heap immediate.  With GC on
  ;; and exceptions off, the exn heap must still be rejected by validation.
  (func $probe
    unreachable
    ref.test (ref null exn)
    drop)
  (func (export "_start")
    call $probe))
