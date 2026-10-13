;; Real CLI stop/commit/path invalidation with Core3 GC, multi-memory, memory64 and threads.
(module
  (type $node (struct (field i32)))
  (memory $plain 1 1)
  (memory $local 1 1)
  (memory $shared 1 1 shared)
  (memory $wide i64 2 2)
  (global $held (mut (ref null $node)) (ref.null $node))
  (func (export "_start") (local $node (ref $node)) (local $i i32)
    i32.const 7
    struct.new $node
    local.set $node
    local.get $node
    global.set $held
    i32.const 63
    i32.const 17
    i32.store8 $plain
    i32.const 64
    i32.const 90
    i32.const 4
    memory.fill $plain
    i32.const 68
    i32.const 34
    i32.store8 $plain
    i32.const 65535
    i32.const 125
    i32.store8 $plain
    i32.const 63
    i32.const 17
    i32.store8 $local
    i32.const 64
    i32.const 90
    i32.const 4
    memory.fill $local
    i32.const 68
    i32.const 34
    i32.store8 $local
    i32.const 65535
    i32.const 125
    i32.store8 $local
    i32.const 63
    i32.const 17
    i32.store8 $shared
    i32.const 64
    i32.const 90
    i32.const 4
    memory.fill $shared
    i32.const 68
    i32.const 34
    i32.store8 $shared
    i32.const 65535
    i32.const 125
    i32.store8 $shared
    i64.const 63
    i32.const 17
    i32.store8 $wide
    i64.const 64
    i32.const 90
    i64.const 4
    memory.fill $wide
    i64.const 68
    i32.const 34
    i32.store8 $wide
    i64.const 131071
    i32.const 125
    i32.store8 $wide
    i32.const 255
    i32.const 17
    i32.store8 $plain
    i32.const 256
    i32.const 90
    i32.const 256
    memory.fill $plain
    i32.const 512
    i32.const 34
    i32.store8 $plain
    ;; Exactly67 setup opcodes;68 real steps pause on the second of128 nops.
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    i32.const 63
    i32.load8_u $plain
    i32.const 17
    i32.ne if unreachable end
    i32.const 64
    i32.load8_u $plain
    i32.const 1
    i32.ne if unreachable end
    i32.const 65
    i32.load8_u $plain
    i32.const 2
    i32.ne if unreachable end
    i32.const 66
    i32.load8_u $plain
    i32.const 3
    i32.ne if unreachable end
    i32.const 67
    i32.load8_u $plain
    i32.const 4
    i32.ne if unreachable end
    i32.const 68
    i32.load8_u $plain
    i32.const 34
    i32.ne if unreachable end
    i32.const 65535
    i32.load8_u $plain
    i32.const 125
    i32.ne if unreachable end
    i32.const 63
    i32.load8_u $local
    i32.const 17
    i32.ne if unreachable end
    i32.const 64
    i32.load8_u $local
    i32.const 1
    i32.ne if unreachable end
    i32.const 65
    i32.load8_u $local
    i32.const 2
    i32.ne if unreachable end
    i32.const 66
    i32.load8_u $local
    i32.const 3
    i32.ne if unreachable end
    i32.const 67
    i32.load8_u $local
    i32.const 4
    i32.ne if unreachable end
    i32.const 68
    i32.load8_u $local
    i32.const 34
    i32.ne if unreachable end
    i32.const 65535
    i32.load8_u $local
    i32.const 125
    i32.ne if unreachable end
    i32.const 63
    i32.atomic.load8_u $shared
    i32.const 17
    i32.ne if unreachable end
    i32.const 64
    i32.atomic.load8_u $shared
    i32.const 1
    i32.ne if unreachable end
    i32.const 65
    i32.atomic.load8_u $shared
    i32.const 2
    i32.ne if unreachable end
    i32.const 66
    i32.atomic.load8_u $shared
    i32.const 3
    i32.ne if unreachable end
    i32.const 67
    i32.atomic.load8_u $shared
    i32.const 4
    i32.ne if unreachable end
    i32.const 68
    i32.atomic.load8_u $shared
    i32.const 34
    i32.ne if unreachable end
    i32.const 65535
    i32.atomic.load8_u $shared
    i32.const 125
    i32.ne if unreachable end
    i64.const 63
    i32.load8_u $wide
    i32.const 17
    i32.ne if unreachable end
    i64.const 64
    i32.load8_u $wide
    i32.const 1
    i32.ne if unreachable end
    i64.const 65
    i32.load8_u $wide
    i32.const 2
    i32.ne if unreachable end
    i64.const 66
    i32.load8_u $wide
    i32.const 3
    i32.ne if unreachable end
    i64.const 67
    i32.load8_u $wide
    i32.const 4
    i32.ne if unreachable end
    i64.const 68
    i32.load8_u $wide
    i32.const 34
    i32.ne if unreachable end
    i64.const 131071
    i32.load8_u $wide
    i32.const 125
    i32.ne if unreachable end
    i32.const 255
    i32.load8_u $plain
    i32.const 17
    i32.ne if unreachable end
    i32.const 512
    i32.load8_u $plain
    i32.const 34
    i32.ne if unreachable end
    i32.const 0 local.set $i
    block $done loop $check
      i32.const 256 local.get $i i32.add i32.load8_u $plain
      local.get $i i32.ne if unreachable end
      local.get $i i32.const 1 i32.add local.tee $i
      i32.const 256 i32.lt_u br_if $check
    end end
    global.get $held ref.as_non_null struct.get $node 0
    i32.const 7 i32.ne if unreachable end
  ))
