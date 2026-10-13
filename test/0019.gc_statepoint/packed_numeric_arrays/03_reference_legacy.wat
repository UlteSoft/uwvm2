(module
  (rec
    (type $node (struct (field (mut (ref null $node))) (field i32)))
    (type $refs (array (mut (ref null $node))))
    (type $nonnull (array (mut (ref $node))))
    (type $readonly (array (ref null $node))))
  (func (export "run") (result i32)
    (local $n (ref null $node)) (local $a (ref null $refs))
    (local $b (ref null $nonnull)) (local $r (ref null $readonly))
    ref.null $node i32.const 37 struct.new $node local.set $n
    local.get $n local.get $n struct.set $node 0
    local.get $n i32.const 3 array.new $refs local.set $a
    local.get $a i32.const 0 array.get $refs local.get $n ref.eq
    i32.eqz if unreachable end
    local.get $a i32.const 2 ref.null $node array.set $refs
    local.get $a i32.const 2 array.get $refs ref.is_null
    i32.eqz if unreachable end
    local.get $n ref.as_non_null i32.const 1 array.new $nonnull local.set $b
    local.get $b i32.const 0 array.get $nonnull struct.get $node 1
    i32.const 37 i32.ne if unreachable end
    local.get $n local.get $n array.new_fixed $readonly 2 local.set $r
    local.get $a i32.const 1 ref.null $node array.set $refs
    local.get $a i32.const 1 array.get $refs ref.is_null
    i32.eqz if unreachable end
    local.get $a i32.const 0 local.get $r i32.const 0 i32.const 2 array.copy $refs $readonly
    local.get $a i32.const 1 array.get $refs local.get $n ref.eq
    i32.eqz if unreachable end
    local.get $a i32.const 0 array.get $refs ref.as_non_null
    struct.get $node 0 local.get $n ref.eq i32.eqz if unreachable end
    i32.const 0 array.new_default $refs array.len i32.eqz if else unreachable end
    i32.const 1)
  (func (export "_start") call 0 i32.const 1 i32.ne if unreachable end))
