(module
  (type $parent (sub (struct (field i32))))
  (type $child (sub $parent (struct (field i32))))
  (type $refs (array (mut (ref null $parent))))
  (func $aggregate (result i32)
    ref.null $child
    i32.const 4 struct.new $child
    array.new_fixed $refs 2
    array.len)
  (func $bottom (result i32)
    i32.const 0 if
      unreachable ref.as_non_null throw_ref
    end
    i32.const 0 if
      unreachable array.new_fixed $refs 4294967295 drop
    end
    i32.const 0 if
      ref.null noexn ref.as_non_null throw_ref
    end
    i32.const 40)
  (func (export "_start")
    call $aggregate call $bottom i32.add i32.const 42 i32.ne
    if unreachable end))
