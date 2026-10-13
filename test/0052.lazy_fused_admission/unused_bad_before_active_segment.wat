(module
  (memory 1)
  ;; Out-of-bounds active data is an instantiation error, but the invalid body
  ;; must be diagnosed FIRST by fused admission, before applying this segment.
  (data (i32.const 65536) "X")
  (func $unused ref.null func i31.get_u drop)
  (func (export "_start")))
