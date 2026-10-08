;; Same function indices and observable result as the ordinary finish fixture.
;; Sixty-four scalar parameters force real RV64 TailCC stack arguments.
(module
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (func $leaf (param i32) (result i32) local.get 0 i32.const 7 i32.add)
  (func $recursive (param i32 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64) (result i32)
    local.get 0 i32.eqz
    if (result i32) local.get 63 i32.wrap_i64 call $leaf
    else
      local.get 0 i32.const 1 i32.sub
      local.get 1
      local.get 2
      local.get 3
      local.get 4
      local.get 5
      local.get 6
      local.get 7
      local.get 8
      local.get 9
      local.get 10
      local.get 11
      local.get 12
      local.get 13
      local.get 14
      local.get 15
      local.get 16
      local.get 17
      local.get 18
      local.get 19
      local.get 20
      local.get 21
      local.get 22
      local.get 23
      local.get 24
      local.get 25
      local.get 26
      local.get 27
      local.get 28
      local.get 29
      local.get 30
      local.get 31
      local.get 32
      local.get 33
      local.get 34
      local.get 35
      local.get 36
      local.get 37
      local.get 38
      local.get 39
      local.get 40
      local.get 41
      local.get 42
      local.get 43
      local.get 44
      local.get 45
      local.get 46
      local.get 47
      local.get 48
      local.get 49
      local.get 50
      local.get 51
      local.get 52
      local.get 53
      local.get 54
      local.get 55
      local.get 56
      local.get 57
      local.get 58
      local.get 59
      local.get 60
      local.get 61
      local.get 62
      local.get 63
      call $recursive i32.const 1 i32.add
    end)
  (func $root (result i32)
    i32.const 2
    i64.const 1
    i64.const 2
    i64.const 3
    i64.const 4
    i64.const 5
    i64.const 6
    i64.const 7
    i64.const 8
    i64.const 9
    i64.const 10
    i64.const 11
    i64.const 12
    i64.const 13
    i64.const 14
    i64.const 15
    i64.const 16
    i64.const 17
    i64.const 18
    i64.const 19
    i64.const 20
    i64.const 21
    i64.const 22
    i64.const 23
    i64.const 24
    i64.const 25
    i64.const 26
    i64.const 27
    i64.const 28
    i64.const 29
    i64.const 30
    i64.const 31
    i64.const 32
    i64.const 33
    i64.const 34
    i64.const 35
    i64.const 36
    i64.const 37
    i64.const 38
    i64.const 39
    i64.const 40
    i64.const 41
    i64.const 42
    i64.const 43
    i64.const 44
    i64.const 45
    i64.const 46
    i64.const 47
    i64.const 48
    i64.const 49
    i64.const 50
    i64.const 51
    i64.const 52
    i64.const 53
    i64.const 54
    i64.const 55
    i64.const 56
    i64.const 57
    i64.const 58
    i64.const 59
    i64.const 60
    i64.const 61
    i64.const 62
    i64.const 5
    call $recursive)
  (func $spin_root (result i32) call $spin)
  (func $spin (result i32)
    (loop $again
      i32.const 0 call $leaf drop
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)
    i32.const 77))
