;; Exercise a real, non-null exception token across the exnref call-frame ABI.
(module
  (tag $tag)
  (type $identity (func (param exnref) (result exnref)))
  (func $identity (type $identity)
    local.get 0)
  (func (export "_start")
    (block $caught (result (ref exn))
      try_table (catch_ref $tag $caught)
        throw $tag
      end
      unreachable)
    call $identity
    ref.is_null
    if unreachable end))
