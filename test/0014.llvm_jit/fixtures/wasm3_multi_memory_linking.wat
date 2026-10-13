;; Core 3 explicit memory indices, imported aliases, defined-memory index adjustment,
;; SIMD lane memargs and the register-ring load16/store16 fusion boundary.
(module
  (import "a" "mem" (memory $a 1))
  (import "a" "mem" (memory $alias 1))
  (import "b" "mem" (memory $b 1))
  (memory $local 1 4)
  (data (memory $a) (i32.const 0) "\11\22\33\44")
  (data $passive "\aa\bb\cc\dd")
  (func $different (param $dst i32) (param $src i32)
    local.get $dst local.get $src i32.const 1 i32.shl
    i32.load16_u $a
    i32.store16 $b)
  (func $same (param $dst i32) (param $src i32)
    local.get $dst local.get $src i32.const 1 i32.shl
    i32.load16_u $a
    i32.store16 $alias)
  (func (export "_start")
    i32.const 8 i32.const 1 call $different
    i32.const 8 i32.load $b i32.const 0x4433 i32.ne if unreachable end
    i32.const 8 i32.const 1 call $same
    i32.const 8 i32.load $a i32.const 0x4433 i32.ne if unreachable end
    ;; Two imported indices resolve to one object; lock it once and preserve memmove overlap.
    i32.const 1 i32.const 0 i32.const 3 memory.copy $alias $a
    i32.const 0 i32.load $a i32.const 0x33221111 i32.ne if unreachable end
    ;; Distinct native source and destination retain separate bounds and allocations.
    i32.const 0 i32.const 0 i32.const 4 memory.copy $b $alias
    i32.const 0 i32.load $b i32.const 0x33221111 i32.ne if unreachable end
    i32.const 0 i32.const 0 i32.const 4 memory.init $local $passive
    i32.const 0 i32.load $local i32.const 0xddccbbaa i32.ne if unreachable end
    i32.const 0 v128.load $b i32x4.extract_lane 0 i32.const 0x33221111 i32.ne if unreachable end
    i32.const 8 v128.const i32x4 1 2 3 4 v128.store32_lane $local 2
    i32.const 8 i32.load $local i32.const 3 i32.ne if unreachable end
    i32.const 1 memory.grow $alias i32.const 1 i32.ne if unreachable end
    memory.size $a i32.const 2 i32.ne if unreachable end
    memory.size $b i32.const 1 i32.ne if unreachable end
    memory.size $local i32.const 1 i32.ne if unreachable end
  )
)
