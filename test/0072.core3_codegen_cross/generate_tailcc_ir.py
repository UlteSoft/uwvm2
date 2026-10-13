#!/usr/bin/env python3
"""Private TailCC stress IR; native MCJIT execution qualifies each derivative."""
import argparse
import hashlib
import json
from pathlib import Path

HOPS = 2_000_000
ATTR = 'noinline nounwind uwtable'
MASK = (1 << 64) - 1


def probe():
    # A volatile stack byte preserves a real, distinct frame address. The
    # extrema cover the native tail functions, including their argument areas.
    return '''  %stackbyte = alloca i8, align 1
  store volatile i8 0, ptr %stackbyte
  %stackaddr = ptrtoint ptr %stackbyte to i64
  %oldlow = load volatile i64, ptr @stacklow
  %oldhigh = load volatile i64, ptr @stackhigh
  %below = icmp ult i64 %stackaddr, %oldlow
  %above = icmp ugt i64 %stackaddr, %oldhigh
  %newlow = select i1 %below, i64 %stackaddr, i64 %oldlow
  %newhigh = select i1 %above, i64 %stackaddr, i64 %oldhigh
  store volatile i64 %newlow, ptr @stacklow
  store volatile i64 %newhigh, ptr @stackhigh
'''


def preamble(indirect):
    text = '''target triple = "loongarch64-unknown-linux-gnu"
@stacklow = internal global i64 -1
@stackhigh = internal global i64 0
declare void @llvm.trap() cold noreturn nounwind
'''
    if indirect:
        text += '@nextwide = internal global ptr @wide\n@nextsmall = internal global ptr @small\n'
    return text


def target(name, indirect):
    return (f'  %target = load volatile ptr, ptr @next{name}\n', '%target') if indirect else ('', '@' + name)


def check_lines(comparisons):
    lines = []
    for i, (kind, lhs, rhs) in enumerate(comparisons):
        lines.append(f'  %bad{i} = icmp ne {kind} {lhs}, {rhs}\n')
        if i:
            prev = '%bad0' if i == 1 else f'%anybad{i - 1}'
            lines.append(f'  %anybad{i} = or i1 {prev}, %bad{i}\n')
    condition = '%bad0' if len(comparisons) == 1 else f'%anybad{len(comparisons) - 1}'
    lines.append(f'  br i1 {condition}, label %fail, label %checked\n'
                 'fail:\n  call void @llvm.trap()\n  unreachable\nchecked:\n')
    return ''.join(lines)


def entry_result_check(result, expected):
    return f'''  %low = load volatile i64, ptr @stacklow
  %high = load volatile i64, ptr @stackhigh
  %span = sub i64 %high, %low
  %grew = icmp ugt i64 %span, 4096
  %wrong = icmp ne i64 {result}, {expected}
  %failed = or i1 %grew, %wrong
  %status = zext i1 %failed to i64
  ret i64 %status
}}
'''


def rotate():
    values = [((i + 1) * 0x1020304050607081) & MASK for i in range(24)]
    signature = ', '.join(['i64 %n'] + [f'i64 %v{i}' for i in range(24)])
    text = preamble(False)
    for name, other in [('wide', 'small'), ('small', 'wide')]:
        text += f'define tailcc i64 @{name}({signature}) {ATTR} {{\nstart:\n' + probe()
        text += '  %done = icmp eq i64 %n, 0\n  br i1 %done, label %last, label %again\nlast:\n'
        expected = [values[(i + HOPS) % 24] for i in range(24)]
        text += check_lines([('i64', f'%v{i}', v) for i, v in enumerate(expected)])
        text += '  ret i64 0\nagain:\n  %next = sub i64 %n, 1\n'
        arguments = ', '.join(['i64 %next'] + [f'i64 %v{(i + 1) % 24}' for i in range(24)])
        text += f'  %result = musttail call tailcc i64 @{other}({arguments})\n  ret i64 %result\n}}\n'
    arguments = ', '.join([f'i64 {HOPS}'] + [f'i64 {v}' for v in values])
    text += f'define i64 @entry() {ATTR} {{\n  %result = call tailcc i64 @wide({arguments})\n'
    return text + entry_result_check('%result', 0), dict(name='rotate-spilled-24', tail_edges=HOPS, checked_fields=24)


def grow_shrink(indirect):
    constants = [((i + 1) * 0x1F2345678910203) & MASK for i in range(23)]
    signature = ', '.join(['i64 %n', 'i64 %acc'] + [f'i64 %v{i}' for i in range(23)])
    text = preamble(indirect)
    text += f'define tailcc i64 @wide({signature}) {ATTR} {{\nstart:\n' + probe()
    for i, constant in enumerate(constants):
        text += f'  %expected{i} = xor i64 %n, {constant}\n'
    text += check_lines([('i64', f'%v{i}', f'%expected{i}') for i in range(23)])
    text += '''  %done = icmp eq i64 %n, 0
  br i1 %done, label %last, label %again
last:
  ret i64 %acc
again:
  %next = sub i64 %n, 1
  %accnext = add i64 %acc, %n
'''
    load, callee = target('small', indirect)
    text += load + f'  %result = musttail call tailcc i64 {callee}(i64 %next, i64 %accnext)\n  ret i64 %result\n}}\n'
    text += f'define tailcc i64 @small(i64 %n, i64 %acc) {ATTR} {{\nstart:\n' + probe()
    for i, constant in enumerate(constants):
        text += f'  %v{i} = xor i64 %n, {constant}\n'
    load, callee = target('wide', indirect)
    arguments = ', '.join(['i64 %n', 'i64 %acc'] + [f'i64 %v{i}' for i in range(23)])
    text += load + f'  %result = musttail call tailcc i64 {callee}({arguments})\n  ret i64 %result\n}}\n'
    initial = ', '.join([f'i64 {HOPS}', 'i64 0'] + [f'i64 {constant ^ HOPS}' for constant in constants])
    text += f'define i64 @entry() {ATTR} {{\n  %result = call tailcc i64 @wide({initial})\n'
    expected = HOPS * (HOPS + 1) // 2
    return text + entry_result_check('%result', expected), dict(name='grow-shrink-' + ('indirect' if indirect else 'direct'), tail_edges=2 * HOPS, checked_fields=23)


def mixed(indirect):
    # More values than each native argument register bank; zero/NaN payloads
    # are transported as bits and cannot be accidentally compared as floats.
    f32 = [0x80000000, 1, 0x7FC12345, 0x7F800000, 0x3FC00000, 0xFFC54321] * 2
    f64 = [0x8000000000000000, 1, 0x7FF8123456789ABC, 0x7FF0000000000000,
           0x3FF8000000000000, 0xFFF8543210987654] * 2
    ints = [((i + 1) * 0x1234567891234567) & MASK for i in range(24)]
    vectors = [[(i * 31 + lane * 17) & 255 for lane in range(16)] for i in range(12)]
    vector_text = lambda lanes: '<' + ', '.join(f'i8 {lane}' for lane in lanes) + '>'
    params = ['ptr %out', 'i64 %n'] + [f'float %f{i}' for i in range(12)] + [f'double %d{i}' for i in range(12)] + [f'<16 x i8> %x{i}' for i in range(12)] + [f'i64 %v{i}' for i in range(24)]
    actual = ['ptr %out', 'i64 %n'] + [f'float bitcast (i32 {v} to float)' for v in f32] + [f'double bitcast (i64 {v} to double)' for v in f64] + [f'<16 x i8> {vector_text(v)}' for v in vectors] + [f'i64 {v}' for v in ints]
    text = preamble(indirect)
    text += f'define tailcc void @wide({", ".join(params)}) {ATTR} {{\nstart:\n' + probe()
    comparisons = []
    for i, value in enumerate(f32):
        text += f'  %fbit{i} = bitcast float %f{i} to i32\n'
        comparisons.append(('i32', f'%fbit{i}', value))
    for i, value in enumerate(f64):
        text += f'  %dbit{i} = bitcast double %d{i} to i64\n'
        comparisons.append(('i64', f'%dbit{i}', value))
    for i, vector in enumerate(vectors):
        for lane, value in enumerate(vector):
            text += f'  %lane{i}_{lane} = extractelement <16 x i8> %x{i}, i32 {lane}\n'
            comparisons.append(('i8', f'%lane{i}_{lane}', value))
    comparisons += [('i64', f'%v{i}', value) for i, value in enumerate(ints)]
    text += check_lines(comparisons)
    text += '  %done = icmp eq i64 %n, 0\n  br i1 %done, label %last, label %again\nlast:\n'
    # Match the runtime's explicit result-address ABI rather than native sret.
    results = [('i64', f'%v{i}', ints[i]) for i in range(24)] + [('i32', f'%fbit{i}', f32[i]) for i in range(4)] + [('i64', f'%dbit{i}', f64[i]) for i in range(3)]
    for i, (kind, value, _) in enumerate(results):
        text += f'  %field{i} = getelementptr i8, ptr %out, i64 {i * 8}\n  store {kind} {value}, ptr %field{i}, align 8\n'
    text += '  ret void\nagain:\n  %next = sub i64 %n, 1\n'
    load, callee = target('small', indirect)
    text += load + f'  musttail call tailcc void {callee}(ptr %out, i64 %next)\n  ret void\n}}\n'
    text += f'define tailcc void @small(ptr %out, i64 %n) {ATTR} {{\nstart:\n' + probe()
    load, callee = target('wide', indirect)
    text += load + f'  musttail call tailcc void {callee}({", ".join(actual)})\n  ret void\n}}\n'
    initial = list(actual)
    initial[0], initial[1] = 'ptr %out', f'i64 {HOPS}'
    text += f'define i64 @entry() {ATTR} {{\n  %out = alloca [248 x i8], align 16\n  call tailcc void @wide({", ".join(initial)})\n'
    for i, (kind, _, _) in enumerate(results):
        text += f'  %field{i} = getelementptr i8, ptr %out, i64 {i * 8}\n  %value{i} = load {kind}, ptr %field{i}, align 8\n'
    text += check_lines([(kind, f'%value{i}', expected) for i, (kind, _, expected) in enumerate(results)])
    text += '  %result = add i64 0, 0\n'
    return text + entry_result_check('%result', 0), dict(name='mixed-spills-31-results-' + ('indirect' if indirect else 'direct'), tail_edges=2 * HOPS, checked_parameter_bits=240, checked_result_fields=31)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('output', type=Path)
    a = ap.parse_args()
    a.output.mkdir(exist_ok=False)
    fixtures = []
    for text, row in [rotate(), grow_shrink(False), grow_shrink(True), mixed(False), mixed(True)]:
        path = a.output / (row['name'] + '.ll')
        path.write_text(text)
        fixtures.append(row | dict(path=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest(), stack_span_limit_bytes=4096))
    (a.output / 'fixtures.json').write_text(json.dumps(dict(fixtures=fixtures,
        scope='Generated IR only; native TailCC execution, stack restoration and object assembly need independent qualification'), indent=2) + '\n')


if __name__ == '__main__':
    main()
