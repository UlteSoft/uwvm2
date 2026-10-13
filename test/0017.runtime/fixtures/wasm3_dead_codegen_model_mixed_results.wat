(module
  ;; Non-defaultable reference and v128 results need physical accounting after end.
  (func $probe (result i32)
    block (result i32 (ref i31) v128) unreachable end
    drop i31.get_s i32.const 0 select)
  (func (export "_start") call $probe drop))
