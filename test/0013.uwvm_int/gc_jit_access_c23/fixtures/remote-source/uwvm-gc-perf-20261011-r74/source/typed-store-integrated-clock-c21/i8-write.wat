(module (import "wasi_snapshot_preview1" "clock_time_get" (func $clock (param i32 i64 i32) (result i32)))
(import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
(memory (export "memory") 1) (type $A (array (mut i8))) (func $work (local $a (ref null $A)) (local $n i32) (local $sum i32) i32.const 0  i32.const 1024 array.new $A local.set $a (loop $L local.get $a local.get $n i32.const 1023 i32.and local.get $n i32.const 2654435761 i32.mul i32.const 2139095041 i32.xor array.set $A local.get $n i32.const 1 i32.add local.tee $n i32.const 131072 i32.lt_u br_if $L) i32.const 0 local.set $n (loop $V local.get $a local.get $n array.get_u $A local.get $n i32.const 130048 i32.add i32.const 2654435761 i32.mul i32.const 2139095041 i32.xor i32.const 255 i32.and i32.ne if unreachable end local.get $n i32.const 1 i32.add local.tee $n i32.const 1024 i32.lt_u br_if $V)) (func $batch (param $count i32) (loop $B call $work local.get $count i32.const 1 i32.sub local.tee $count br_if $B))
(func (export "_start") (local $samples i32) (local $delta i64) (local $i i32) (local $hex i32)
 i32.const 32 call $batch i32.const 32 call $batch
 (loop $S
  i32.const 1 i64.const 1 i32.const 0 call $clock if unreachable end
  i32.const 32 call $batch
  i32.const 1 i64.const 1 i32.const 8 call $clock if unreachable end
  i32.const 8 i64.load i32.const 0 i64.load i64.sub local.tee $delta i64.eqz if unreachable end
  i32.const 0 local.set $i
  (loop $H
   local.get $delta i32.const 15 local.get $i i32.sub i32.const 4 i32.mul i64.extend_i32_u i64.shr_u i32.wrap_i64 i32.const 15 i32.and local.set $hex
   i32.const 80 local.get $i i32.add local.get $hex local.get $hex i32.const 10 i32.lt_u if (result i32) i32.const 48 else i32.const 87 end i32.add i32.store8
   local.get $i i32.const 1 i32.add local.tee $i i32.const 16 i32.lt_u br_if $H)
  i32.const 96 i32.const 10 i32.store8
  i32.const 64 i32.const 80 i32.store i32.const 68 i32.const 17 i32.store
  i32.const 1 i32.const 64 i32.const 1 i32.const 72 call $write if unreachable end
  i32.const 72 i32.load i32.const 17 i32.ne if unreachable end
  local.get $samples i32.const 1 i32.add local.tee $samples i32.const 5 i32.lt_u br_if $S)))