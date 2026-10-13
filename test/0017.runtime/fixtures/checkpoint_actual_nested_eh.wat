(module
  (type $record (struct (field (mut i32))))
  (tag $event (param i32 (ref $record)))
  (global $guest_catches (mut i32) (i32.const 0))
  (func (export "_start") (result i32)
    (local $payload i32)
    (local $object (ref null $record))
    (local $object_alias (ref null $record))
    (local $exception (ref null exn))
    (local $exception_alias (ref null exn))
    (local $second_object (ref null $record))
    (local $named_exception (ref null exn))
    block $named (result i32 (ref $record) (ref exn))
      try_table (catch_ref $event $named)
        block $any (result (ref exn))
          try_table (catch_all_ref $any)
            call $raise
            drop
          end
          unreachable
        end
        local.set $exception
        global.get $guest_catches i32.const 1 i32.add global.set $guest_catches
        local.get $exception
        throw_ref
      end
      unreachable
    end
    local.set $named_exception
    local.set $object
    local.set $payload
    global.get $guest_catches i32.const 1 i32.add global.set $guest_catches
    local.get $object local.set $object_alias
    local.get $exception local.set $exception_alias
    ;; Unique exact run4: first caught value + canonical tag/payload + aliases.
    local.get $named_exception
    nop nop nop nop
    drop
    local.get $object ref.as_non_null i32.const 84 struct.set $record 0
    block $again (result i32 (ref $record) (ref exn))
      try_table (catch_ref $event $again)
        local.get $exception
        throw_ref
      end
      unreachable
    end
    local.set $named_exception
    local.set $second_object
    i32.const 37 i32.ne if unreachable end
    global.get $guest_catches i32.const 1 i32.add global.set $guest_catches
    ;; Unique exact run5: same immutable payload now reaches the mutated object.
    local.get $named_exception
    nop nop nop nop nop
    drop
    local.get $payload
    local.get $object ref.as_non_null struct.get $record 0
    i32.add
    local.get $second_object ref.as_non_null struct.get $record 0
    i32.add)
  (func $raise (result i32)
    (local $pending (ref $record))
    ;; Never read this uninitialized nondefaultable local. The real stop has
    ;; an i64 operand AND the distinct saved i64 if-entry parameter.
    i64.const 99
    i32.const 1
    if (param i64) (result i64)
      nop nop nop
    else
      nop
    end
    drop
    i32.const 37
    i32.const 42
    struct.new $record
    throw $event))
