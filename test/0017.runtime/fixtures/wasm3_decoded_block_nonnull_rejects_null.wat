(module
 (type $s (struct (field i32)))
 (func $probe block (result (ref $s)) ref.null $s end drop)
 (func (export "_start") call $probe))
