"""Real two-worker Wasm exception fixtures; independent of debugger output."""


def examples():
    for leaf in ('numeric', 'tuple', 'reference'):
        for disposition in ('caught', 'uncaught'):
            tuple_case = leaf == 'tuple'
            tag = '$wide' if tuple_case else '$number'
            result = 'i32 i64 v128 (ref null $node)' if tuple_case else 'i32'
            call = f'i64.const -777 local.get 0 call ${leaf}'
            body = (f'''block $caught (result {result})
                try_table (catch {tag} $caught)
                  {call} drop drop unreachable
                end unreachable
              end {'drop drop drop' if tuple_case else ''}'''
                    if disposition == 'caught' else f'{call} drop drop i32.const 0')
            wat = f'''(module
              (type $node (struct (field i32)))
              (tag $number (param i32))
              (tag $wide (param i32 i64 v128 (ref null $node)))
              (global $release (mut i32) (i32.const 0))
              (func $numeric (param i32) (result i32) (local i64)
                i64.const -1234 local.set 1
                i64.const -991 local.get 0 throw $number)
              (func $tuple (param i32) (result i32) (local (ref null $node))
                i32.const 42 struct.new $node local.set 1
                i64.const -991 local.get 0 i64.const -1234567890123
                v128.const i32x4 1 2 3 4 local.get 1 throw $wide)
              (func $reference (param i32) (result i32) (local exnref)
                block $saved (result i32 exnref)
                  try_table (catch_ref $number $saved)
                    local.get 0 throw $number
                  end unreachable
                end local.set 1 drop
                i64.const -991 local.get 1 throw_ref)
              (func $entry (export "entry") (param i32) (result i32) (local i64)
                i64.const -888 local.set 1 {body})
              (func $companion (export "companion") (param i32) (result i32) (local i64)
                i64.const 1234 local.set 1
                loop $spin
                  i64.const -777 global.get $release i32.eqz br_if $spin drop
                end local.get 0))'''
            yield dict(name=f'{leaf}-{disposition}', leaf=leaf,
                       disposition=disposition, wat=wat)
