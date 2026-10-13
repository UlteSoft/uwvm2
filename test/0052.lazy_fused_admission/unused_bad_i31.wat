(module
  ;; _start never calls this invalid modern GC instruction body.
  (func $unused (result i32) ref.null func i31.get_u)
  (func (export "_start")))
