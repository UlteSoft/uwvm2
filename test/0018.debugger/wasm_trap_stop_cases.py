"""Fixed inputs for real post-failure Wasm stops; independent expected values."""
from wasm_container_boundary_cases import dropped_elem_trap_examples


def trap_examples():
    cases = dropped_elem_trap_examples()
    prefix = ['i64 = -991', 'f32 = bits=0x80000000']
    def add(name, declarations, inputs, opcode, values, diagnostic, tables=()):
        point = dict(function=0, ordinal=0, values=prefix + values, caller=None,
                     locals_page=None, frame_total=None, tables=list(tables), members=[])
        cases.append(dict(name=name, expected=[point], diagnostic='Runtime crash (' + diagnostic + ')',
            wat='(module (global $p (mut i64) (i64.const -991)) '
                '(global $z (mut f32) (f32.const -0)) ' + declarations +
                ' (func (export "_start") global.get $p global.get $z ' + inputs +
                ' nop ' + opcode + ' unreachable))'))
    add('trap-unreachable', '', '', 'unreachable', [], 'catch unreachable')
    add('trap-divide-zero', '', 'i32.const -17 i32.const 0', 'i32.div_s',
        ['i32 = -17', 'i32 = 0'], 'integer divide by zero')
    add('trap-overflow', '', 'i64.const -9223372036854775808 i64.const -1', 'i64.div_s',
        ['i64 = -9223372036854775808', 'i64 = -1'], 'integer overflow')
    add('trap-memory-bounds', '(memory 1)', 'i32.const 65536', 'i32.load',
        ['i32 = 65536'], 'memory access out of bounds')
    for wide in (False, True):
        width = 'i64' if wide else 'i32'
        add('trap-empty-table-' + width, '(table ' + ('i64 ' if wide else '') + '0 funcref)',
            width + '.const 0', 'table.get 0', [width + ' = 0'], 'table access out of bounds',
            [dict(index=0, values=[])])
    add('trap-table-fill', '(table 1 funcref)', 'i32.const 1 ref.null func i32.const 1',
        'table.fill 0', ['i32 = 1', r'\(ref null func\) = null', 'i32 = 1'], 'table access out of bounds',
        [dict(index=0, values=[r'\(ref null func\) = null'])])
    add('trap-null-reference', '', 'ref.null func', 'ref.as_non_null',
        [r'\(ref null func\) = null'], 'null reference')
    shared = '(global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0))'
    for name, total, body in (
        ('trap-recursive-72', 72, '(func $recur (param i32) global.get $p global.get $z local.get 0 if '
         'local.get 0 i32.const 1 i32.sub call $recur else nop unreachable end drop drop) '
         '(func (export "_start") i32.const 70 call $recur)'),
        ('trap-tail-activation', 2, '(func $leaf global.get $p global.get $z nop unreachable) '
         '(func $middle return_call $leaf) '
         '(func (export "_start") global.get $p global.get $z call $middle drop drop)')):
        point = dict(function=0, ordinal=0, values=prefix, caller=prefix, locals_page=None,
                     frame_total=total, tables=[], members=[])
        cases.append(dict(name=name, expected=[point], diagnostic='Runtime crash (catch unreachable)',
                          wat='(module ' + shared + body + ')'))
    add('trap-atomic-alignment', '(memory 1 1 shared)', 'i32.const 1', 'i32.atomic.load',
        ['i32 = 1'], 'unaligned atomic memory access')
    return cases
