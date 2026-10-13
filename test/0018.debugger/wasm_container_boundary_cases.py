"""Fixed empty-table and dropped-element trap oracles for Wasm-only debugging."""


def empty_table_examples():
    cases = []
    for wide in (False, True):
        width = 'i64' if wide else 'i32'
        prefix = ['i64 = -991', 'f32 = bits=0x80000000']
        code = ['global.get $p', 'global.get $z']
        expected = []
        contents = []

        def observe(extra=(), final=False):
            code.append('nop')
            values = [r'\(ref null func\) = ' + ('null' if v is None
                      else f'function module=0 index={v}') for v in contents]
            expected.append(dict(function=1, ordinal=len(expected),
                values=[] if final else prefix + list(extra), caller=None,
                locals_page=None, frame_total=None, tables=[dict(index=0, values=values)], members=[]))

        observe()
        code.append('table.size $t'); observe([f'{width} = 0']); code.append('drop')
        code.extend((f'{width}.const 0', 'ref.null func', f'{width}.const 0', 'table.fill $t'))
        observe()
        code.extend((f'{width}.const 0', f'{width}.const 0', f'{width}.const 0', 'table.copy $t $t'))
        observe()
        code.extend(('elem.drop $e', f'{width}.const 0', 'i32.const 0', 'i32.const 0', 'table.init $t $e'))
        observe()
        code.extend(('ref.func $leaf', f'{width}.const 1', 'table.grow $t'))
        contents = [0]; observe([f'{width} = 0']); code.append('drop')
        code.extend(('ref.null func', f'{width}.const 2', 'table.grow $t'))
        observe([f'{width} = -1']); code.append('drop')
        code.extend(('ref.null func', f'{width}.const 1', 'table.grow $t'))
        contents = [0, None]; observe([f'{width} = 1']); code.append('drop')
        code.extend(('ref.func $leaf', f'{width}.const 0', 'table.grow $t'))
        observe([f'{width} = 2']); code.append('drop')
        code.extend((f'{width}.const 0', 'call_indirect $t (type $f)'))
        observe(['i32 = 17']); code.extend(('drop', 'drop', 'drop')); observe(final=True)
        cases.append(dict(name='empty-table-' + ('i64' if wide else 'i32'), expected=expected,
            wat='(module (global $p (mut i64) (i64.const -991)) '
                '(global $z (mut f32) (f32.const -0)) (type $f (func (result i32))) '
                f'(table $t {"i64 " if wide else ""}0 2 funcref) '
                '(func $leaf (type $f) i32.const 17) (elem $e func $leaf) '
                '(func (export "_start") ' + ' '.join(code) + '))'))
    return cases


def dropped_elem_trap_examples():
    cases = []
    for typed in (False, True):
        reference = '(ref null $f)' if typed else 'funcref'
        null_type = 'null type-index=0 module=0' if typed else 'null func'
        for operation in ('new', 'init'):
            values = ['i64 = -991', 'f32 = bits=0x80000000']
            code = ['i32.const 1', 'array.new_default $a', 'local.set $a',
                    'i32.const 0', 'local.get $a', 'table.set $roots', 'elem.drop $e',
                    'global.get $p', 'global.get $z']
            if operation == 'init':
                code.extend(('local.get $a', 'i32.const 0'))
                values.extend([r'\(ref null type-index=1 module=0\) = array #1', 'i32 = 0'])
            code.extend(('i32.const 0', 'i32.const 1', 'nop', f'array.{operation}_elem $a $e'))
            values.extend(['i32 = 0', 'i32 = 1'])
            if operation == 'new':
                code.append('drop')
            code.extend(('nop', 'drop', 'drop'))
            members = [dict(selection=s, root=0, table=0, path=[], kind='array', type_index=1,
                values=[rf'\(ref {null_type}\) = null mutable']) for s in ('locals', 'table')]
            point = dict(function=1, ordinal=0, values=values, caller=None, locals_page=None,
                         frame_total=None, tables=[], members=members)
            cases.append(dict(name=f'dropped-elem-{operation}-' + ('typed' if typed else 'generic'),
                expected=[point], operation='array.' + operation + '_elem',
                diagnostic='Runtime crash (array access out of bounds)',
                wat='(module (global $p (mut i64) (i64.const -991)) '
                    '(global $z (mut f32) (f32.const -0)) (type $f (func (result i32))) '
                    f'(type $a (array (mut {reference}))) (table $roots 1 (ref null $a)) '
                    '(func $leaf (type $f) i32.const 17) '
                    f'(elem $e {reference} (ref.func $leaf)) '
                    '(func (export "_start") (local $a (ref null $a)) ' + ' '.join(code) + '))'))
    return cases
