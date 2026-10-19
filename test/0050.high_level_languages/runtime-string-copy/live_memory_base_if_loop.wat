(module
  (memory 1)
  (data (i32.const 56) "\80\00\00\00")
  (data (i32.const 132) "\78\56\34\12")
  (func (export "_start") (local $i i32) (local $hash i32)
    ;; An older memory base remains live across an if-result and a loop.
    (i32.const 0) (i32.load offset=56)
    (i32.const 1)
    (if (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1))
      (else (i32.const 0)))
    (i32.const 3) (i32.and) (i32.const 2) (i32.shl) (i32.add) (i32.load)
    (i32.const 305419896) (i32.ne)
    (if (then unreachable))))
