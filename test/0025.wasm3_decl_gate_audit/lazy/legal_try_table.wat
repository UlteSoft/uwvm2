(module (tag $t (param i32)) (func $probe (block $caught (result i32) (try_table (catch $t $caught) i32.const 7 throw $t) i32.const 0) drop) (func (export "_start") call $probe))
