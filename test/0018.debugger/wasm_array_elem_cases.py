"""Fixed reference identities for live GC element-segment debugger checks."""


def array_elem_examples():
    cases = []
    for typed in (False, True):
        reference = '(ref null $f)' if typed else 'funcref'
        null = '$f' if typed else 'func'
        printed = 'null type-index=0 module=0' if typed else 'null func'
        prefix = ['i64 = -991', 'f32 = bits=0x80000000']
        code = ['global.get $p', 'global.get $z']
        expected = []
        a = b = empty = None

        def value(function):
            return rf'\(ref {printed}\) = ' + ('null' if function is None
                    else f'function module=0 index={function}') + ' mutable'

        def observe(extra=(), operand_array=None, final=False):
            pages = []
            def page(selection, root, values):
                return dict(selection=selection, root=root, table=0, path=[], kind='array',
                            type_index=1, values=[value(function) for function in values])
            for root, values in enumerate((a, b, empty)):
                if values is not None:
                    pages.extend((page('locals', root, values), page('table', root, values)))
            if operand_array is not None:
                pages.append(page('operands', 2, operand_array))
            code.append('nop')
            expected.append(dict(function=2, ordinal=len(expected), values=[] if final else prefix + list(extra),
                caller=None, locals_page=None, frame_total=None, tables=[], members=pages))

        code.extend(('i32.const 0', 'i32.const 4'))
        observe(['i32 = 0', 'i32 = 4'])
        code.append('array.new_elem $a $elem')
        reference_value = r'\(ref type-index=1 module=0\) = array #1'
        observe([reference_value], operand_array=[0, None, 1, 0])
        code.extend(('local.set $a', 'i32.const 0', 'local.get $a', 'table.set $roots'))
        a = [0, None, 1, 0]; observe()

        code.extend(('i32.const 4', 'array.new_default $a', 'local.set $b',
                     'i32.const 1', 'local.get $b', 'table.set $roots'))
        b = [None] * 4; observe()
        code.extend(('local.get $b', 'i32.const 1', 'i32.const 2', 'i32.const 2'))
        observe([r'\(ref null type-index=1 module=0\) = array #1', 'i32 = 1', 'i32 = 2', 'i32 = 2'])
        code.append('array.init_elem $a $elem')
        b = [None, 1, 0, None]; observe()
        code.extend(('local.get $b', 'i32.const 3', 'ref.func $leaf1', 'array.set $a'))
        b = [None, 1, 0, 1]; observe()
        code.extend(('local.get $b', 'i32.const 1', 'array.get $a'))
        observe([rf'\(ref {printed}\) = function module=0 index=1'])
        code.extend(('ref.cast (ref $f)', 'call_ref $f'))
        observe(['i32 = 29']); code.append('drop')

        code.append('elem.drop $elem'); observe()
        code.extend(('local.get $b', 'i32.const 4', 'i32.const 0', 'i32.const 0', 'array.init_elem $a $elem'))
        observe()
        code.extend(('i32.const 0', 'i32.const 0', 'array.new_elem $a $elem'))
        observe([reference_value], operand_array=[])
        code.extend(('local.set $empty', 'i32.const 2', 'local.get $empty', 'table.set $roots'))
        empty = []; observe()
        code.extend(('local.get $empty', 'array.len'))
        observe(['i32 = 0']); code.append('drop')
        code.extend(('drop', 'drop')); observe(final=True)
        cases.append(dict(name='gc-array-elem-' + ('typed-funcref' if typed else 'funcref'), expected=expected,
            wat='(module (global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0)) '
                f'(type $f (func (result i32))) (type $a (array (mut {reference}))) '
                '(table $roots 3 (ref null $a)) '
                '(func $leaf0 (type $f) i32.const 17) (func $leaf1 (type $f) i32.const 29) '
                f'(elem $elem {reference} (ref.func $leaf0) (ref.null {null}) (ref.func $leaf1) (ref.func $leaf0)) '
                '(func (export "_start") (local $a (ref null $a)) (local $b (ref null $a)) (local $empty (ref null $a)) '
                + ' '.join(code) + '))'))
    return cases
