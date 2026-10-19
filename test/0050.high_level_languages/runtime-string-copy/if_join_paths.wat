(module
  (memory 1)
  (data (i32.const 56) "\64\00\00\00")
  (func $require (param $ok i32)
    (if (i32.eqz (local.get $ok)) (then unreachable)))
  (func $implicit (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1)
      )
      (else (i32.const 2)))
    (i32.add))
  (func $then_br (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1) (br $join)
      )
      (else (i32.const 2)))
    (i32.add))
  (func $else_br (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1)
      )
      (else (i32.const 2) (br $join)))
    (i32.add))
  (func $no_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
      ))
    (i32.const 1) (i32.add))
  (func $implicit_true
    (call $require (i32.eq (call $implicit (i32.const 1)) (i32.const 101))))
  (func $implicit_false
    (call $require (i32.eq (call $implicit (i32.const 0)) (i32.const 102))))
  (func $then_br_true
    (call $require (i32.eq (call $then_br (i32.const 1)) (i32.const 101))))
  (func $then_br_false
    (call $require (i32.eq (call $then_br (i32.const 0)) (i32.const 102))))
  (func $else_br_true
    (call $require (i32.eq (call $else_br (i32.const 1)) (i32.const 101))))
  (func $else_br_false
    (call $require (i32.eq (call $else_br (i32.const 0)) (i32.const 102))))
  (func $no_else_true
    (call $require (i32.eq (call $no_else (i32.const 1)) (i32.const 101))))
  (func $no_else_false
    (call $require (i32.eq (call $no_else (i32.const 0)) (i32.const 101))))
  (func (export "_start")
    (call $implicit_true)
    (call $implicit_false)
    (call $then_br_true)
    (call $then_br_false)
    (call $else_br_true)
    (call $else_br_false)
    (call $no_else_true)
    (call $no_else_false)
  ))
