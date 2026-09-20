(module
  (memory 1)
  (func (export "_start")
    (i32.store align=4 (i32.const 1) (i32.const 0x78563412))
    (if (i32.ne (i32.load align=4 (i32.const 1)) (i32.const 0x78563412))
      (then unreachable))))
