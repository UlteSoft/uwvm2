;; Core 3 none is the bottom of the GC heap hierarchy. Typed aggregate
;; globals may be initialized with its null value without function references.
(module
  (type $record (struct (field i32)))
  (global $saved (ref null $record) (ref.null none))
  (func (export "_start")
    global.get $saved
    ref.is_null
    i32.eqz
    if unreachable end))
