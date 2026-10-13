(module
  (tag $tag (param i32))
  ;; Catch transports i32; the declared target requires i64.
  (func $unused
    block $caught (result i64)
      try_table (catch $tag $caught) i32.const 1 throw $tag end
      unreachable
    end drop)
  (func (export "_start")))
