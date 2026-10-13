(module (type $node (struct (field i64))) (type $a (array (mut (ref null eq))))
          (table $roots 16 (ref null $a))
          (func (export "_start") (local $node (ref null $node)) (local $a (ref null $a)) (local $i i32)
          i64.const 72891 struct.new $node local.set $node i32.const 0 local.set $i
          loop $alloc i32.const 257 array.new_default $a local.set $a local.get $a i32.const 0 local.get $node array.set $a
          local.get $i i32.const 15 i32.and local.get $a table.set $roots
          local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $alloc end
          i32.const 0 local.set $i loop $verify
          local.get $i table.get $roots local.set $a local.get $a i32.const 0 array.get $a local.get $node ref.eq i32.eqz if unreachable end local.get $a i32.const 128 array.get $a ref.is_null i32.eqz if unreachable end local.get $a i32.const 256 array.get $a ref.is_null i32.eqz if unreachable end
          local.get $i i32.const 1 i32.add local.tee $i i32.const 16 i32.lt_u br_if $verify end
          local.get $node struct.get $node 0 i64.const 72891 i64.ne if unreachable end))