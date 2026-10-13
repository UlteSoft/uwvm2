"""Exact stack observations for every scalar atomic width, with 32/64-bit addresses.

The real runner assembles and independently validates each generated module,
then checks live typed stacks before/after the instructions and checks readback.
These cases do not establish multi-participant wait/notify scheduling support.
"""


def atomic_examples(mark):
    cases = []
    widths = [('i32', 32), ('i64', 64), ('i32', 8), ('i32', 16),
              ('i64', 8), ('i64', 16), ('i64', 32)]

    def carrier(kind, value):
        bits = 32 if kind == 'i32' else 64
        value &= (1 << bits) - 1
        if value >= 1 << (bits - 1):
            value -= 1 << bits
        return f'{kind} = {value}'

    for address in ('i32', 'i64'):
        code = []; expected = []; ordinal = 0; coverage = []
        prefix = ['i64 = 1000']
        address_value = f'{address} = 8'

        def observe(values):
            nonlocal ordinal
            code.append('nop')
            expected.append(mark(0, ordinal, prefix + values))
            ordinal += 1

        for kind, bits in widths:
            full_bits = 32 if kind == 'i32' else 64
            narrow = bits != full_bits
            mask = (1 << bits) - 1
            initial = -17 if kind == 'i32' else -29
            old = initial & mask
            load = f'{kind}.atomic.load' + (f'{bits}_u' if narrow else '')
            store = f'{kind}.atomic.store' + (str(bits) if narrow else '')

            def initialize():
                # Reset the entire scalar slot; narrow accesses prove truncation
                # and zero extension, full-width accesses prove signed display.
                code.extend([f'{address}.const 8', f'{kind}.const {initial}',
                             f'{kind}.store'])

            def readback(value):
                code.extend([f'{address}.const 8', load])
                observe([carrier(kind, value)])
                code.append('drop')

            initialize()
            code.append(f'{address}.const 8')
            observe([address_value])
            code.append(load); coverage.append(load)
            observe([carrier(kind, old)])
            code.append('drop')

            initialize()
            code.extend([f'{address}.const 8', f'{kind}.const -65'])
            observe([address_value, carrier(kind, -65)])
            code.append(store); coverage.append(store)
            observe([])
            readback(-65 & mask)

            for operation in ('add', 'sub', 'and', 'or', 'xor', 'xchg', 'cmpxchg'):
                # Test successful and unsuccessful comparisons independently.
                for matches in ((True, False) if operation == 'cmpxchg' else (True,)):
                    initialize(); operand = 101
                    code.extend([f'{address}.const 8'])
                    if operation == 'cmpxchg':
                        comparison = old if matches else old - 1
                        code.extend([f'{kind}.const {comparison}', f'{kind}.const {operand}'])
                        observe([address_value, carrier(kind, comparison), carrier(kind, operand)])
                        updated = operand if matches else old
                    else:
                        code.append(f'{kind}.const {operand}')
                        observe([address_value, carrier(kind, operand)])
                        updated = {'add': old + operand, 'sub': old - operand,
                                   'and': old & operand, 'or': old | operand,
                                   'xor': old ^ operand, 'xchg': operand}[operation]
                    opcode = f'{kind}.atomic.rmw' + (str(bits) if narrow else '')
                    opcode += f'.{operation}' + ('_u' if narrow else '')
                    code.append(opcode); coverage.append(opcode)
                    observe([carrier(kind, old)])
                    code.append('drop')
                    readback(updated & mask)

        # No live waiter is invented: mismatch/zero-deadline return codes and
        # notify without waiters are distinct from multi-thread wakeup tests.
        for kind, initial in (('i32', 13), ('i64', 29)):
            opcode = 'memory.atomic.wait' + ('32' if kind == 'i32' else '64')
            for matches in (True, False):
                code.extend([f'{address}.const 8', f'{kind}.const {initial}', f'{kind}.store',
                             f'{address}.const 8', f'{kind}.const {initial if matches else initial + 1}',
                             'i64.const 0'])
                observe([address_value, carrier(kind, initial if matches else initial + 1), 'i64 = 0'])
                code.append(opcode); coverage.append(opcode)
                observe(['i32 = ' + ('2' if matches else '1')])
                code.append('drop')
        for count in (0, 7):
            code.extend([f'{address}.const 8', f'i32.const {count}'])
            observe([address_value, f'i32 = {count}'])
            code.append('memory.atomic.notify'); coverage.append('memory.atomic.notify')
            observe(['i32 = 0']); code.append('drop')
        # Fence consumes no operands and leaves the caller prefix untouched.
        observe([]); code.append('atomic.fence'); coverage.append('atomic.fence'); observe([])
        code.append('drop')
        memory = 'i64 ' if address == 'i64' else ''
        cases.append(dict(name=f'atomic-all-widths-{address}-address',
            wat=f'(module (memory {memory}1 1 shared) '
                '(func (export "_start") i64.const 1000 ' + ' '.join(code) + '))',
            expected=expected, atomic_opcodes=sorted(set(coverage))))
    return cases
