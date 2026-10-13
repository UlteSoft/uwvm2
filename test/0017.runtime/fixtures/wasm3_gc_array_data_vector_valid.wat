(module
  (type $array (array (mut v128)))
  (data $segment "\01\02\03\04\05\06\07\08\09\0a\0b\0c\0d\0e\0f\10"
                 "\11\12\13\14\15\16\17\18\19\1a\1b\1c\1d\1e\1f\20")
  (func $probe (local $values (ref $array))
    i32.const 0 i32.const 2 array.new_data $array $segment local.set $values
    local.get $values i32.const 0 array.get $array i32x4.extract_lane 0
    i32.const 0x04030201 i32.ne if unreachable end
    local.get $values i32.const 1 array.get $array i32x4.extract_lane 3
    i32.const 0x201f1e1d i32.ne if unreachable end
    local.get $values i32.const 0 i32.const 16 i32.const 1 array.init_data $array $segment
    local.get $values i32.const 0 array.get $array i32x4.extract_lane 0
    i32.const 0x14131211 i32.ne if unreachable end
    local.get $values i32.const 1 array.get $array i32x4.extract_lane 3
    i32.const 0x201f1e1d i32.ne if unreachable end)
  (func (export "_start") call $probe))
