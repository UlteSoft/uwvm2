;; Wide and reference fields always retain the original generic route.
(module
  (type $s (struct (field (mut i64)) (field (mut f64)) (field (mut (ref null eq)))))
  (func (export "_start") (local $r (ref $s))
    (local.set $r (struct.new_default $s))
    (struct.set $s 0 (local.get $r) (i64.const 0x1234567887654321))
    (struct.set $s 1 (local.get $r) (f64.reinterpret_i64 (i64.const 0x7ff0123456789abc)))
    (struct.set $s 2 (local.get $r) (ref.i31 (i32.const 123)))
    (if (i64.ne (struct.get $s 0 (local.get $r)) (i64.const 0x1234567887654321)) (then unreachable))
    (if (i64.ne (i64.reinterpret_f64 (struct.get $s 1 (local.get $r))) (i64.const 0x7ff0123456789abc)) (then unreachable))
    (if (i32.ne (i31.get_u (ref.cast (ref i31) (struct.get $s 2 (local.get $r)))) (i32.const 123)) (then unreachable))))
