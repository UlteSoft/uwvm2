(module (type $churn (struct (field i32))) (type $a (array (mut i64))) 
      (func $f) (elem declare func $f) (table $roots 16 (ref null $a))
      (func (export "_start") (local $a (ref null $a)) (local $i i32) (local $j i32) (local $e (ref null exn))
       i32.const 0 local.set $i loop $allocate
      i64.const -1 i32.const 16 array.new $a drop
      i32.const 16 array.new_default $a local.set $a
      local.get $a i32.const 0 array.get $a i64.eqz i32.eqz if unreachable end local.get $a i32.const 15 array.get $a i64.eqz i32.eqz if unreachable end
      local.get $i i32.const 15 i32.and local.get $a table.set $roots
      local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $allocate end
      i32.const 0 local.set $i loop $churn
      i32.const 71 struct.new $churn drop
      local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn end
      i32.const 0 local.set $i loop $verify_roots
      local.get $i table.get $roots local.set $a local.get $a i32.const 0 array.get $a i64.eqz i32.eqz if unreachable end local.get $a i32.const 15 array.get $a i64.eqz i32.eqz if unreachable end
      local.get $i i32.const 1 i32.add local.tee $i i32.const 16 i32.lt_u br_if $verify_roots end))