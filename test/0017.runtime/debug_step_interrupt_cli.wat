(module
  (func $callee (loop $forever br $forever))
  (func (export "_start") call $callee))
