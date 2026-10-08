;; The local $record is index 0; the provider's equivalent $record is index 1.
(module
  (type $record (struct (field i32)))
  (type $make (func (result (ref $record))))
  (import "P" "make" (func $make (type $make)))
  (func (export "_start")
    call $make
    struct.get $record 0
    i32.const 42
    i32.ne
    if unreachable end))
