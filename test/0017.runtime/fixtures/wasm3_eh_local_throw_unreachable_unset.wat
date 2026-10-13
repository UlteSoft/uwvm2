;; Stack polymorphism after throw does not waive definite local initialization.
(module
  (type $box (struct (field i32)))
  (tag $value)
  (func $probe (param $raise i32) (local $slot (ref $box))
    block $handled
      try_table (catch_all $handled)
        local.get $raise
        if
          throw $value
          local.get $slot drop
        end
      end
    end)
  (func (export "_start") i32.const 0 call $probe))
