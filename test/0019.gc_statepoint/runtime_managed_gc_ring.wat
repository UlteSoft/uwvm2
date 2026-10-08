(module
      (type $s (struct (field i32))) (table (export "roots") 1024 anyref)
      (func (export "_start") (local $state i32) (local $remaining i32) 
        i32.const 123456789 local.set $state
        i32.const 65536 local.set $remaining
        
        loop $again
          local.get $state i32.const 1664525 i32.mul i32.const 1013904223 i32.add local.set $state
          local.get $remaining i32.const 1023 i32.and
          local.get $state struct.new $s table.set 0
          local.get $remaining i32.const 1023 i32.and
          table.get 0 ref.cast (ref $s) struct.get $s 0 local.set $state
          local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $again
        end
        local.get $state i32.const 1863634197 i32.ne if unreachable end))
