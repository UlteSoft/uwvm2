"""Independent live table/GC member oracles for the real paused Wasm CLI."""
from wasm_operand_bulk_state_cases import bulk_state_examples


def live_container_examples():
    cases = []
    for case in bulk_state_examples():
        if not case['name'].startswith('bulk-table-'):
            continue
        assert len(case['expected']) == 38
        for point in case['expected']:
            n = point['ordinal']
            src = [None, None, None, None] if n == 0 else [None, 0, 1, None]
            dst = ([None] * 4 if n < 5 else [0, 1, None, None] if n < 10
                   else [0, 0, 1, None] if n < 16 else [0, 0, 1, 1] if n < 22
                   else [None, 0, 1, 1] if n < 25 else [None, 0, 1, 1, 0])
            def values(items):
                return [r'\(ref null func\) = ' + ('null' if item is None
                        else f'function module=0 index={item}') for item in items]
            point['tables'] = [dict(index=0, values=values(src)), dict(index=1, values=values(dst))]
            point['members'] = []
        cases.append(case)

    prefix = ['i64 = -991', 'f32 = bits=0x80000000']
    seed = '(global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0))'
    for bits in (8, 16):
        mask = (1 << bits) - 1
        code = ['global.get $p', 'global.get $z']
        expected = []
        a = [0, 127, 128, 511 & mask]
        b = None

        def observe(extra=(), final=False):
            def page(root, values, selection='locals'):
                return dict(selection=selection, root=root, table=0, path=[], kind='array',
                            type_index=0, values=[f'i32 = packed-u{bits}={v} mutable' for v in values])
            pages = [page(0, a), page(0, a, 'table')]
            if b is not None:
                pages.append(page(1, b))
            code.append('nop')
            expected.append(dict(function=0, ordinal=len(expected), values=[] if final else prefix + list(extra),
                caller=None, locals_page=None, frame_total=None, tables=[], members=pages))

        code.extend(('i32.const 0', 'i32.const 127', 'i32.const 128', 'i32.const 511',
                     'array.new_fixed $a 4', 'local.set $a',
                     'i32.const 0', 'local.get $a', 'table.set $roots'))
        observe()
        code.extend(('local.get $a', 'i32.const 1', 'i32.const -1', 'array.set $a'))
        a = [0, mask, 128, 511 & mask]; observe()
        code.extend(('local.get $a', 'i32.const 2', 'i32.const 4660', 'i32.const 2', 'array.fill $a'))
        a = [0, mask, 4660 & mask, 4660 & mask]; observe()
        code.extend(('local.get $a', 'i32.const 1', 'local.get $a', 'i32.const 0',
                     'i32.const 3', 'array.copy $a $a'))
        a = [0, 0, mask, 4660 & mask]; observe()
        code.extend(('i32.const 0', 'i32.const 4', 'array.new_data $a $data', 'local.set $b'))
        b = [0, 52, 18, 255] if bits == 8 else [13312, 65298, 65408, 32766]
        observe()
        code.extend(('local.get $a', 'i32.const 1', 'i32.const 4', 'i32.const 2', 'array.init_data $a $data'))
        a = [0, 128 if bits == 8 else 65408, 255 if bits == 8 else 32766, 4660 & mask]
        observe()
        code.extend(('local.get $a', 'i32.const 1', 'array.get_s $a'))
        observe(['i32 = -128']); code.append('drop')
        code.extend(('local.get $a', 'i32.const 1', 'array.get_u $a'))
        observe([f'i32 = {a[1]}']); code.append('drop')
        code.extend(('data.drop $data', 'local.get $a', 'i32.const 4', 'i32.const 0',
                     'i32.const 0', 'array.init_data $a $data'))
        observe(); code.extend(('drop', 'drop')); observe(final=True)
        cases.append(dict(name=f'gc-packed-i{bits}-live-members', expected=expected,
            wat=f'(module {seed} (type $a (array (mut i{bits}))) '
                '(table $roots 1 (ref null $a)) (data $data "\\00\\34\\12\\ff\\80\\ff\\fe\\7f") '
                '(func (export "_start") (local $a (ref null $a)) (local $b (ref null $a)) '
                + ' '.join(code) + '))'))

    code = ['global.get $p', 'global.get $z', 'i32.const 7', 'struct.new $node', 'local.set $node',
            'local.get $node', 'ref.null $node', 'local.get $node', 'array.new_fixed $refs 3',
            'local.set $refs', 'i32.const 0', 'local.get $refs', 'table.set $roots']
    expected = []
    def reference_observe(value, slots, final=False):
        code.append('nop')
        pages = []
        for selection, root in (('locals', 1), ('table', 0)):
            pages.append(dict(selection=selection, root=root, table=0, path=[], kind='array', type_index=1,
                values=[r'\(ref null type-index=0 module=0\) = ' +
                        ('struct #2' if item else 'null') + ' mutable' for item in slots]))
            for index, item in enumerate(slots):
                if item:
                    pages.append(dict(selection=selection, root=root, table=0, path=[index],
                        kind='struct', type_index=0, values=[f'i32 = {value} mutable']))
        expected.append(dict(function=0, ordinal=len(expected), values=[] if final else prefix,
            caller=None, locals_page=None, frame_total=None, tables=[], members=pages))
    reference_observe(7, [True, False, True])
    code.extend(('local.get $node', 'i32.const -93', 'struct.set $node 0'))
    reference_observe(-93, [True, False, True])
    code.extend(('local.get $refs', 'i32.const 1', 'ref.null $node', 'i32.const 2', 'array.fill $refs'))
    reference_observe(-93, [True, False, False])
    code.extend(('local.get $refs', 'i32.const 2', 'local.get $node', 'array.set $refs'))
    reference_observe(-93, [True, False, True])
    code.extend(('drop', 'drop')); reference_observe(-93, [True, False, True], final=True)
    cases.append(dict(name='gc-shared-reference-live-members', expected=expected,
        wat=f'(module {seed} (type $node (struct (field (mut i32)))) '
            '(type $refs (array (mut (ref null $node)))) (table $roots 1 (ref null $refs)) '
            '(func (export "_start") (local $node (ref null $node)) (local $refs (ref null $refs)) '
            + ' '.join(code) + '))'))
    return cases
