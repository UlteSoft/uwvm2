(module
  ;; The dead then-arm model must not overwrite the real saved else-entry ring.
  (func $probe (param i32) (result i32)
    local.get 0 if (result i32)
      block (result i32 i32) unreachable end
      i32.const 0 select
    else i32.const 47 end)
  (func (export "_start")
    i32.const 0 call $probe i32.const 47 i32.ne if unreachable end))
