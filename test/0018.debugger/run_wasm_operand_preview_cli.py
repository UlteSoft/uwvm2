#!/usr/bin/env python3
"""Check exact live Wasm operand stacks through the real CLI in the Linux cgroup."""
import argparse
import hashlib
import json
import os
import re
import subprocess
import time
from pathlib import Path
from run_wasm_opcode_step_cli import Console
from wasm_operand_atomic_cases import atomic_examples
from wasm_operand_relaxed_simd_cases import relaxed_simd_examples


class OperandConsole(Console):
    prompt_timeout = 120

    def prompt(self):
        # Deep live-stack instrumentation can take longer to compile on the
        # shared host. Commands remain bounded and the outer keeper owns the
        # process tree and enforces its resource/deadline limits.
        marker=b'(uwvm-debug) ';deadline=time.monotonic()+self.prompt_timeout
        while marker not in self.pending:
            assert self.selector.select(max(0,deadline-time.monotonic())),self.transcript[-4000:]
            chunk=os.read(self.child.stdout.fileno(),65536)
            assert chunk,(self.child.poll(),self.transcript[-4000:])
            self.pending.extend(chunk);self.transcript.extend(chunk)
        end=self.pending.index(marker)+len(marker)
        reply=bytes(self.pending[:end]);del self.pending[:end]
        return reply


def examples():
    seed = '(global $x (mut i32) (i32.const 37)) (global $y (mut i64) (i64.const 1000))'
    cases = []
    def add(name, wat, expected):
        cases.append(dict(name=name, wat=wat, expected=expected))
    def mark(function, ordinal, values, caller=None, locals_page=None, frame_total=None):
        return dict(function=function, ordinal=ordinal, values=values, caller=caller,
                    locals_page=locals_page, frame_total=frame_total)
    mixed = '''(module
      (global $x (mut i32) (i32.const -37))
      (global $y (mut i64) (i64.const -9223372036854775785))
      (global $f (mut f32) (f32.const -0))
      (global $d (mut f64) (f64.const 1.5))
      (func (export "_start")
        global.get $x global.get $y global.get $f global.get $d nop
        f64.const 2 f64.mul nop
        drop drop i64.const 7 i64.add nop
        drop i32.const 100 i32.add nop drop nop
        i32.const 0x7fc12345 f32.reinterpret_i32 nop
        i64.const 0x7ff8123456789abc f64.reinterpret_i64 nop
        v128.const i32x4 1 -2 3 4 nop drop drop drop nop))'''
    a=['i32 = -37','i64 = -9223372036854775785','f32 = bits=0x80000000']
    nan=['f32 = bits=0x7fc12345','f64 = bits=0x7ff8123456789abc']
    add('mixed-numeric-bits',mixed,[mark(0,0,a+['f64 = bits=0x3ff8000000000000']),
        mark(0,1,a+['f64 = bits=0x4008000000000000']),
        mark(0,2,['i32 = -37','i64 = -9223372036854775778']),mark(0,3,['i32 = 63']),mark(0,4,[]),
        mark(0,5,nan[:1]),mark(0,6,nan),mark(0,7,nan+['v128 = bytes=01000000feffffff0300000004000000']),mark(0,8,[])])
    add('gc-reference-kinds','''(module
      (type $s (struct (field (mut i32)))) (type $a (array (mut i64)))
      (func $target) (elem declare func $target)
      (func (export "_start") i32.const 37 struct.new $s
        i64.const -9 i32.const 3 array.new $a i32.const -7 ref.i31
        ref.func $target ref.null extern nop drop drop drop drop drop nop))''',[
        mark(1,0,[r'\(ref type-index=0 module=0\) = struct #1',r'\(ref type-index=1 module=0\) = array #2',
                  r'\(ref i31\) = i31 signed=-7 unsigned=2147483641',
                  r'\(ref type-index=2 module=0\) = function module=0 index=0',r'\(ref null extern\) = null']),mark(1,1,[])])
    for condition,result in ((1,152),(0,190)):
        wat=f'''(module {seed} (global $condition (mut i32) (i32.const {condition}))
          (func (export "_start") global.get $y global.get $x i32.const 1 i32.add
          global.get $condition if (param i32) (result i32) nop i32.const 4 i32.mul nop
          else nop i32.const 5 i32.mul nop end nop drop drop nop))'''
        branch=0 if condition else 2
        add('if-prefix-'+str(condition),wat,[mark(0,branch,['i64 = 1000','i32 = 38']),
            mark(0,branch+1,['i64 = 1000','i32 = '+str(result)]),mark(0,4,['i64 = 1000','i32 = '+str(result)]),mark(0,5,[])])
    # Native register materialization must not replace a structured control
    # prefix with a branch-local SSA definition. Exercise integer, FP and SIMD
    # prefixes on both edges and again after the real merge.
    prefix = ['i64 = 1000', 'f32 = bits=0x7fc12345', 'f64 = bits=0x8000000000000000',
              'v128 = bytes=01000000feffffff0300000004000000']
    for condition, result in ((1, 152), (0, 190)):
        wat = f'''(module {seed}
          (global $condition (mut i32) (i32.const {condition}))
          (global $f (mut f32) (f32.const nan:0x412345))
          (global $d (mut f64) (f64.const -0))
          (func (export "_start") global.get $y global.get $f global.get $d
            v128.const i32x4 1 -2 3 4 global.get $x i32.const 1 i32.add
            global.get $condition if (param i32) (result i32)
              nop i32.const 4 i32.mul nop else nop i32.const 5 i32.mul nop end
            nop drop drop drop drop drop nop))'''
        branch = 0 if condition else 2
        add('if-mixed-prefix-' + str(condition), wat,
            [mark(0, branch, prefix + ['i32 = 38']),
             mark(0, branch + 1, prefix + ['i32 = ' + str(result)]),
             mark(0, 4, prefix + ['i32 = ' + str(result)]), mark(0, 5, [])])
    for condition in (0, 1):
        wat = f'''(module {seed}
          (global $condition (mut i32) (i32.const {condition}))
          (func (export "_start") global.get $y global.get $x
            global.get $condition if (param i32) (result i32) nop end
            nop drop drop nop))'''
        expected = ([mark(0, 0, ['i64 = 1000', 'i32 = 37'])] if condition else [])
        add('if-identity-prefix-' + str(condition), wat,
            expected + [mark(0, 1, ['i64 = 1000', 'i32 = 37']), mark(0, 2, [])])
    add('loop-backedge-prefix',f'''(module {seed} (func (export "_start") (local $n i32)
        global.get $y i32.const 3 loop (param i32) (result i32)
        local.tee $n nop i32.const 1 i32.sub local.tee $n local.get $n br_if 0 end
        nop drop drop nop))''',[*[mark(0,0,['i64 = 1000','i32 = '+str(n)]) for n in (3,2,1)],
        mark(0,1,['i64 = 1000','i32 = 0']),mark(0,2,[])])
    leaf='(func $leaf (param i32) (result i32 i64) local.get 0 i32.const 2 i32.mul local.get 0 i64.extend_i32_s nop)'
    for tail in (False,True):
        middle='(func $middle (param i32) (result i32 i64) local.get 0 return_call $leaf)' if tail else ''
        entry=2 if tail else 1
        add('tail-call-frames' if tail else 'call-multivalue-frames',f'''(module {seed} {leaf} {middle}
          (func (export "_start") global.get $y global.get $x call {'$middle' if tail else '$leaf'}
            nop drop drop drop nop))''',[mark(0,0,['i32 = 74','i64 = 37'],['i64 = 1000']),
            mark(entry,0,['i64 = 1000','i32 = 74','i64 = 37']),mark(entry,1,[])])
    # Table and typed-reference dispatch must preserve the caller's prefix.
    # A tail transfer retires its middle activation before entering the leaf;
    # neither the original entry nor its live operand prefix may disappear.
    for dispatch in ('indirect', 'ref'):
        declarations = ('(table 1 funcref) (elem (i32.const 0) func $leaf)'
                        if dispatch == 'indirect' else '(elem declare func $leaf)')
        for tail in (False, True):
            operation = ('return_call_' if tail else 'call_') + dispatch
            target = 'i32.const 0' if dispatch == 'indirect' else 'ref.func $leaf'
            annotation = '(type $pair)' if dispatch == 'indirect' else '$pair'
            middle = (f'(func $middle (type $pair) local.get 0 {target} '
                      f'{operation} {annotation})') if tail else ''
            invocation = 'call $middle' if tail else f'{target} {operation} {annotation}'
            entry = 2 if tail else 1
            add(('tail-' if tail else 'call-') + dispatch + '-frames', f'''(module {seed}
              (type $pair (func (param i32) (result i32 i64)))
              (func $leaf (type $pair) local.get 0 i32.const 2 i32.mul
                local.get 0 i64.extend_i32_s nop)
              {declarations} {middle}
              (func (export "_start") global.get $y global.get $x {invocation}
                nop drop drop drop nop))''', [
                mark(0, 0, ['i32 = 74', 'i64 = 37'], ['i64 = 1000'], frame_total=2),
                mark(entry, 0, ['i64 = 1000', 'i32 = 74', 'i64 = 37'], frame_total=1),
                mark(entry, 1, [], frame_total=1)])
    add('exception-payload-prefix',f'''(module {seed} (tag $e (param i32 i64))
      (func (export "_start") global.get $y block (result i32 i64)
        try_table (result i32 i64) (catch $e 0) i32.const -7 i64.const 99 nop throw $e end end
        nop drop drop drop nop))''',[mark(0,0,['i64 = 1000','i32 = -7','i64 = 99']),
        mark(0,1,['i64 = 1000','i32 = -7','i64 = 99']),mark(0,2,[])])
    add('exception-reference-rethrow',f'''(module {seed} (tag $e (param i32 i64))
      (func $throw i32.const -7 i64.const 99 throw $e)
      (func (export "_start") global.get $y block $done
        block $caught (result (ref exn))
          try_table (catch_all_ref $caught) call $throw end unreachable
        end nop
        try_table (param (ref exn)) (catch_all $done) throw_ref end unreachable
      end nop drop nop))''',[
        mark(1,0,['i64 = 1000',r'\(ref exn\) = exception #1']),
        mark(1,1,['i64 = 1000']),mark(1,2,[])])
    reference_type='(ref type-index=0 module=0)'
    add('nondefaultable-local-initialization','''(module
      (type $s (struct (field (mut i32))))
      (func (export "_start") (local $p (ref $s))
        nop i32.const 37 struct.new $s local.set $p nop
        local.get $p nop drop nop))''',[
        mark(0,0,[],locals_page={'total':1,'values':{
            0:reference_type+' = unavailable (local is not initialized)'}}),
        mark(0,1,[],locals_page={'total':1,'values':{0:reference_type+' = struct #1'}}),
        mark(0,2,[r'\(ref type-index=0 module=0\) = struct #1']),mark(0,3,[])])
    add('atomic-rmw-wait',f'''(module {seed} (memory 1 1 shared)
      (func (export "_start") i32.const 0 i32.const 9 i32.store global.get $y
        i32.const 0 i32.const 4 i32.atomic.rmw.add nop i32.const 0 i32.atomic.load nop
        drop drop i32.const 0 i32.const 13 i64.const 0 nop memory.atomic.wait32 nop drop drop nop))''',[
        mark(0,0,['i64 = 1000','i32 = 9']),mark(0,1,['i64 = 1000','i32 = 9','i32 = 13']),
        mark(0,2,['i64 = 1000','i32 = 0','i32 = 13','i64 = 0']),mark(0,3,['i64 = 1000','i32 = 2']),mark(0,4,[])])
    add('multi-memory-copy-values', f'''(module {seed}
      (memory $a 1) (memory $b 1) (data (memory $a) (i32.const 0) "AB")
      (func (export "_start") global.get $y
        i32.const 8 i32.const 0 i32.const 2 nop memory.copy $b $a nop
        i32.const 8 i32.load8_u $b nop drop drop nop))''', [
        mark(0, 0, ['i64 = 1000', 'i32 = 8', 'i32 = 0', 'i32 = 2']),
        mark(0, 1, ['i64 = 1000']), mark(0, 2, ['i64 = 1000', 'i32 = 65']),
        mark(0, 3, [])])
    stored = '-9223372036854775785'
    add('memory64-load-store-values', f'''(module {seed}
      (memory i64 1) (data (i64.const 8) "A")
      (func (export "_start") (local $address i64) i64.const 8 local.set $address
        global.get $y local.get $address nop i32.load8_u nop drop
        i64.const 16 i64.const {stored} nop i64.store nop
        i64.const 16 i64.load nop drop drop nop))''', [
        mark(0, 0, ['i64 = 1000', 'i64 = 8']),
        mark(0, 1, ['i64 = 1000', 'i32 = 65']),
        mark(0, 2, ['i64 = 1000', 'i64 = 16', 'i64 = ' + stored]),
        mark(0, 3, ['i64 = 1000']), mark(0, 4, ['i64 = 1000', 'i64 = ' + stored]),
        mark(0, 5, [])])
    add('memory64-atomic-wait-values', f'''(module {seed} (memory i64 1 1 shared)
      (func (export "_start") i64.const 0 i64.const 13 i64.store global.get $y
        i64.const 0 i64.const 13 i64.const 0 nop memory.atomic.wait64 nop
        drop drop nop))''', [
        mark(0, 0, ['i64 = 1000', 'i64 = 0', 'i64 = 13', 'i64 = 0']),
        mark(0, 1, ['i64 = 1000', 'i32 = 2']), mark(0, 2, [])])
    add('table64-reference-values', f'''(module {seed}
      (type $f (func (result i32))) (table $t i64 1 3 funcref)
      (func $leaf (type $f) i32.const 42) (elem (table $t) (i64.const 0) func $leaf)
      (func (export "_start") global.get $y i64.const 0 nop table.get $t nop
        ref.is_null nop drop ref.null func i64.const 1 nop table.grow $t nop
        drop drop nop))''', [
        mark(1, 0, ['i64 = 1000', 'i64 = 0']),
        mark(1, 1, ['i64 = 1000', r'\(ref null func\) = function module=0 index=0']),
        mark(1, 2, ['i64 = 1000', 'i32 = 0']),
        mark(1, 3, ['i64 = 1000', r'\(ref null func\) = null', 'i64 = 1']),
        mark(1, 4, ['i64 = 1000', 'i64 = 1']), mark(1, 5, [])])
    vector = 'v128 = bytes=000102030405060708090a0b0c0d0e0f'
    reverse = 'v128 = bytes=0f0e0d0c0b0a09080706050403020100'
    # All swizzle indices are in range: every permitted relaxed result agrees.
    add('relaxed-simd-swizzle-values', f'''(module {seed}
      (func (export "_start") global.get $y
        v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
        nop i8x16.relaxed_swizzle nop drop drop nop))''', [
        mark(0, 0, ['i64 = 1000', vector, reverse]),
        mark(0, 1, ['i64 = 1000', reverse]), mark(0, 2, [])])
    add('gc-cast-field-values', f'''(module {seed}
      (type $base (sub (struct (field i32))))
      (type $child (sub $base (struct (field i32) (field i64))))
      (func (export "_start") (local $object (ref null $base))
        i32.const 7 i64.const 9 struct.new $child local.set $object
        global.get $y local.get $object nop ref.cast (ref $child) nop
        struct.get $child 1 nop drop drop nop))''', [
        mark(0, 0, ['i64 = 1000', r'\(ref null type-index=0 module=0\) = struct #1']),
        mark(0, 1, ['i64 = 1000', r'\(ref type-index=1 module=0\) = struct #1']),
        mark(0, 2, ['i64 = 1000', 'i64 = 9']), mark(0, 3, [])])
    globals=' '.join(f'(global $g{n} (mut i32) (i32.const {37+n}))' for n in range(300))
    deep=' '.join('global.get $g'+str(n) for n in range(300))
    add('deep-300-pagination',f'(module {globals} (func (export "_start") {deep} nop '+ 'drop '*300+'nop))',[
        mark(0,0,['i32 = '+str(37+n) for n in range(300)]),mark(0,1,[])])
    # Real operand values stay available beyond the old 32 KiB checkpoint cap.
    # Cover both sides of an unaligned packet base (two flag bytes per local).
    for count in (1000,1001,10000):
        high_locals = dict(total=count, values={0: 'i32 = 123', count-2: 'i32 = 0',
                                              count-1: 'i32 = -4567'})
        add('heap-workspace-'+str(count)+'-locals','(module (func (export "_start") (local '+ 'i32 '*count+
            ') i32.const 123 local.set 0 i32.const -4567 local.set '+str(count-1)+
            ' local.get 0 i64.const 456 nop drop drop nop))',
            [mark(0,0,['i32 = 123','i64 = 456'],locals_page=high_locals),
             mark(0,1,[],locals_page=high_locals)])
    # The former fixed identity quota must no longer hide this real operand site.
    add('deep-72-identity-preview','(module (func $recur (param i32) local.get 0 if '
        'local.get 0 i32.const 1 i32.sub call $recur else nop end) '
        '(func (export "_start") i32.const 70 call $recur))',[mark(0,0,[])])
    cases.extend(atomic_examples(mark))
    cases.extend(relaxed_simd_examples(mark))
    return cases


def markers(tool, wasm):
    dump=subprocess.check_output([str(tool),'dump',str(wasm)],text=True)
    function=None; first=None; found={}
    for line in dump.splitlines():
        match=re.match(r'=+ func (\d+) =+',line)
        if match:
            function=int(match[1]); first=None; found[function]=[]; continue
        match=re.match(r'\s*0x([0-9a-f]+) \| [0-9a-f ]+\| (.*)',line)
        if not match or function is None: continue
        address=int(match[1],16); description=match[2].strip()
        if description=='end' or re.match(r'[a-z][a-z0-9_]*(?: |$)',description):
            # Ignore section/size/local metadata; code op names have no spaces
            # except the official dump's named immediates following the name.
            if description.startswith(('size of function','custom section','name:','code section')): continue
            if re.search(r'local blocks|locals of type',description): continue
            if first is None: first=address
            if description=='nop': found[function].append(address-first)
        if 'custom section' in description: function=None
    return found,dump


def operands(console,thread,frame,expected,observations):
    all_rows=[]
    for first in range(0,max(1,len(expected)),64):
        reply=console.send(f'operands {thread} {frame} {first} 64')
        assert b'Wasm state unavailable' not in reply and b'error:' not in reply,reply
        total=re.search(rb'first=([0-9]+) total=([0-9]+)',reply);assert total,reply
        assert tuple(map(int,total.groups()))==(first,len(expected)),reply
        rows=re.findall(rb'^operand ([0-9]+) (.*)$',reply,re.M)
        for index,value in rows:
            index=int(index); value=value.decode()
            assert index==len(all_rows),(index,len(all_rows),reply)
            assert re.fullmatch(expected[index],value),(index,expected[index],value,reply)
            all_rows.append(value)
    assert len(all_rows)==len(expected),(expected,all_rows)
    observations.append(dict(frame=frame,values=all_rows))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source-root','binary','wasm-tools','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--case',action='append',help='Run selected named cases; repeat for a targeted matrix.')
    p.add_argument('--call-stack-policy',action='append',choices=('instruction','unwind'),
                   help='Select a call-stack policy for an independent targeted retry; defaults to both.')
    p.add_argument('--jit-policy',choices=('debug','default','fast-compile','balanced','max'),
                   help='Exercise an explicit LLVM optimization policy without changing the VM default.')
    p.add_argument('--runner-prefix-json',type=Path,
        help='JSON argv prefix for QEMU user mode; the binary remains the real target VM.')
    p.add_argument('--prompt-timeout',type=int,default=120)
    p.add_argument('--stop-timeout',type=int,default=20)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    assert 1 <= a.prompt_timeout <= 600 and 1 <= a.stop_timeout <= 120
    OperandConsole.prompt_timeout = a.prompt_timeout
    runner=json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
    assert isinstance(runner,list) and all(isinstance(x,str) and x for x in runner)
    subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    cases=[c for c in examples() if a.case is None or c['name'] in a.case];assert cases,a.case
    assert a.case is None or {c['name'] for c in cases} == set(a.case), a.case
    results=[]
    for case in cases:
        wat=a.out/(case['name']+'.wat');wasm=wat.with_suffix('.wasm');wat.write_text(case['wat']+'\n')
        subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True)
        subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],check=True)
        sites,dump=markers(a.wasm_tools,wasm);wat.with_suffix('.dump').write_text(dump)
        for policy in a.call_stack_policy or ('instruction','unwind'):
            c=None;row=dict(case=case['name'],policy=policy,actual_VM=True,observations=[],
                            atomic_opcodes=case.get('atomic_opcodes', []),
                            relaxed_simd_opcodes=case.get('relaxed_simd_opcodes', []))
            try:
                argv=[*runner,str(a.binary),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack',policy,
                    *['-WFE-'+f for f in ('multi-value','reference-types','function-references','gc','simd','tail-call','exceptions','threads',
                                         'bulk-memory','multi-memory','memory64','table64','relaxed-simd')],
                    *(['-Rllvm-policy',a.jit_policy] if a.jit_policy else []),
                    '--run',str(wasm)]
                row['argv']=argv
                c=OperandConsole(argv,a.out/(case['name']+'-'+policy+'.log'));c.prompt()
                for function,offsets in sites.items():
                    for offset in offsets:assert b'registered' in c.send(f'break 0 {function} {offset}')
                for expected in case['expected']:
                    c.send('continue');limit=time.monotonic()+a.stop_timeout
                    while True:
                        reply=c.send('status')
                        if b'stopped:' in reply:break
                        assert b'guest exited' not in reply and time.monotonic()<limit,reply
                        time.sleep(.01)
                    thread,function,offset=c.location()
                    assert (function,offset)==(expected['function'],sites[expected['function']][expected['ordinal']]),(expected,reply)
                    observations=[]
                    if expected['frame_total'] is not None:
                        stop = re.search(rb'^stop-id ([0-9]+)$', reply, re.M)
                        assert stop, reply
                        page = c.send(f'frames wasm {thread} {int(stop[1])} 0 3')
                        header = re.search(rb'^wasm-frames stop=([0-9]+) thread=([0-9]+) selected=0 count=([0-9]+) total=([0-9]+) first=0 physical=([0-9]+)$', page, re.M)
                        assert header, page
                        total = expected['frame_total']
                        assert tuple(map(int, header.groups())) == (int(stop[1]), thread, total, total, 0), page
                        observations.append(dict(total_frames=total))
                    if expected['values'] is None:
                        reply=c.send(f'operands {thread} 0 0 64')
                        assert b'Wasm state unavailable:' in reply and b'operand 0 ' not in reply,reply
                        observations.append(dict(frame=0,preview_available=False,reply=reply.decode()))
                        row.update(preview_available=False,complete_preview_requirement_met=False)
                    else:operands(c,thread,0,expected['values'],observations)
                    if expected['caller'] is not None:operands(c,thread,1,expected['caller'],observations)
                    if expected['locals_page'] is not None:
                        local_page=expected['locals_page'];local_rows=[]
                        for index,value in local_page['values'].items():
                            answer=c.send(f'locals wasm {thread} 0 {index} 1')
                            assert b'Wasm state unavailable' not in answer and b'error:' not in answer,answer
                            total=re.search(rb'first=([0-9]+) total=([0-9]+)',answer);assert total,answer
                            assert tuple(map(int,total.groups()))==(index,local_page['total']),answer
                            rows=re.findall(rb'^local ([0-9]+) (.*)$',answer,re.M)
                            assert rows==[(str(index).encode(),value.encode())],answer
                            local_rows.append(dict(index=index,value=value))
                        observations.append(dict(frame=0,total_locals=local_page['total'],locals=local_rows))
                    row['observations'].append(dict(function=function,offset=offset,frames=observations))
                c.send('continue');limit=time.monotonic()+a.stop_timeout
                while True:
                    reply=c.send('status')
                    if b'guest exited:' in reply:
                        assert b'guest exited: 0' in reply,reply;break
                    assert b'stopped:' not in reply and time.monotonic()<limit,reply;time.sleep(.01)
                row['passed']=True
            except Exception as error:row.update(passed=False,error=repr(error))
            finally:
                if c is not None:c.close()
            results.append(row);print(json.dumps({k:v for k,v in row.items() if k not in ('observations','argv')}),flush=True)
            (a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    (a.out/'inputs.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(a.binary.read_bytes()).hexdigest(),
        harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        runner_prefix=runner,runner_sha256=hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,
        prompt_timeout=a.prompt_timeout,stop_timeout=a.stop_timeout,jit_policy=a.jit_policy,
        call_stack_policies=a.call_stack_policy or ['instruction','unwind'],
        cgroup=Path('/proc/self/cgroup').read_text()),indent=2)+'\n')
    raise SystemExit(0 if all(r['passed'] for r in results) else 1)


if __name__=='__main__':main()
