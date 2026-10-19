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
  (func $br_if_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1))
      (else (i32.const 2) (local.get $cond) (i32.eqz) (br_if $join)))
    (i32.add))
  (func $br_table_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1))
      (else (i32.const 2) (i32.const 0) (br_table $join $join)))
    (i32.add))
  (func $no_else_br (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (br $join)))
    (i32.const 1) (i32.add))
  (func $ref_null_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result i32)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1))
      (else (i32.const 2) (ref.null extern) (br_on_null $join) (drop)))
    (i32.add))
  (func $ref_non_null_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result (ref any))
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1) (ref.i31))
      (else (i32.const 2) (ref.i31) (br_on_non_null $join) unreachable))
    (ref.cast (ref i31)) (i31.get_u)
    (i32.add))
  (func $ref_cast_else (param $cond i32) (result i32) (local $i i32) (local $hash i32)
    (i32.const 0) (i32.load offset=56)
    (local.get $cond)
    (if $join (result (ref any))
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i32.const 1) (ref.i31))
      (else (i32.const 2) (ref.i31) (br_on_cast $join (ref null any) (ref i31)) (drop) unreachable))
    (ref.cast (ref i31)) (i31.get_u)
    (i32.add))
  (func $wide_i64 (param $cond i32) (result i64) (local $i i32) (local $hash i32)
    (i64.const 100)
    (local.get $cond)
    (if $join (result i64)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (i64.const 1))
      (else (i64.const 2)))
    (i64.add))
  (func $wide_f64 (param $cond i32) (result f64) (local $i i32) (local $hash i32)
    (f64.const 100)
    (local.get $cond)
    (if $join (result f64)
      (then
        (local.set $hash (i32.const 1))
        (loop $again
          (local.set $hash (i32.add (i32.mul (local.get $hash) (i32.const 263)) (i32.const 7)))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $again (i32.lt_u (local.get $i) (i32.const 5))))
        (f64.const 1.5))
      (else (f64.const 2.5)))
    (f64.add))
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
  (func $br_if_else_true
    (call $require (i32.eq (call $br_if_else (i32.const 1)) (i32.const 101))))
  (func $br_if_else_false
    (call $require (i32.eq (call $br_if_else (i32.const 0)) (i32.const 102))))
  (func $br_table_else_true
    (call $require (i32.eq (call $br_table_else (i32.const 1)) (i32.const 101))))
  (func $br_table_else_false
    (call $require (i32.eq (call $br_table_else (i32.const 0)) (i32.const 102))))
  (func $no_else_br_true
    (call $require (i32.eq (call $no_else_br (i32.const 1)) (i32.const 101))))
  (func $no_else_br_false
    (call $require (i32.eq (call $no_else_br (i32.const 0)) (i32.const 101))))
  (func $ref_null_else_true
    (call $require (i32.eq (call $ref_null_else (i32.const 1)) (i32.const 101))))
  (func $ref_null_else_false
    (call $require (i32.eq (call $ref_null_else (i32.const 0)) (i32.const 102))))
  (func $ref_non_null_else_true
    (call $require (i32.eq (call $ref_non_null_else (i32.const 1)) (i32.const 101))))
  (func $ref_non_null_else_false
    (call $require (i32.eq (call $ref_non_null_else (i32.const 0)) (i32.const 102))))
  (func $ref_cast_else_true
    (call $require (i32.eq (call $ref_cast_else (i32.const 1)) (i32.const 101))))
  (func $ref_cast_else_false
    (call $require (i32.eq (call $ref_cast_else (i32.const 0)) (i32.const 102))))
  (func $wide_i64_true
    (call $require (i64.eq (call $wide_i64 (i32.const 1)) (i64.const 101))))
  (func $wide_i64_false
    (call $require (i64.eq (call $wide_i64 (i32.const 0)) (i64.const 102))))
  (func $wide_f64_true
    (call $require (f64.eq (call $wide_f64 (i32.const 1)) (f64.const 101.5))))
  (func $wide_f64_false
    (call $require (f64.eq (call $wide_f64 (i32.const 0)) (f64.const 102.5))))
  (func (export "_start")
    (call $implicit_true)
    (call $implicit_false)
    (call $then_br_true)
    (call $then_br_false)
    (call $else_br_true)
    (call $else_br_false)
    (call $no_else_true)
    (call $no_else_false)
    (call $br_if_else_true)
    (call $br_if_else_false)
    (call $br_table_else_true)
    (call $br_table_else_false)
    (call $no_else_br_true)
    (call $no_else_br_false)
    (call $ref_null_else_true)
    (call $ref_null_else_false)
    (call $ref_non_null_else_true)
    (call $ref_non_null_else_false)
    (call $ref_cast_else_true)
    (call $ref_cast_else_false)
    (call $wide_i64_true)
    (call $wide_i64_false)
    (call $wide_f64_true)
    (call $wide_f64_false)
  ))
