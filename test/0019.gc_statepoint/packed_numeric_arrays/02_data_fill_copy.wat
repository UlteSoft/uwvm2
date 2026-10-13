(module
  (rec
    (type $bytes (array (mut i8)))
    (type $halves (array (mut i16)))
    (type $readonly (array i8))
    (type $vectors (array (mut v128))))
  (data $halves_data "\f1\80\02\01\04\03\06\05")
  (data $vector_data "\00\11\22\33\44\55\66\77\88\99\aa\bb\cc\dd\ee\ff")
  (func (export "run") (result i32)
    (local $h (ref null $halves)) (local $b (ref null $bytes))
    (local $r (ref null $readonly)) (local $v (ref null $vectors))
    i32.const 0 i32.const 4 array.new_data $halves $halves_data local.set $h
    local.get $h i32.const 0 array.get_s $halves i32.const 0xffff80f1 i32.ne if unreachable end
    local.get $h i32.const 3 array.get_u $halves i32.const 0x0506 i32.ne if unreachable end
    local.get $h i32.const 1 i32.const 0 i32.const 1 array.init_data $halves $halves_data
    local.get $h i32.const 1 array.get_u $halves i32.const 0x80f1 i32.ne if unreachable end
    local.get $h i32.const 2 i32.const 0x1234ffff i32.const 2 array.fill $halves
    local.get $h i32.const 2 array.get_s $halves i32.const -1 i32.ne if unreachable end
    i32.const 1 i32.const 2 i32.const 3 i32.const 4 i32.const 5
    array.new_fixed $bytes 5 local.set $b
    local.get $b i32.const 1 local.get $b i32.const 0 i32.const 4 array.copy $bytes $bytes
    local.get $b i32.const 1 array.get_u $bytes i32.const 1 i32.ne if unreachable end
    local.get $b i32.const 4 array.get_u $bytes i32.const 4 i32.ne if unreachable end
    i32.const 1 i32.const 2 i32.const 3 i32.const 4 i32.const 5
    array.new_fixed $bytes 5 local.set $b
    local.get $b i32.const 0 local.get $b i32.const 1 i32.const 4 array.copy $bytes $bytes
    local.get $b i32.const 0 array.get_u $bytes i32.const 2 i32.ne if unreachable end
    local.get $b i32.const 3 array.get_u $bytes i32.const 5 i32.ne if unreachable end
    i32.const 7 i32.const 8 array.new_fixed $readonly 2 local.set $r
    local.get $b i32.const 0 local.get $r i32.const 0 i32.const 2 array.copy $bytes $readonly
    local.get $b i32.const 1 array.get_u $bytes i32.const 8 i32.ne if unreachable end
    i32.const 0 i32.const 1 array.new_data $vectors $vector_data local.set $v
    local.get $v i32.const 0 array.get $vectors i32x4.extract_lane 0
    i32.const 0x33221100 i32.ne if unreachable end
    data.drop $vector_data
    local.get $v i32.const 0 array.get $vectors i32x4.extract_lane 3
    i32.const 0xffeeddcc i32.ne if unreachable end
    i32.const 0 array.new_default $bytes local.set $b
    local.get $b i32.const 0 i32.const 9 i32.const 0 array.fill $bytes
    local.get $b i32.const 0 local.get $b i32.const 0 i32.const 0 array.copy $bytes $bytes
    local.get $b array.len i32.eqz if else unreachable end
    i32.const 1)
  (func (export "_start") call 0 i32.const 1 i32.ne if unreachable end))
