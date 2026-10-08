(module
      (rec (type $node (struct (field i64) (field (mut (ref null $a)))))
           (type $a (array (mut (ref null $node)))))
      (global $root (mut (ref null $a)) (ref.null $a))
      (func (export "_start") (local $a (ref null $a)) (local $node (ref null $node)) (local $i i32)
       i32.const 257 array.new_default $a local.set $a
       local.get $a i32.const 256 array.get $a ref.is_null i32.eqz if unreachable end
       i64.const 816927 ref.null $a struct.new $node local.set $node
       local.get $node local.get $a struct.set $node 1
       local.get $a i32.const 256 local.get $node array.set $a local.get $a global.set $root
       ref.null $a local.set $a ref.null $node local.set $node
       loop $churn i32.const 257 array.new_default $a drop
        local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn end
       global.get $root i32.const 256 array.get $a local.tee $node
       struct.get $node 0 i64.const 816927 i64.ne if unreachable end
       local.get $node struct.get $node 1 global.get $root ref.eq i32.eqz if unreachable end
       ref.null $a global.set $root))