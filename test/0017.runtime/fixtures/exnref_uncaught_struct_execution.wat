;; An uncaught Core 3 exception carries a VM-managed struct reference. The
;; diagnostic may print its opaque token and kind but must never dereference it.
(module
  (type $record (struct (field i32)))
  (tag $t (param (ref $record)))
  (func $throwing (export "_start")
    i32.const 37
    struct.new $record
    throw $t))
