(module
  (type $record (struct (field i32)))
  (func $unused f64.const 1 struct.new $record drop)
  (func (export "_start")))
