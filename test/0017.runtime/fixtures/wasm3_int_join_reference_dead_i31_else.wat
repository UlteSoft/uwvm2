(module
  ;; This probe is valid even though no execution edge reaches the inner result.
  ;; Its declared i31 prefix must survive the later explicit else restoration.
  (func $probe (result i32)
    block (result (ref i31)) unreachable end
    i32.const 0 if else end i31.get_s)
  (func (export "_start") call $probe drop))
