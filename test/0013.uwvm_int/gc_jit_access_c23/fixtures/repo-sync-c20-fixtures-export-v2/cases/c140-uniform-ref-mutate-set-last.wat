(module (type $node (struct (field (mut i32))))
 (type $payload (array (mut (ref null $node))))
 (func (export "_start") (local $a (ref null $payload)) (local $src (ref null $payload))
  (local $seed (ref null $node)) (local $child (ref null $node)) (local $j i32)
  i32.const 7 struct.new $node local.tee $seed i32.const 4096 array.new $payload local.set $a
  i32.const 43 struct.new $node local.set $child
  local.get $a i32.const 4095 local.get $child array.set $payload
  ref.null $node local.set $seed ref.null $node local.set $child ref.null $payload local.set $src
  loop $garbage i32.const 17 struct.new $node drop
   local.get $j i32.const 1 i32.add local.tee $j i32.const 8192 i32.lt_u br_if $garbage end
  local.get $a i32.const 4095 array.get $payload ref.as_non_null struct.get $node 0 i32.const 43 i32.ne if unreachable end
  local.get $a i32.const 0 array.get $payload ref.as_non_null struct.get $node 0 i32.const 7 i32.ne if unreachable end
  local.get $a i32.const 4095 array.get $payload ref.as_non_null struct.get $node 0 i32.const 43 i32.ne if unreachable end))