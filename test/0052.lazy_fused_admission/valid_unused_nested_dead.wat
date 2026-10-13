(module
  ;; Admission must still type/emit unused bodies, including polymorphic stacks.
  (func $unused (result (ref i31))
    unreachable block (result (ref i31)) loop (result (ref i31))
      unreachable drop
    end end)
  (func (export "_start")))
