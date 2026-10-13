"""Live bulk-memory/table stacks and readback for every 32/64-bit width pair.

Expected results come from fixed byte/reference layouts, not from a second
execution of the implementation under test. Run through the real CLI harness.
"""


def bulk_state_examples():
    cases = []
    prefix = ['i64 = -991', 'f32 = bits=0x80000000']
    globals_ = '(global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0))'

    for destination in ('i32', 'i64'):
        for source in ('i32', 'i64'):
            length = 'i64' if destination == source == 'i64' else 'i32'

            def builder(function):
                code = ['global.get $p', 'global.get $z']
                expected = []

                def observe(values):
                    code.append('nop')
                    expected.append(dict(function=function, ordinal=len(expected),
                        values=prefix + values, caller=None, locals_page=None, frame_total=None))

                def finish():
                    code.extend(('drop', 'drop', 'nop'))
                    expected.append(dict(function=function, ordinal=len(expected),
                        values=[], caller=None, locals_page=None, frame_total=None))

                return code, expected, observe, finish

            def number(kind, value):
                return f'{kind} = {value}'

            code, expected, observe, finish = builder(0)

            def memory_readback(memory, kind, first, values):
                for offset, value in enumerate(values, first):
                    code.extend((f'{kind}.const {offset}', f'i32.load8_u {memory}'))
                    observe([number('i32', value)])
                    code.append('drop')

            code.extend((f'{source}.const 16', 'i32.const 1', 'i32.const 4'))
            observe([number(source, 16), number('i32', 1), number('i32', 4)])
            code.append('memory.init $src $data'); observe([])
            memory_readback('$src', source, 16, [22, 33, 44, 55])

            code.extend((f'{destination}.const 32', f'{source}.const 16', f'{length}.const 4'))
            observe([number(destination, 32), number(source, 16), number(length, 4)])
            code.append('memory.copy $dst $src'); observe([])
            memory_readback('$dst', destination, 32, [22, 33, 44, 55])

            code.extend((f'{destination}.const 33', f'{destination}.const 32', f'{destination}.const 3'))
            observe([number(destination, 33), number(destination, 32), number(destination, 3)])
            code.append('memory.copy $dst $dst'); observe([])
            memory_readback('$dst', destination, 32, [22, 22, 33, 44])

            code.extend((f'{destination}.const 34', 'i32.const 511', f'{destination}.const 2'))
            observe([number(destination, 34), number('i32', 511), number(destination, 2)])
            code.append('memory.fill $dst'); observe([])
            memory_readback('$dst', destination, 32, [22, 22, 255, 255])

            observe([]); code.append('data.drop $data'); observe([])
            code.extend((f'{source}.const 65536', 'i32.const 0', 'i32.const 0'))
            observe([number(source, 65536), number('i32', 0), number('i32', 0)])
            code.append('memory.init $src $data'); observe([])
            code.append('memory.size $dst'); observe([number(destination, 1)]); code.append('drop')
            for delta, old_size, new_size in ((1, 1, 2), (1, -1, 2)):
                code.append(f'{destination}.const {delta}'); observe([number(destination, delta)])
                code.append('memory.grow $dst'); observe([number(destination, old_size)]); code.append('drop')
                code.append('memory.size $dst'); observe([number(destination, new_size)]); code.append('drop')
            finish()
            cases.append(dict(name=f'bulk-memory-dst-{destination}-src-{source}',
                wat=f'(module {globals_} (memory $src {"i64 " if source == "i64" else ""}1 2) '
                    f'(memory $dst {"i64 " if destination == "i64" else ""}1 2) '
                    '(data $data "\\0b\\16\\21\\2c\\37") (func (export "_start") '
                    + ' '.join(code) + '))', expected=expected))

            code, expected, observe, finish = builder(2)
            null = r'\(ref null func\) = null'

            def function_value(index, nullable=True):
                kind = 'null func' if nullable else 'type-index=0 module=0'
                return rf'\(ref {kind}\) = function module=0 index={index}'

            def table_readback(table, kind, first, values):
                for index, function in enumerate(values, first):
                    code.extend((f'{kind}.const {index}', f'table.get {table}'))
                    observe([null if function is None else function_value(function)])
                    code.append('drop')

            code.extend((f'{source}.const 1', 'i32.const 0', 'i32.const 2'))
            observe([number(source, 1), number('i32', 0), number('i32', 2)])
            code.append('table.init $src $elem'); observe([])
            table_readback('$src', source, 1, [0, 1])

            code.extend((f'{destination}.const 0', f'{source}.const 1', f'{length}.const 2'))
            observe([number(destination, 0), number(source, 1), number(length, 2)])
            code.append('table.copy $dst $src'); observe([])
            table_readback('$dst', destination, 0, [0, 1, None])

            code.extend((f'{destination}.const 1', f'{destination}.const 0', f'{destination}.const 2'))
            observe([number(destination, 1), number(destination, 0), number(destination, 2)])
            code.append('table.copy $dst $dst'); observe([])
            table_readback('$dst', destination, 0, [0, 0, 1, None])

            code.extend((f'{destination}.const 2', 'ref.func $leaf1', f'{destination}.const 2'))
            observe([number(destination, 2), function_value(1, False), number(destination, 2)])
            code.append('table.fill $dst'); observe([])
            table_readback('$dst', destination, 0, [0, 0, 1, 1])

            code.extend((f'{destination}.const 0', 'ref.null func'))
            observe([number(destination, 0), null]); code.append('table.set $dst'); observe([])
            table_readback('$dst', destination, 0, [None])
            for delta, old_size, new_size, function in ((1, 4, 5, 0), (2, -1, 5, None)):
                code.extend(('ref.null func' if function is None else 'ref.func $leaf0',
                             f'{destination}.const {delta}'))
                observe([null if function is None else function_value(function, False), number(destination, delta)])
                code.append('table.grow $dst'); observe([number(destination, old_size)]); code.append('drop')
                code.append('table.size $dst'); observe([number(destination, new_size)]); code.append('drop')
            table_readback('$dst', destination, 4, [0])
            observe([]); code.append('elem.drop $elem'); observe([])
            code.extend((f'{source}.const 4', 'i32.const 0', 'i32.const 0'))
            observe([number(source, 4), number('i32', 0), number('i32', 0)])
            code.append('table.init $src $elem'); observe([])
            code.append(f'{destination}.const 4'); observe([number(destination, 4)])
            code.append('call_indirect $dst (type $f)'); observe([number('i32', 17)]); code.append('drop')
            finish()
            cases.append(dict(name=f'bulk-table-dst-{destination}-src-{source}',
                wat=f'(module {globals_} (type $f (func (result i32))) '
                    f'(table $src {"i64 " if source == "i64" else ""}4 6 funcref) '
                    f'(table $dst {"i64 " if destination == "i64" else ""}4 6 funcref) '
                    '(func $leaf0 (type $f) i32.const 17) (func $leaf1 (type $f) i32.const 29) '
                    '(elem $elem func $leaf0 $leaf1) (func (export "_start") '
                    + ' '.join(code) + '))', expected=expected))
    return cases
