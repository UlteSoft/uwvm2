(module
   (type $child (struct (field (mut i32))))
   
   (type $a (array (mut (ref null func))))
   (func (export "_start")
    (local $a (ref null $a)) (local $c (ref null $child))
    (local $i i32) (local $j i32) (local $k i32)
    
    loop $round
    ref.null func i32.const 0 array.new $a local.set $a
local.get $a array.len i32.const 0 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
ref.null func i32.const 1 array.new $a local.set $a
local.get $a array.len i32.const 1 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
ref.null func i32.const 3 array.new $a local.set $a
local.get $a array.len i32.const 3 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
ref.null func i32.const 16 array.new $a local.set $a
local.get $a array.len i32.const 16 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
ref.null func i32.const 257 array.new $a local.set $a
local.get $a array.len i32.const 257 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
ref.null func i32.const 4096 array.new $a local.set $a
local.get $a array.len i32.const 4096 i32.ne if unreachable end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
     local.get $i i32.const 1 i32.add local.tee $i i32.const 128 i32.lt_u br_if $round
    end
    ref.null $child local.set $c
    i32.const 0 local.set $j loop $churn
    i32.const 43 struct.new $child drop
    local.get $j i32.const 1 i32.add local.tee $j i32.const 8192 i32.lt_u br_if $churn
   end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
i32.const 0 local.set $j loop $churn
    i32.const 43 struct.new $child drop
    local.get $j i32.const 1 i32.add local.tee $j i32.const 8192 i32.lt_u br_if $churn
   end
i32.const 0 local.set $k
   block $done loop $check
    local.get $k local.get $a array.len i32.ge_u br_if $done
    local.get $a local.get $k array.get $a ref.is_null i32.eqz if unreachable end
    local.get $k i32.const 1 i32.add local.set $k br $check
   end end
   ))