;; Core 3 GC: packed mutable arrays, bounds-checked writes and sign extension.
(module
  (type $numbers (array (mut i16)))
  (func (export "_start") (local $a (ref null $numbers))
    i32.const -1
    i32.const 4
    array.new $numbers
    local.set $a

    local.get $a
    array.len
    i32.const 4
    i32.ne
    if unreachable end

    local.get $a
    i32.const 2
    i32.const 32767
    array.set $numbers
    local.get $a
    i32.const 2
    array.get_s $numbers
    i32.const 32767
    i32.ne
    if unreachable end

    local.get $a
    i32.const 0
    array.get_s $numbers
    i32.const -1
    i32.ne
    if unreachable end))
