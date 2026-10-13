(module
  (memory i64 1)
  (type $record (struct (field i64)))
  (func $empty_start)
  (start $empty_start)
  ;; This modern Core3 invalid function is genuinely unused by start/exports.
  (func $unused i64.const 0 i64.add drop)
)
