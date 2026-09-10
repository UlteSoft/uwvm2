(module
  (type $v (func))
  (memory 1)

  ;; Preserve four Wasm call frames while the access faults in the registered mmap guard.
  (func $leaf (type $v)
    i32.const -1
    i64.load offset=4294967295 align=1
    drop)
  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
