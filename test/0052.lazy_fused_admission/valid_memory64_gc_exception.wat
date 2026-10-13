(module
  (memory i64 1)
  (type $record (struct (field i32)))
  (tag $tag (param i32))
  (func $caught (result i32)
    block $done (result i32)
      try_table (catch $tag $done) i32.const 47 throw $tag end
      unreachable
    end)
  (func (export "_start")
    i64.const 0 call $caught struct.new $record struct.get $record 0 i32.store
    i64.const 0 i32.load i32.const 47 i32.ne if unreachable end))
