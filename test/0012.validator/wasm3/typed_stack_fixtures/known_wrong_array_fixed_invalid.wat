(module (type $refs (array (mut (ref null eq))))
  (func (export "_start")
    unreachable i64.const 1 array.new_fixed $refs 4294967295 drop))
