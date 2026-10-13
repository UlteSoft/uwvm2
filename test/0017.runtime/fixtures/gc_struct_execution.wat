;; Core 3 GC: mutable and packed fields, signed/unsigned unpacking.
(module
  (type $pair (struct (field (mut i32)) (field i8)))
  (func (export "_start") (local $p (ref null $pair))
    i32.const 41
    i32.const -1
    struct.new $pair
    local.set $p

    local.get $p
    struct.get $pair 0
    i32.const 41
    i32.ne
    if unreachable end

    local.get $p
    i32.const 42
    struct.set $pair 0
    local.get $p
    struct.get $pair 0
    i32.const 42
    i32.ne
    if unreachable end

    local.get $p
    struct.get_s $pair 1
    i32.const -1
    i32.ne
    if unreachable end
    local.get $p
    struct.get_u $pair 1
    i32.const 255
    i32.ne
    if unreachable end))
