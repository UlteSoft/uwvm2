(module
  (tag $error (param i32))
  (func (export "_start") unreachable)
  ;; Even an unused catch must match its target label's actual payload type.
  (func $unused
    block $wrong (result i64)
      try_table (catch $error $wrong) i32.const 7 throw $error end
      unreachable
    end drop))
