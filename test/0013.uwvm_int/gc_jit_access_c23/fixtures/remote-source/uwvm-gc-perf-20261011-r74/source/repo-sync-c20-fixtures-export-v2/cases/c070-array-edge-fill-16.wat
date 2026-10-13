(module (type $n (struct (field (mut i32)))) (type $a (array (mut (ref null $n)))) (type $trash (struct (field i64)))
(func (export "_start") (local $a (ref null $a)) (local $src (ref null $a)) (local $n (ref null $n)) (local $i i32)
(local $unused0 (ref null $n))
(local $unused1 (ref null $n))
(local $unused2 (ref null $n))
(local $unused3 (ref null $n))
(local $unused4 (ref null $n))
(local $unused5 (ref null $n))
(local $unused6 (ref null $n))
(local $unused7 (ref null $n))
(local $unused8 (ref null $n))
(local $unused9 (ref null $n))
(local $unused10 (ref null $n))
(local $unused11 (ref null $n))
(local $unused12 (ref null $n))
(local $unused13 (ref null $n))
(local $unused14 (ref null $n))
(local $unused15 (ref null $n))
i32.const 16 array.new_default $a local.set $a
i32.const 71 struct.new $n local.set $n
local.get $a i32.const 8 local.get $n i32.const 1 array.fill $a
ref.null $n local.set $n ref.null $a local.set $src
i32.const 0 local.set $i loop $churn0 i64.const 19 struct.new $trash drop local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn0 end
local.get $a i32.const 8 array.get $a ref.as_non_null struct.get $n 0 i32.const 71 i32.ne if unreachable end
local.get $a i32.const 8 array.get $a ref.as_non_null i32.const 91 struct.set $n 0
i32.const 0 local.set $i loop $churn1 i64.const 19 struct.new $trash drop local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn1 end
local.get $a i32.const 8 array.get $a ref.as_non_null struct.get $n 0 i32.const 91 i32.ne if unreachable end
))
