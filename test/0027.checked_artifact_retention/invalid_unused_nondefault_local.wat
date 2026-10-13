(module
  ;; _start never calls this body, but the all-function fused producer must reject it.
  (func $invalid (result (ref any)) (local (ref any)) local.get 0)
  (func (export "_start")))
