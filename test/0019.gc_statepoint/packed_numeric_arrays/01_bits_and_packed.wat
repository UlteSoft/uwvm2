(module
  (rec
    (type $bytes (array (mut i8)))
    (type $halves (array (mut i16)))
    (type $words (array (mut i32)))
    (type $wide (array (mut i64)))
    (type $single (array (mut f32)))
    (type $double (array (mut f64)))
    (type $vectors (array (mut v128)))
    (type $readonly (array i32)))
  (func (export "run") (result i32)
    (local $b (ref null $bytes)) (local $h (ref null $halves))
    (local $w (ref null $words)) (local $d (ref null $wide))
    (local $f (ref null $single)) (local $g (ref null $double))
    (local $v (ref null $vectors)) (local $r (ref null $readonly))
    i32.const 0x12345680 i32.const 3 array.new $bytes local.set $b
    local.get $b i32.const 0 array.get_u $bytes i32.const 128 i32.ne if unreachable end
    local.get $b i32.const 1 array.get_s $bytes i32.const -128 i32.ne if unreachable end
    local.get $b i32.const 2 i32.const 0x123456ff array.set $bytes
    local.get $b i32.const 2 array.get_s $bytes i32.const -1 i32.ne if unreachable end
    i32.const 0x123480f1 i32.const 2 array.new $halves local.set $h
    local.get $h i32.const 0 array.get_u $halves i32.const 0x80f1 i32.ne if unreachable end
    local.get $h i32.const 1 array.get_s $halves i32.const 0xffff80f1 i32.ne if unreachable end
    i32.const 0x87654321 i32.const 1 array.new $words local.set $w
    local.get $w i32.const 0 array.get $words i32.const 0x87654321 i32.ne if unreachable end
    i64.const 0x8765432101234567 i32.const 1 array.new $wide local.set $d
    local.get $d i32.const 0 array.get $wide i64.const 0x8765432101234567 i64.ne if unreachable end
    i32.const 0x7f800123 f32.reinterpret_i32 i32.const 1 array.new $single local.set $f
    local.get $f i32.const 0 array.get $single i32.reinterpret_f32
    i32.const 0x7f800123 i32.ne if unreachable end
    i64.const 0x7ff0000000000123 f64.reinterpret_i64 i32.const 1 array.new $double local.set $g
    local.get $g i32.const 0 array.get $double i64.reinterpret_f64
    i64.const 0x7ff0000000000123 i64.ne if unreachable end
    v128.const i32x4 0x11223344 -1 0x55667788 0x89abcdef
    i32.const 1 array.new $vectors local.set $v
    local.get $v i32.const 0 array.get $vectors i32x4.extract_lane 0
    i32.const 0x11223344 i32.ne if unreachable end
    local.get $v i32.const 0 array.get $vectors i32x4.extract_lane 3
    i32.const 0x89abcdef i32.ne if unreachable end
    i32.const 17 i32.const 19 array.new_fixed $readonly 2 local.set $r
    local.get $r i32.const 1 array.get $readonly i32.const 19 i32.ne if unreachable end
    i32.const 0 array.new_default $words array.len i32.eqz if else unreachable end
    i32.const 1)
  (func (export "_start") call 0 i32.const 1 i32.ne if unreachable end))
