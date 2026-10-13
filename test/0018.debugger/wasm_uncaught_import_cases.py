"""Independent imported-tag and Wasm-to-Wasm exception debug fixtures."""


def imported_uncaught_examples():
    tag = '(module (tag $e (export "e") (param i32)))'
    alias_tag = '(module (import "P" "e" (tag $e (param i32))) (export "e" (tag $e)))'
    provider = '''(module (tag $e (export "e") (param i32))
      (global $p (mut i64) (i64.const -991))
      (global $z (mut f32) (f32.const -0))
      (func (export "f") (local i32) i32.const 31 local.set 0
        global.get $p global.get $z i32.const -17 nop throw $e))'''
    alias_call = '''(module (import "P" "e" (tag $e (param i32)))
      (import "P" "f" (func $f)) (export "e" (tag $e)) (export "f" (func $f)))'''
    seed = '(global $p (mut i64) (i64.const -992)) (global $z (mut f32) (f32.const -0))'
    prefix = 'global.get $p global.get $z '
    values = lambda n: [f'i64 = {n}', 'f32 = bits=0x80000000']
    cases = []

    def add(name, providers, consumer, role, function, handled, caller=None):
        frames = [dict(role=role, function=function, values=values(-991 if role == 'P' else -992) + ['i32 = -17'],
                       locals=['i32 = 31' if role == 'P' else 'i32 = 17'])]
        if caller is not None:
            frames.append(dict(role='C', function=caller, values=values(-992), locals=['i64 = -27']))
        cases.append(dict(name=name, providers=providers, consumer=consumer,
                          leaf_role=role, leaf_function=function, handled=handled,
                          frames=frames, tag_owner='P'))

    local_body = '(local i32) i32.const 17 local.set 0 ' + prefix + 'i32.const -17 nop throw $e'
    add('import-tag-unhandled', [('P', tag)],
        '(module (import "P" "e" (tag $e (param i32))) ' + seed +
        '(func (export "_start") ' + local_body + '))', 'C', 0, False)
    add('import-alias-distinct-tag-unhandled', [('P', tag), ('Q', tag), ('A', alias_tag)],
        '(module (import "A" "e" (tag $e (param i32))) (import "Q" "e" (tag $other (param i32))) ' + seed +
        '(func (export "_start") (local i32) i32.const 17 local.set 0 ' + prefix +
        'block $caught (result i32 (ref exn)) try_table (catch_ref $other $caught) '
        'i32.const -17 nop throw $e end unreachable end drop drop drop drop))', 'C', 0, False)

    def consumer(source, clause=None, tail=False):
        imports = f'(import "{source}" "e" (tag $e (param i32))) (import "{source}" "f" (func $f)) '
        middle = '(func $middle return_call $f)' if tail else ''
        body = '(local i64) i64.const -27 local.set 0 ' + prefix
        if clause is None:
            body += 'call $middle' if tail else 'call $f'
            body += ' drop drop'
        else:
            body += 'block $caught (result ' + ('i32 (ref exn)' if clause != 'catch_all_ref' else '(ref exn)') + ') '
            body += f'try_table ({clause} $caught) call $f end unreachable end '
            body += 'drop drop drop drop' if clause != 'catch_all_ref' else 'drop drop drop'
        return '(module ' + imports + seed + middle + '(func (export "_start") ' + body + '))'

    add('cross-call-unhandled', [('P', provider)], consumer('P'), 'P', 0, False, caller=1)
    add('cross-tail-forward-unhandled', [('P', provider), ('A', alias_call)],
        consumer('A', tail=True), 'P', 0, False, caller=2)
    add('handled-import-alias', [('P', tag), ('A', alias_tag)],
        '(module (import "A" "e" (tag $e (param i32))) (import "P" "e" (tag $same (param i32))) ' + seed +
        '(func (export "_start") (local i32) i32.const 17 local.set 0 ' + prefix +
        'block $caught (result i32 (ref exn)) try_table (catch_ref $same $caught) '
        'i32.const -17 nop throw $e end unreachable end drop drop drop drop))', 'C', 0, True)
    add('handled-cross-tag', [('P', provider)], consumer('P', 'catch_ref $e'), 'P', 0, True, caller=1)
    add('handled-cross-all-ref', [('P', provider)], consumer('P', 'catch_all_ref'), 'P', 0, True, caller=1)
    add('handled-alias-call-tag', [('P', provider), ('A', alias_call)],
        consumer('A', 'catch_ref $e'), 'P', 0, True, caller=1)
    return cases
