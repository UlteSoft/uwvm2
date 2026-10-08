(module
  (tag $t)
  (func (export "_start")
    (local $saved (ref null exn))
    (block $caught (result (ref exn))
      try_table (catch_ref $t $caught)
        throw $t
      end
      unreachable)
    local.tee $saved
    ref.test (ref exn)
    i32.eqz
    if unreachable end
    local.get $saved
    ref.cast (ref exn)
    drop
    (block $matched (result (ref exn))
      local.get $saved
      br_on_cast $matched (ref null exn) (ref exn)
      drop
      unreachable)
    drop))
