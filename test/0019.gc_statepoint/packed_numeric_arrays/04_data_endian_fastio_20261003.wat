(module
  (rec
    (type $byte (array (mut i8)))
    (type $half (array (mut i16)))
    (type $word (array (mut i32)))
    (type $long (array (mut i64)))
    (type $float (array (mut f32)))
    (type $double (array (mut f64)))
    (type $vector (array (mut v128))))
  (data $byte_wire "\a5\80\00\5a")
  (data $half_wire "\a5\f1\80\00\00\5a")
  (data $word_wire "\a5\21\43\65\87\00\00\00\00\5a")
  (data $long_wire "\a5\67\45\23\01\21\43\65\87\00\00\00\00\00\00\00\00\5a")
  (data $float_wire "\a5\23\01\80\7f\00\00\00\00\5a")
  (data $double_wire "\a5\23\01\00\00\00\00\f0\7f\00\00\00\00\00\00\00\00\5a")
  (data $vector_wire "\a5\00\11\22\33\44\55\66\77\88\99\aa\bb\cc\dd\ee\ff\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\5a")
  (func $verify_vector (param $value v128)
    local.get $value i8x16.extract_lane_u 0 i32.const 0 i32.ne if unreachable end
    local.get $value i8x16.extract_lane_u 15 i32.const 255 i32.ne if unreachable end
    local.get $value i16x8.extract_lane_u 0 i32.const 0x1100 i32.ne if unreachable end
    local.get $value i16x8.extract_lane_u 7 i32.const 0xffee i32.ne if unreachable end
    local.get $value i32x4.extract_lane 0 i32.const 0x33221100 i32.ne if unreachable end
    local.get $value i32x4.extract_lane 3 i32.const 0xffeeddcc i32.ne if unreachable end
    local.get $value i64x2.extract_lane 0 i64.const 0x7766554433221100 i64.ne if unreachable end
    local.get $value i64x2.extract_lane 1 i64.const 0xffeeddccbbaa9988 i64.ne if unreachable end)
  (func $run (export "run") (result i32)
    (local $byte_array (ref null $byte))
    (local $half_array (ref null $half))
    (local $word_array (ref null $word))
    (local $long_array (ref null $long))
    (local $float_array (ref null $float))
    (local $double_array (ref null $double))
    (local $vector_array (ref null $vector))
    i32.const 1 i32.const 2 array.new_data $byte $byte_wire local.set $byte_array
    local.get $byte_array i32.const 0 array.get_u $byte
    i32.const 0x80 i32.ne if unreachable end
    local.get $byte_array i32.const 1 array.get_u $byte
    i32.const 0 i32.ne if unreachable end
    local.get $byte_array i32.const 0 array.get_s $byte i32.const -128 i32.ne if unreachable end
    local.get $byte_array i32.const 1 i32.const 1 i32.const 1 array.init_data $byte $byte_wire
    local.get $byte_array i32.const 1 array.get_u $byte
    i32.const 0x80 i32.ne if unreachable end
    data.drop $byte_wire
    local.get $byte_array i32.const 0 array.get_u $byte
    i32.const 0x80 i32.ne if unreachable end
    local.get $byte_array i32.const 1 array.get_u $byte
    i32.const 0x80 i32.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $half $half_wire local.set $half_array
    local.get $half_array i32.const 0 array.get_u $half
    i32.const 0x80f1 i32.ne if unreachable end
    local.get $half_array i32.const 1 array.get_u $half
    i32.const 0 i32.ne if unreachable end
    local.get $half_array i32.const 0 array.get_s $half i32.const -32527 i32.ne if unreachable end
    local.get $half_array i32.const 1 i32.const 1 i32.const 1 array.init_data $half $half_wire
    local.get $half_array i32.const 1 array.get_u $half
    i32.const 0x80f1 i32.ne if unreachable end
    data.drop $half_wire
    local.get $half_array i32.const 0 array.get_u $half
    i32.const 0x80f1 i32.ne if unreachable end
    local.get $half_array i32.const 1 array.get_u $half
    i32.const 0x80f1 i32.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $word $word_wire local.set $word_array
    local.get $word_array i32.const 0 array.get $word
    i32.const 0x87654321 i32.ne if unreachable end
    local.get $word_array i32.const 1 array.get $word
    i32.const 0 i32.ne if unreachable end
    local.get $word_array i32.const 1 i32.const 1 i32.const 1 array.init_data $word $word_wire
    local.get $word_array i32.const 1 array.get $word
    i32.const 0x87654321 i32.ne if unreachable end
    data.drop $word_wire
    local.get $word_array i32.const 0 array.get $word
    i32.const 0x87654321 i32.ne if unreachable end
    local.get $word_array i32.const 1 array.get $word
    i32.const 0x87654321 i32.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $long $long_wire local.set $long_array
    local.get $long_array i32.const 0 array.get $long
    i64.const 0x8765432101234567 i64.ne if unreachable end
    local.get $long_array i32.const 1 array.get $long
    i64.const 0 i64.ne if unreachable end
    local.get $long_array i32.const 1 i32.const 1 i32.const 1 array.init_data $long $long_wire
    local.get $long_array i32.const 1 array.get $long
    i64.const 0x8765432101234567 i64.ne if unreachable end
    data.drop $long_wire
    local.get $long_array i32.const 0 array.get $long
    i64.const 0x8765432101234567 i64.ne if unreachable end
    local.get $long_array i32.const 1 array.get $long
    i64.const 0x8765432101234567 i64.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $float $float_wire local.set $float_array
    local.get $float_array i32.const 0 array.get $float
    i32.reinterpret_f32
    i32.const 0x7f800123 i32.ne if unreachable end
    local.get $float_array i32.const 1 array.get $float
    i32.reinterpret_f32
    i32.const 0 i32.ne if unreachable end
    local.get $float_array i32.const 1 i32.const 1 i32.const 1 array.init_data $float $float_wire
    local.get $float_array i32.const 1 array.get $float
    i32.reinterpret_f32
    i32.const 0x7f800123 i32.ne if unreachable end
    data.drop $float_wire
    local.get $float_array i32.const 0 array.get $float
    i32.reinterpret_f32
    i32.const 0x7f800123 i32.ne if unreachable end
    local.get $float_array i32.const 1 array.get $float
    i32.reinterpret_f32
    i32.const 0x7f800123 i32.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $double $double_wire local.set $double_array
    local.get $double_array i32.const 0 array.get $double
    i64.reinterpret_f64
    i64.const 0x7ff0000000000123 i64.ne if unreachable end
    local.get $double_array i32.const 1 array.get $double
    i64.reinterpret_f64
    i64.const 0 i64.ne if unreachable end
    local.get $double_array i32.const 1 i32.const 1 i32.const 1 array.init_data $double $double_wire
    local.get $double_array i32.const 1 array.get $double
    i64.reinterpret_f64
    i64.const 0x7ff0000000000123 i64.ne if unreachable end
    data.drop $double_wire
    local.get $double_array i32.const 0 array.get $double
    i64.reinterpret_f64
    i64.const 0x7ff0000000000123 i64.ne if unreachable end
    local.get $double_array i32.const 1 array.get $double
    i64.reinterpret_f64
    i64.const 0x7ff0000000000123 i64.ne if unreachable end
    i32.const 1 i32.const 2 array.new_data $vector $vector_wire local.set $vector_array
    local.get $vector_array i32.const 0 array.get $vector
    call $verify_vector
    local.get $vector_array i32.const 1 array.get $vector
    v128.any_true if unreachable end
    local.get $vector_array i32.const 1 i32.const 1 i32.const 1 array.init_data $vector $vector_wire
    local.get $vector_array i32.const 1 array.get $vector
    call $verify_vector
    data.drop $vector_wire
    local.get $vector_array i32.const 0 array.get $vector
    call $verify_vector
    local.get $vector_array i32.const 1 array.get $vector
    call $verify_vector
    i32.const 0x6f3a12b5)
  (func (export "_start") call $run i32.const 0x6f3a12b5 i32.ne if unreachable end))
