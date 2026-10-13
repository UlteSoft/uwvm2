(module
  ;; Validation bottom resets at the inner end; execution remains unreachable.
  ;; The implicit-else merge has no incoming physical snapshot to authenticate.
  (func $probe (result i32)
    block (result (ref i31)) unreachable end
    i32.const 0 if unreachable end i31.get_s)
  (func (export "_start") call $probe drop))
