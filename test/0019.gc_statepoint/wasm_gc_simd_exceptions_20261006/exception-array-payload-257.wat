(module (type $node (struct (field i64))) (type $a (array (mut (ref null exn))))
      (tag $tag (param (ref $node))) (global $root (mut (ref null $a)) (ref.null $a))
      (func (export "_start") (local $a (ref null $a)) (local $e (ref null exn)) (local $i i32)
      (block $new (result (ref exn)) (try_table (catch_all_ref $new)
        i64.const 91472 struct.new $node throw $tag) unreachable) local.set $e
      i32.const 257 array.new_default $a local.set $a
      local.get $a i32.const 256 local.get $e array.set $a local.get $a global.set $root
      ref.null exn local.set $e ref.null $a local.set $a i32.const 0 local.set $i
      loop $churn i32.const 257 array.new_default $a drop
        local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn end
      (block $caught (result (ref $node)) (try_table (catch $tag $caught)
        global.get $root i32.const 256 array.get $a ref.as_non_null throw_ref) unreachable)
      struct.get $node 0 i64.const 91472 i64.ne if unreachable end
      ref.null $a global.set $root))