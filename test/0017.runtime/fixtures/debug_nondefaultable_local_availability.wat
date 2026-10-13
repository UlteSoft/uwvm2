;; Core 3 nondefaultable local is valid before first assignment. Debug capture
;; must retain index/type as unavailable and must not load its uninitialized
;; native alloca. After local.set the same original index becomes available.
(module
  (type $node (struct (field i32)))
  (func (export "run") (result i32)
    (local $unset (ref $node))
    (local $nullable (ref null $node))
    nop
    (local.set $unset (struct.new $node (i32.const 42)))
    nop
    (struct.get $node 0 (local.get $unset))))
