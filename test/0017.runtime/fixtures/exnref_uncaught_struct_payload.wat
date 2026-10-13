;; Uncaught Core 3 exception with a full 16-byte managed reference payload.
;; The diagnostic must preserve the struct kind and original throwing frame.
(module
  (type $record (struct (field i32)))
  (tag $t (param (ref $record)))
  (func $leaf
    i32.const 7
    struct.new $record
    throw $t)
  (func (export "_start")
    call $leaf))
