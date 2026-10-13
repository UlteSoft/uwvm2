(module
  (type $signature (func (result i32)))
  (func $not_declared (type $signature) (result i32) i32.const 42)
  ;; Only body occurrences refer to function0. _start export declares function1.
  (func (export "_start")
    (block $skip br $skip ref.func $not_declared drop))
)
