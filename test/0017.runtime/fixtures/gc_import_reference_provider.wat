;; Core 3 cross-module GC: provider and consumer use equivalent recursive types
;; at different module-local indices. The returned aggregate must remain owned.
(module
  (type $unrelated (func))
  (type $record (struct (field i32)))
  (type $make (func (result (ref $record))))
  (func (export "make") (type $make)
    i32.const 42
    struct.new $record))
