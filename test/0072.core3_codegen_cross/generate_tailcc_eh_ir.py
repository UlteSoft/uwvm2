#!/usr/bin/env python3
"""Typed native unwind after TailCC argument-area growth, plus ordinary-call controls."""
import argparse
import hashlib
import json
from pathlib import Path
from generate_tailcc_ir import probe, preamble, entry_result_check


def fixture(kind):
    parameters = ', '.join(f'i64 %a{i}' for i in range(18))
    arguments = ', '.join(f'i64 {i + 1}' for i in range(18))
    text = preamble(False) + '''@tailcc_exception_typeinfo = external constant i8
declare void @tailcc_raise(i64) noreturn
declare i32 @__gxx_personality_v0(...)
declare i32 @llvm.eh.typeid.for.p0(ptr)
declare ptr @__cxa_begin_catch(ptr) nounwind
declare void @__cxa_end_catch() nounwind
'''
    if kind != 'return':
        text += '@tailcc_expects_raise = internal constant i8 1\n'
    text += f'define tailcc i64 @wide({parameters}) noinline uwtable {{\n' + probe()
    last = '%a0'
    for i in range(1, 18):
        text += f'  %sum{i} = add i64 {last}, %a{i}\n'
        last = f'%sum{i}'
    text += f'  ret i64 {last}\n}}\n' if kind == 'return' else f'  call void @tailcc_raise(i64 {last})\n  unreachable\n}}\n'
    text += 'define tailcc i64 @small() noinline uwtable {\n' + probe()
    text += f'  %result = musttail call tailcc i64 @wide({arguments})\n  ret i64 %result\n}}\n'
    text += '''define i64 @after_catch(i64 %payload, ptr %first, ptr %last) noinline nounwind uwtable "disable-tail-calls"="true" {
  %a = load volatile i64, ptr %first, align 8
  %b = load volatile i64, ptr %last, align 8
  %ok_a = icmp eq i64 %a, 123456789
  %ok_b = icmp eq i64 %b, 987654321
  %ok_value = icmp eq i64 %payload, 171
  %ok_ab = and i1 %ok_a, %ok_b
  %ok = and i1 %ok_ab, %ok_value
  %bad = xor i1 %ok, true
  %status = zext i1 %bad to i64
  ret i64 %status
}
define i64 @entry() noinline uwtable "disable-tail-calls"="true" personality ptr @__gxx_personality_v0 {
  %storage = alloca [32 x i64], align 16
  %first = getelementptr inbounds [32 x i64], ptr %storage, i64 0, i64 0
  %last = getelementptr inbounds [32 x i64], ptr %storage, i64 0, i64 31
  store volatile i64 123456789, ptr %first, align 8
  store volatile i64 987654321, ptr %last, align 8
'''
    call = '@small()' if kind == 'grow-throw' else f'@wide({arguments})'
    text += f'  %value = invoke tailcc i64 {call} to label %normal unwind label %dispatch\nnormal:\n'
    if kind == 'return':
        text += '  %result = call i64 @after_catch(i64 %value, ptr %first, ptr %last)\n'
        text += entry_result_check('%result', 0).replace('}\n', '', 1)
    else:
        text += '  ret i64 1\n'
    text += '''dispatch:
  %exception = landingpad { ptr, i32 } cleanup catch ptr @tailcc_exception_typeinfo
  %pointer = extractvalue { ptr, i32 } %exception, 0
  %selector = extractvalue { ptr, i32 } %exception, 1
  %wanted = call i32 @llvm.eh.typeid.for.p0(ptr @tailcc_exception_typeinfo)
  %matches = icmp eq i32 %selector, %wanted
  br i1 %matches, label %caught, label %foreign
foreign:
  resume { ptr, i32 } %exception
caught:
  %object = call ptr @__cxa_begin_catch(ptr %pointer)
  %payload = load i64, ptr %object, align 8
  call void @__cxa_end_catch()
  %caught_result = call i64 @after_catch(i64 %payload, ptr %first, ptr %last)
'''
    if kind == 'return':
        text += '  ret i64 1\n}\n'
    else:
        text += entry_result_check('%caught_result', 0)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False)
    rows = []
    for kind in ('grow-throw', 'direct-throw', 'return'):
        path = args.output / (kind + '.ll')
        path.write_text(fixture(kind))
        rows.append(dict(name=kind, path=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                         throws=int(kind != 'return'), stack_span_limit_bytes=4096))
    (args.output / 'fixtures.json').write_text(json.dumps(dict(fixtures=rows,
        scope='Generated IR only; actual typed landing pad, caller canaries, post-catch call, CFI and restored SP require native execution'), indent=2) + '\n')


if __name__ == '__main__':
    main()
