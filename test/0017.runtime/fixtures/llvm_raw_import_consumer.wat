(module
  (import "P" "answer" (func $answer (param i32) (result i32)))
  (global $started (mut i32) (i32.const 0))
  (func $initialize
    i32.const 41 call $answer i32.const 42 i32.ne if unreachable end
    i32.const 1 global.set $started)
  (start $initialize)
  (func (export "_start")
    global.get $started i32.const 1 i32.ne if unreachable end
    i32.const 98 call $answer i32.const 99 i32.ne if unreachable end))
