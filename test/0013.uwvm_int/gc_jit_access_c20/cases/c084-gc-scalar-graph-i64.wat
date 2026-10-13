(module
(type $base (sub (struct (field (mut i64)))))
(type $child (sub $base (struct (field (mut i64)))))
(global $seed (mut i32) (i32.const 17))
(func (export "_start")
(local $r0 (ref null $base))
(local $r1 (ref null $base))
(local $r2 (ref null $base))
(local $r3 (ref null $base))
(local $r4 (ref null $base))
(local $r5 (ref null $base))
(local $r6 (ref null $base))
(local $r7 (ref null $base))
(local $r8 (ref null $base))
(local $r9 (ref null $base))
(local $r10 (ref null $base))
(local $r11 (ref null $base))
(local $r12 (ref null $base))
(local $r13 (ref null $base))
(local $r14 (ref null $base))
(local $r15 (ref null $base))
(local $r16 (ref null $base))
(local $r17 (ref null $base))
(local $r18 (ref null $base))
(local $r19 (ref null $base))
(local $r20 (ref null $base))
(local $r21 (ref null $base))
(local $r22 (ref null $base))
(local $r23 (ref null $base))
(local $r24 (ref null $base))
(local $r25 (ref null $base))
(local $r26 (ref null $base))
(local $r27 (ref null $base))
(local $r28 (ref null $base))
(local $r29 (ref null $base))
(local $r30 (ref null $base))
(local $r31 (ref null $base))
(local $seed i32) (local $batch i32) global.get $seed local.set $seed global.get $seed i32.const 1 i32.add global.set $seed loop $again
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 0 i32.add i64.extend_i32_u struct.new $child local.set $r0
local.get $r0 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 0 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 1 i32.add i64.extend_i32_u struct.new $child local.set $r1
local.get $r1 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 1 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 2 i32.add i64.extend_i32_u struct.new $child local.set $r2
local.get $r2 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 2 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 3 i32.add i64.extend_i32_u struct.new $child local.set $r3
local.get $r3 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 3 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 4 i32.add i64.extend_i32_u struct.new $child local.set $r4
local.get $r4 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 4 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 5 i32.add i64.extend_i32_u struct.new $child local.set $r5
local.get $r5 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 5 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 6 i32.add i64.extend_i32_u struct.new $child local.set $r6
local.get $r6 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 6 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 7 i32.add i64.extend_i32_u struct.new $child local.set $r7
local.get $r7 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 7 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 8 i32.add i64.extend_i32_u struct.new $child local.set $r8
local.get $r8 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 8 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 9 i32.add i64.extend_i32_u struct.new $child local.set $r9
local.get $r9 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 9 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 10 i32.add i64.extend_i32_u struct.new $child local.set $r10
local.get $r10 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 10 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 11 i32.add i64.extend_i32_u struct.new $child local.set $r11
local.get $r11 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 11 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 12 i32.add i64.extend_i32_u struct.new $child local.set $r12
local.get $r12 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 12 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 13 i32.add i64.extend_i32_u struct.new $child local.set $r13
local.get $r13 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 13 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 14 i32.add i64.extend_i32_u struct.new $child local.set $r14
local.get $r14 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 14 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 15 i32.add i64.extend_i32_u struct.new $child local.set $r15
local.get $r15 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 15 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 16 i32.add i64.extend_i32_u struct.new $child local.set $r16
local.get $r16 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 16 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 17 i32.add i64.extend_i32_u struct.new $child local.set $r17
local.get $r17 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 17 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 18 i32.add i64.extend_i32_u struct.new $child local.set $r18
local.get $r18 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 18 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 19 i32.add i64.extend_i32_u struct.new $child local.set $r19
local.get $r19 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 19 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 20 i32.add i64.extend_i32_u struct.new $child local.set $r20
local.get $r20 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 20 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 21 i32.add i64.extend_i32_u struct.new $child local.set $r21
local.get $r21 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 21 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 22 i32.add i64.extend_i32_u struct.new $child local.set $r22
local.get $r22 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 22 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 23 i32.add i64.extend_i32_u struct.new $child local.set $r23
local.get $r23 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 23 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 24 i32.add i64.extend_i32_u struct.new $child local.set $r24
local.get $r24 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 24 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 25 i32.add i64.extend_i32_u struct.new $child local.set $r25
local.get $r25 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 25 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 26 i32.add i64.extend_i32_u struct.new $child local.set $r26
local.get $r26 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 26 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 27 i32.add i64.extend_i32_u struct.new $child local.set $r27
local.get $r27 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 27 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 28 i32.add i64.extend_i32_u struct.new $child local.set $r28
local.get $r28 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 28 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 29 i32.add i64.extend_i32_u struct.new $child local.set $r29
local.get $r29 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 29 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 30 i32.add i64.extend_i32_u struct.new $child local.set $r30
local.get $r30 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 30 i32.add i32.ne if unreachable end
local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 31 i32.add i64.extend_i32_u struct.new $child local.set $r31
local.get $r31 struct.get $base 0 i32.wrap_i64 local.get $seed local.get $batch i32.const 32 i32.mul i32.add i32.const 31 i32.add i32.ne if unreachable end
local.get $r0 struct.get $base 0 drop
local.get $batch i32.const 1 i32.add local.tee $batch i32.const 256 i32.lt_u br_if $again end
))
