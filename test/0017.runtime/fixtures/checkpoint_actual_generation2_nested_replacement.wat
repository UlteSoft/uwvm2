(module
  (global $root_prefix (mut i32) (i32.const 0))
  (global $child_prefix (mut i32) (i32.const 0))
  (global $after_capture (mut i32) (i32.const 0))
  ;; Linux keeper extracts the COMPLETE first function body using official WAT
  ;; assembler; the target ABI remains (i64)->i64, actual new i31 local is21.
  (func (param $counter i64) (result i64) (local $hold (ref i31))
    global.get $child_prefix i32.const 1 i32.add global.set $child_prefix
    i32.const 21 ref.i31 local.set $hold
    i64.const 99
    nop
    global.get $after_capture i32.const 1 i32.add global.set $after_capture
    local.get $counter i64.add
    local.get $hold i31.get_u i64.extend_i32_u i64.add))
