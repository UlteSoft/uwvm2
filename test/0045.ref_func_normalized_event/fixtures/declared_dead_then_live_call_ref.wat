(module
  (type $signature (func (result i32)))
  (func $target (type $signature) (result i32) i32.const 42)
  (elem declare func $target)
  (func (export "_start")
    (block $skip br $skip ref.func $target drop)
    ref.func $target call_ref $signature
    i32.const 42 i32.ne if unreachable end)
)
