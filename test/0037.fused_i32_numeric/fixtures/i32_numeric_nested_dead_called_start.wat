;; New test-only successor: real _start plus a called lazy child.
;; Both functions contain actual logical block/loop frames below an unconditional
;; branch. The numeric code must validate but must not execute or emit live IR.
(module
  (type $box (struct (field i32)))
  (func $nested (param i32) (result i32)
    block $value (result i32)
      local.get 0
      br $value
      block
        loop
          i32.const 35 i32.clz drop
          i32.const 35 i32.ctz drop
          i32.const 35 i32.popcnt drop
          i32.const 35 i32.const 2 i32.add drop
          i32.const 35 i32.const 2 i32.sub drop
          i32.const 35 i32.const 2 i32.mul drop
          i32.const 35 i32.const 2 i32.div_s drop
          i32.const 35 i32.const 2 i32.div_u drop
          i32.const 35 i32.const 2 i32.rem_s drop
          i32.const 35 i32.const 2 i32.rem_u drop
          i32.const 35 i32.const 2 i32.and drop
          i32.const 35 i32.const 2 i32.or drop
          i32.const 35 i32.const 2 i32.xor drop
          i32.const 35 i32.const 2 i32.shl drop
          i32.const 35 i32.const 2 i32.shr_s drop
          i32.const 35 i32.const 2 i32.shr_u drop
          i32.const 35 i32.const 2 i32.rotl drop
          i32.const 35 i32.const 2 i32.rotr drop
          unreachable
        end
      end
    end)
  (func (export "_start") (local $box (ref $box))
    block $skip
      br $skip
      block
        loop
          i32.const 35 i32.clz drop
          i32.const 35 i32.ctz drop
          i32.const 35 i32.popcnt drop
          i32.const 35 i32.const 2 i32.add drop
          i32.const 35 i32.const 2 i32.sub drop
          i32.const 35 i32.const 2 i32.mul drop
          i32.const 35 i32.const 2 i32.div_s drop
          i32.const 35 i32.const 2 i32.div_u drop
          i32.const 35 i32.const 2 i32.rem_s drop
          i32.const 35 i32.const 2 i32.rem_u drop
          i32.const 35 i32.const 2 i32.and drop
          i32.const 35 i32.const 2 i32.or drop
          i32.const 35 i32.const 2 i32.xor drop
          i32.const 35 i32.const 2 i32.shl drop
          i32.const 35 i32.const 2 i32.shr_s drop
          i32.const 35 i32.const 2 i32.shr_u drop
          i32.const 35 i32.const 2 i32.rotl drop
          i32.const 35 i32.const 2 i32.rotr drop
          unreachable
        end
      end
    end
    ;; The nondefaultable GC local is genuinely initialized and read.
    i32.const 42 struct.new $box local.set $box
    local.get $box struct.get $box 0 call $nested
    i32.const 42 i32.ne
    if unreachable end))
