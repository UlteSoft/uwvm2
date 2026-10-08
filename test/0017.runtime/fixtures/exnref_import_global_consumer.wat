(module
  (import "P" "saved" (global $saved (mut (ref null exn))))
  (func (export "_start")
    global.get $saved
    ref.is_null
    i32.eqz
    if unreachable end))
