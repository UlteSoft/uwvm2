(module (func $probe block (result externref) ref.null exn end drop)
 (func (export "_start") call $probe))
