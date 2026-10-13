#!/usr/bin/env python3
"""Generate small, verifiable Tail ABI cases without compiling on the local host."""
import json
from pathlib import Path
import sys

HEADER = '''target triple = "riscv64-unknown-linux-gnu"
declare void @probe_observe_stack()
declare void @probe_raise(i64) noreturn
declare i64 @probe_get_target() nounwind
attributes #0 = { noinline uwtable }
attributes #1 = { noinline uwtable "disable-tail-calls"="true" }
'''


def params(n, ty='i64'):
    return ', '.join(f'{ty} %a{i}' for i in range(n))


def args(values, ty='i64'):
    return ', '.join(f'{ty} {v}' for v in values)


def total(n, weighted=False, ty='i64'):
    lines = []
    values = [f'%a{i}' for i in range(n)]
    if weighted:
        for i in range(n):
            lines.append(f'%w{i} = mul i64 %a{i}, {i+1}')
        values = [f'%w{i}' for i in range(n)]
    last = values[0]
    for i in range(1, n):
        lines.append(f'%sum{i} = {"fadd" if ty == "double" else "add"} {ty} {last}, {values[i]}')
        last = f'%sum{i}'
    return '\n'.join(lines), last


def cases():
    for cc, name in [('', 'c-same12-control'), ('tailcc ', 'tail-same12-permute')]:
        body, result = total(12, True)
        yield name, f'''define {cc}i64 @sink({params(12)}) #0 {{
call void @probe_observe_stack()
{body}
ret i64 {result}
}}
define {cc}i64 @middle({params(12)}) #0 {{
call void @probe_observe_stack()
%r = musttail call {cc}i64 @sink({args([f'%a{i}' for i in reversed(range(12))])})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call {cc}i64 @middle({args(range(1,13))})
ret i64 %r
}}''', 364, 'return', not bool(cc)
    body, result = total(10)
    yield 'tail-0-to10-to2', f'''define tailcc i64 @two(i64 %a0, i64 %a1) #0 {{
call void @probe_observe_stack()
%r = add i64 %a0, %a1
ret i64 %r
}}
define tailcc i64 @ten({params(10)}) #0 {{
call void @probe_observe_stack()
{body}
%r = musttail call tailcc i64 @two(i64 {result}, i64 7)
ret i64 %r
}}
define tailcc i64 @zero() #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @ten({args(range(1,11))})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @zero()
ret i64 %r
}}''', 62, 'return', False
    body, result = total(12, True)
    yield 'tail-indirect2-to12', f'''define tailcc i64 @sink({params(12)}) #0 {{
call void @probe_observe_stack()
{body}
ret i64 {result}
}}
define tailcc i64 @middle(ptr %target, i64 %extra) #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 %target({args([1,2,3,4,5,6,7,8,9,10,11,'%extra'])})
ret i64 %r
}}
define i64 @run_case() #1 {{
%address = call i64 @probe_get_target()
; [safe: engine-owned executable sink] The mapped callback returns the entire
; finalized address, kept alive by the driver's owning ExecutionEngine.
%target = inttoptr i64 %address to ptr
%r = call tailcc i64 @middle(ptr %target, i64 12)
ret i64 %r
}}''', 650, 'return', False
    yield 'tail-deep2-to12-to2', f'''define tailcc i64 @two(i64 %n, i64 %acc) #0 {{
call void @probe_observe_stack()
%end = icmp eq i64 %n, 0
br i1 %end, label %done, label %again
done:
ret i64 %acc
again:
%next = sub i64 %n, 1
%sum = add i64 %acc, 1
%r = musttail call tailcc i64 @twelve(i64 %next, i64 %sum, {args(range(3,13))})
ret i64 %r
}}
define tailcc i64 @twelve(i64 %n, i64 %acc, {', '.join(f'i64 %pad{i}' for i in range(10))}) #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @two(i64 %n, i64 %acc)
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @two(i64 10000, i64 0)
ret i64 %r
}}''', 10000, 'return', False
    body, result = total(10, ty='double')
    yield 'tail-f64-register-overflow', f'''define tailcc i64 @sink({params(10,'double')}) #0 {{
call void @probe_observe_stack()
{body}
%r = fptoui double {result} to i64
ret i64 %r
}}
define tailcc i64 @zero() #0 {{
%r = musttail call tailcc i64 @sink({args([str(i)+'.0' for i in range(1,11)],'double')})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @zero()
ret i64 %r
}}''', 55, 'return', False
    yield 'tail-v128-new-computed', '''define tailcc i64 @sink(<4 x i32> %v) #0 {
call void @probe_observe_stack()
%a = extractelement <4 x i32> %v, i32 0
%b = extractelement <4 x i32> %v, i32 3
%x = mul i32 %a, 10
%y = add i32 %x, %b
%r = zext i32 %y to i64
ret i64 %r
}
define tailcc i64 @one(i32 %seed) #0 {
%v = insertelement <4 x i32> <i32 0, i32 2, i32 3, i32 4>, i32 %seed, i32 0
%r = musttail call tailcc i64 @sink(<4 x i32> %v)
ret i64 %r
}
define i64 @run_case() #1 {
%r = call tailcc i64 @one(i32 7)
ret i64 %r
}''', 74, 'return', False
    yield 'tail-v128-permute', '''define tailcc i64 @sink(<4 x i32> %a, <4 x i32> %b) #0 {
call void @probe_observe_stack()
%x = extractelement <4 x i32> %a, i32 0
%y = extractelement <4 x i32> %b, i32 3
%wx = mul i32 %x, 100
%s = add i32 %wx, %y
%r = zext i32 %s to i64
ret i64 %r
}
define tailcc i64 @middle(<4 x i32> %a, <4 x i32> %b) #0 {
%r = musttail call tailcc i64 @sink(<4 x i32> %b, <4 x i32> %a)
ret i64 %r
}
define i64 @run_case() #1 {
%r = call tailcc i64 @middle(<4 x i32> <i32 1, i32 2, i32 3, i32 4>, <4 x i32> <i32 5, i32 6, i32 7, i32 8>)
ret i64 %r
}''', 504, 'return', False
    body, result = total(12)
    yield 'tail-native-eh-backtrace', f'''define tailcc i64 @thrower({params(12)}) #0 {{
call void @probe_observe_stack()
{body}
call void @probe_raise(i64 {result})
unreachable
}}
define tailcc i64 @retired(i64 %seed) #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @thrower({args(range(1,13))})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @retired(i64 7)
ret i64 %r
}}''', 78, 'throw', False


def invalid_cases():
    yield 'c-mismatched-arity', 'declare i64 @callee(i64)\ndefine i64 @run_case() { %r = musttail call i64 @callee(i64 1)\nret i64 %r }', 'mismatched parameter counts'
    yield 'tail-mismatched-cc', 'declare i64 @callee()\ndefine tailcc i64 @run_case() { %r = musttail call i64 @callee()\nret i64 %r }', 'mismatched calling conv'
    yield 'tail-not-immediate-ret', 'declare tailcc i64 @callee()\ndefine tailcc i64 @run_case() { %r = musttail call tailcc i64 @callee()\n%x = add i64 %r, 1\nret i64 %x }', 'musttail call must precede a ret'
    yield 'tail-inreg', 'declare tailcc i64 @callee(i64 inreg)\ndefine tailcc i64 @run_case() { %r = musttail call tailcc i64 @callee(i64 inreg 1)\nret i64 %r }', 'inreg attribute not allowed'
    yield 'tail-varargs', 'declare tailcc i64 @callee(...)\ndefine tailcc i64 @run_case(...) { %r = musttail call tailcc i64 (...) @callee(...)\nret i64 %r }', 'tail call for varargs'


def main():
    out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=False);rows=[]
    valid_cases=list(cases())
    same12=next(row for row in valid_cases if row[0]=='c-same12-control')
    fast_vector=next(row for row in valid_cases if row[0]=='tail-v128-permute')
    fast_new=next(row for row in valid_cases if row[0]=='tail-v128-new-computed')
    valid_cases.insert(0, ('c-same2-control', """define i64 @sink(i64 %x, i64 %y) #0 {
call void @probe_observe_stack()
%r = add i64 %x, %y
ret i64 %r
}
define i64 @middle(i64 %x, i64 %y) #0 {
call void @probe_observe_stack()
%r = musttail call i64 @sink(i64 %y, i64 %x)
ret i64 %r
}
define i64 @run_case() #1 {
%r = call i64 @middle(i64 1, i64 2)
ret i64 %r
}""", 3, 'return', True))
    valid_cases.append(('c-indirect2-control', """define i64 @sink(i64 %a, i64 %b) #0 {
call void @probe_observe_stack()
%sum = add i64 %a, %b
ret i64 %sum
}
define i64 @run_case() #1 {
%address = call i64 @probe_get_target()
; [safe: engine-owned executable sink] The host callback preserves all address bits.
%target = inttoptr i64 %address to ptr
%result = call i64 %target(i64 1, i64 2)
ret i64 %result
}""",3,'return',True))
    valid_cases += [
        ('c-same12-normal-call',same12[1].replace('musttail call','call'),364,'return',True),
        ('c-same12-without-local-frame',same12[1].replace('call void @probe_observe_stack()',''),364,'return',True),
        ('fast-v128-permute',fast_vector[1].replace('tailcc ','fastcc '),504,'return',True),
        ('fast-v128-new-normal-call',fast_new[1].replace('tailcc ','fastcc ').replace('musttail call','call'),74,'return',True),
    ]
    # Tail may use FastCC's twelve GPRs and twenty FPRs. These larger
    # signatures must reach actual stack slots even with that register budget.
    body18,result18=total(18)
    valid_cases.append(('tail-0-to18-to2', f"""define tailcc i64 @two(i64 %a0, i64 %a1) #0 {{
call void @probe_observe_stack()
%r = add i64 %a0, %a1
ret i64 %r
}}
define tailcc i64 @eighteen({params(18)}) #0 {{
call void @probe_observe_stack()
{body18}
%r = musttail call tailcc i64 @two(i64 {result18}, i64 7)
ret i64 %r
}}
define tailcc i64 @zero() #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @eighteen({args(range(1,19))})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @zero()
ret i64 %r
}}""",178,'return',False))
    valid_cases.append(('tail-stack18-typed-landingpad', f"""@probe_exception_typeinfo = external constant i8
declare i32 @__gxx_personality_v0(...)
declare i32 @llvm.eh.typeid.for.p0(ptr)
declare ptr @__cxa_begin_catch(ptr) nounwind
declare void @__cxa_end_catch() nounwind
define tailcc i64 @thrower({params(18)}) #0 {{
call void @probe_observe_stack()
{body18}
call void @probe_raise(i64 {result18})
unreachable
}}
define tailcc i64 @zero() #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @thrower({args(range(1,19))})
ret i64 %r
}}
define i64 @after_catch(i64 %payload, ptr %first, ptr %last) #1 {{
; [safe: caller-owned 32-element array] Both pointers name initialized elements.
%a = load volatile i64, ptr %first, align 8
%b = load volatile i64, ptr %last, align 8
%ok_a = icmp eq i64 %a, 123456789
%ok_b = icmp eq i64 %b, 987654321
%ok = and i1 %ok_a, %ok_b
%value = add i64 %payload, 17
%result = select i1 %ok, i64 %value, i64 0
ret i64 %result
}}
define i64 @run_case() #1 personality ptr @__gxx_personality_v0 {{
%storage = alloca [32 x i64], align 16
; [safe: storage[0..32)] Constant indices 0 and 31 stay within the full allocation.
%first = getelementptr inbounds [32 x i64], ptr %storage, i64 0, i64 0
; [safe: storage[0..32)] The last element is 31, never the one-past address.
%last = getelementptr inbounds [32 x i64], ptr %storage, i64 0, i64 31
store volatile i64 123456789, ptr %first, align 8
store volatile i64 987654321, ptr %last, align 8
%r = invoke tailcc i64 @zero() to label %unexpected unwind label %dispatch
unexpected:
ret i64 0
dispatch:
%exception = landingpad {{ ptr, i32 }} cleanup catch ptr @probe_exception_typeinfo
%pointer = extractvalue {{ ptr, i32 }} %exception, 0
%selector = extractvalue {{ ptr, i32 }} %exception, 1
%wanted = call i32 @llvm.eh.typeid.for.p0(ptr @probe_exception_typeinfo)
%matches = icmp eq i32 %selector, %wanted
br i1 %matches, label %caught, label %foreign
foreign:
resume {{ ptr, i32 }} %exception
caught:
%object = call ptr @__cxa_begin_catch(ptr %pointer)
; [safe: exact probe_exception activation] The typed selector is checked above;
; begin_catch adjusts to the object, whose sole uint64_t member starts at offset 0.
%payload = load i64, ptr %object, align 8
call void @__cxa_end_catch()
%result = call i64 @after_catch(i64 %payload, ptr %first, ptr %last)
ret i64 %result
}}""",188,'caught',False))
    deep=next(row for row in valid_cases if row[0]=='tail-deep2-to12-to2')[1]
    deep=deep.replace('@twelve','@twenty').replace(args(range(3,13)),args(range(3,21))).replace(', '.join(f'i64 %pad{i}' for i in range(10)),', '.join(f'i64 %pad{i}' for i in range(18)))
    valid_cases.append(('tail-deep2-to20-to2',deep,10000,'return',False))
    body24,result24=total(24,ty='double')
    valid_cases.append(('tail-f64-stack24',f"""define tailcc i64 @sink({params(24,'double')}) #0 {{
call void @probe_observe_stack()
{body24}
%r = fptoui double {result24} to i64
ret i64 %r
}}
define tailcc i64 @zero() #0 {{
%r = musttail call tailcc i64 @sink({args([str(i)+'.0' for i in range(1,25)],'double')})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @zero()
ret i64 %r
}}""",300,'return',False))
    vector4="""define tailcc i64 @sink(<4 x i32> %a, <4 x i32> %b, <4 x i32> %c, <4 x i32> %d) #0 {
call void @probe_observe_stack()
%x = extractelement <4 x i32> %a, i32 0
%y = extractelement <4 x i32> %d, i32 3
%wx = mul i32 %x, 100
%s = add i32 %wx, %y
%r = zext i32 %s to i64
ret i64 %r
}
define tailcc i64 @middle(<4 x i32> %a, <4 x i32> %b, <4 x i32> %c, <4 x i32> %d) #0 {
call void @probe_observe_stack()
%r = musttail call tailcc i64 @sink(<4 x i32> %d, <4 x i32> %c, <4 x i32> %b, <4 x i32> %a)
ret i64 %r
}
define i64 @run_case() #1 {
%r = call tailcc i64 @middle(<4 x i32> <i32 1, i32 2, i32 3, i32 4>, <4 x i32> <i32 5, i32 6, i32 7, i32 8>, <4 x i32> <i32 9, i32 10, i32 11, i32 12>, <4 x i32> <i32 13, i32 14, i32 15, i32 16>)
ret i64 %r
}"""
    valid_cases.append(('tail-four-v128-permute',vector4,1304,'return',False))
    stack_catch=next(row[1] for row in valid_cases if row[0]=='tail-stack18-typed-landingpad')
    direct_catch=stack_catch.replace('invoke tailcc i64 @zero()',f'invoke tailcc i64 @thrower({args(range(1,19))})')
    valid_cases.append(('tail-invoke18-typed-landingpad',direct_catch,188,'caught',False))
    normal_stack=direct_catch.replace(f'call void @probe_raise(i64 {result18})\nunreachable',f'ret i64 {result18}')
    normal_stack=normal_stack.replace('unexpected:\nret i64 0','unexpected:\n%normal = call i64 @after_catch(i64 %r, ptr %first, ptr %last)\nret i64 %normal')
    valid_cases.append(('tail-call18-caller-stack',normal_stack,188,'return',False))
    body20,result20=total(20,True)
    valid_cases.append(('tail-same20-stack-permute',f"""define tailcc i64 @sink({params(20)}) #0 {{
call void @probe_observe_stack()
{body20}
ret i64 {result20}
}}
define tailcc i64 @middle({params(20)}) #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @sink({args([f'%a{i}' for i in reversed(range(20))])})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @middle({args(range(1,21))})
ret i64 %r
}}""",1540,'return',False))
    large_sum,large_result=total(600)
    valid_cases.append(('tail-large-0-to600-to2',f"""define tailcc i64 @two(i64 %x, i64 %y) #0 {{
call void @probe_observe_stack()
%r = add i64 %x, %y
ret i64 %r
}}
define tailcc i64 @many({params(600)}) #0 {{
call void @probe_observe_stack()
{large_sum}
%r = musttail call tailcc i64 @two(i64 {large_result}, i64 7)
ret i64 %r
}}
define tailcc i64 @zero() #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @many({args(range(1,601))})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @zero()
ret i64 %r
}}""",180307,'return',False))
    large_weighted,large_weighted_result=total(600,True)
    valid_cases.append(('tail-large-same600-stack-permute',f"""define tailcc i64 @sink({params(600)}) #0 {{
call void @probe_observe_stack()
{large_weighted}
ret i64 {large_weighted_result}
}}
define tailcc i64 @middle({params(600)}) #0 {{
call void @probe_observe_stack()
%r = musttail call tailcc i64 @sink({args([f'%a{i}' for i in reversed(range(600))])})
ret i64 %r
}}
define i64 @run_case() #1 {{
%r = call tailcc i64 @middle({args(range(1,601))})
ret i64 %r
}}""",36180200,'return',False))
    for name,body,expected,kind,baseline in valid_cases:
        (out/(name+'.ll')).write_text(HEADER+body+'\n')
        rows.append(dict(name=name,valid=True,expected=expected,kind=kind,baseline=baseline,maximum_stack_span=65536 if name.startswith('tail-large') else 4096,minimum_observations=(20001 if name.startswith("tail-deep") else 0 if name=="c-same12-without-local-frame" else 1),baseline_failure=('FAIL value=710 expected=364' if name=='c-same12-control' else None)))
    for name,body,diagnostic in invalid_cases():
        (out/(name+'.ll')).write_text(HEADER+body+'\n')
        rows.append(dict(name=name,valid=False,diagnostic=diagnostic))
    (out/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')


if __name__=='__main__':main()
