;; Core 3 Bot is accepted by every numeric operand, including i64 addresses.
;; The function is compiled when _start calls it, but its unreachable branch is not run.
;; Invalid: a known i32 does not become Bot after unreachable.
(module
  (memory i64 1)
  (func $probe (param i32)
    local.get 0
    if
      unreachable
      i32.const 0
      memory.fill
    end)
  (func (export "_start")
    i32.const 0
    call $probe))
