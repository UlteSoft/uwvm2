;; $hot becomes LLVM tier 1 after warm-up, then calls a distinct module's
;; cold interpreter function. The first exception is caught so compile logs
;; survive; the second exits and must show provider/caller/entry in order.
(module
  (import "p" "event" (tag $event))
  (import "p" "cold" (func $cold (param i32) (result i32)))
  (func $hot (param $raise i32) (result i32)
    local.get $raise
    if
      local.get $raise
      call $cold
      return
    end
    i32.const 0)
  (func (export "_start") (local $n i32)
    i32.const 10000
    local.set $n
    loop $warm
      i32.const 0
      call $hot
      drop
      local.get $n
      i32.const 1
      i32.sub
      local.tee $n
      br_if $warm
    end
    block $caught
      try_table (catch $event $caught)
        i32.const 1
        call $hot
        drop
      end
    end
    i32.const 1
    call $hot
    drop))
