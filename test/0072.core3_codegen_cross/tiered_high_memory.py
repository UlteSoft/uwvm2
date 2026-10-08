"""Six dynamic high-address checks per hot call; low aliases stay observable."""


def fixture():
    return '''(module
      (memory $wide (export "wide") i64 65537 65537 shared)
      (global $base (mut i64) (i64.const 0))
      (global $value (mut i64) (i64.const 0x80706050403020f1))
      (global $fp (mut i64) (i64.const 0x8000000000000000))
      (global $vector (mut v128) (v128.const i32x4 1 0x80000000 0x7fc12345 0x76543210))
      (func $setup
        global.get $base i64.eqz if
          i64.const 0 i32.const 165 i64.const 128 memory.fill $wide
          i64.const 4294967296 global.set $base
        else
          global.get $base i64.atomic.load $wide offset=64 i64.const 4000000 i64.ne if unreachable end
          i64.const 0x40 i64.load $wide i64.const 0xa5a5a5a5a5a5a5a5 i64.ne if unreachable end
          i64.const 9 i64.load $wide align=1 i64.const 0xa5a5a5a5a5a5a5a5 i64.ne if unreachable end
          i64.const 33 i64.load $wide align=1 i64.const 0xa5a5a5a5a5a5a5a5 i64.ne if unreachable end
          i64.const 41 i64.load $wide align=1 i64.const 0xa5a5a5a5a5a5a5a5 i64.ne if unreachable end
        end)
      (func $hot (result i32) (local $v v128)
        global.get $base global.get $value i64.store $wide
        global.get $base i64.load $wide global.get $value i64.ne if unreachable end
        i64.const 0 i64.load $wide i64.const 0xa5a5a5a5a5a5a5a5 i64.ne if unreachable end
        global.get $base global.get $fp f64.reinterpret_i64 f64.store $wide offset=9 align=1
        global.get $base f64.load $wide offset=9 align=1 i64.reinterpret_f64
        global.get $fp i64.ne if unreachable end
        global.get $base global.get $vector v128.store $wide offset=33 align=1
        global.get $base v128.load $wide offset=33 align=1 local.set $v
        local.get $v i64x2.extract_lane 0 global.get $vector i64x2.extract_lane 0 i64.ne if unreachable end
        local.get $v i64x2.extract_lane 1 global.get $vector i64x2.extract_lane 1 i64.ne if unreachable end
        global.get $base i64.const 1 i64.atomic.rmw.add $wide offset=64
        global.get $base i64.atomic.load $wide offset=64 i64.const 1 i64.sub i64.ne if unreachable end
        global.get $value i64.const 1664525 i64.mul i64.const 1013904223 i64.add global.set $value
        global.get $fp i64.const 0x8000000000000000 i64.xor global.set $fp
        global.get $vector v128.const i32x4 1 2 3 4 i32x4.add global.set $vector
        i32.const 42)
      (func (export "_start") (local $n i32)
        call $setup
        i32.const 4000000 local.set $n
        loop $warm call $hot i32.const 42 i32.ne if unreachable end
          local.get $n i32.const 1 i32.sub local.tee $n br_if $warm end
        call $setup))'''
