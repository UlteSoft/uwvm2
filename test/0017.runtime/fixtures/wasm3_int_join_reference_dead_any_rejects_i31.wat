(module
  ;; A dead execution edge does not make the enclosing validation stack bottom.
  ;; The declared any result remains too broad for i31.get_s after the else.
  (func $probe (result i32)
    block (result (ref any)) unreachable end
    i32.const 0 if else end i31.get_s)
  (func (export "_start") call $probe drop))
