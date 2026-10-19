#!/usr/bin/env python3
"""Actual native LLVM-full CLI, real PTY; run only in the original Linux cgroup.

Exercise every command class with a real DWARF/WASIp1 Wasm fixture. The separate
catalogue unit test checks every command page/example, without claiming every
Wasm instruction feature is present in this small guest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from run_debug_run_cli import Console, require
from run_debug_source_inline_metadata_cli import sections, u32


def clean(data):
    return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', data).replace(b'\r', b'')


def exports(wasm):
    result = {}
    for tag, payload in sections(wasm):
        if tag != 7:
            continue
        count, at = u32(payload, 0, len(payload))
        for _ in range(count):
            size, at = u32(payload, at, len(payload)); name = payload[at:at + size]; at += size
            kind = payload[at]; index, at = u32(payload, at + 1, len(payload))
            if kind == 0:
                result[name.decode()] = index
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'source', 'catalog', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--expected-cgroup', required=True)
    a = p.parse_args()
    require(Path('/proc/self/cgroup').read_text() == '0::' + a.expected_cgroup + '\n', 'original cgroup')
    a.out.mkdir(exist_ok=False)
    wasm = a.wasm.resolve(); functions = exports(wasm)
    argv = [str(a.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-call-stack', 'instruction',
            '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable', '--run', str(wasm)]
    c = Console(argv); rows = []; classes = set()

    def ask(command, marker=None, group=None, good=False):
        if '\n' in command:
            at = len(c.log); os.write(c.fd, command.encode() + b'\n')
            raw = c.wait(lambda data: b'\x1b[?1049l' in data and data.endswith(b'(uwvm-debug) '), at)
        else:
            raw = c.command(command, marker.encode() if marker else None)
        answer = clean(raw)
        if good:
            require(b'error:' not in answer and b'timed out' not in answer, (command, answer))
        require(b'command or thread is not valid in the current execution state' not in answer, (command, answer))
        rows.append({'command': command, 'class': group, 'reply': answer.decode(errors='replace')})
        if group:
            classes.add(group)
        (a.out / 'actions.json').write_text(json.dumps(rows, indent=2))
        return answer

    try:
        c.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        ask('wait', 'prepared; no Wasm instruction executed', 'execution')
        ask('up', 'has not started', 'language')
        short = ask('help up', 'This does not run code.')
        require(b'Usage:' in short and b'Example:' in short and b'COUNT' in short, short)
        ask('up nope', 'Usage: up [COUNT]')
        ask('operands nope', 'Usage: operands THREAD')
        ask('set wasip1 arg', 'Usage: set wasip1 arg MODULE INDEX HEX')
        ask('made-up', 'Undefined command')
        ambiguous = ask('star', 'Candidates:')
        require(all(('  ' + word + '\n').encode() in ambiguous for word in ['start', 'startw', 'starti']), ambiguous)
        ambiguous = ask('sta', 'Candidates:')
        require(all(('  ' + word + '\n').encode() in ambiguous for word in ['start', 'startw', 'starti', 'status']), ambiguous)
        ask('stat\t', 'prepared; no Wasm instruction executed', good=True)
        ambiguous = ask('info w', 'Candidates:')
        require(b'  info wasm-events\n  info wasip1\n' in ambiguous, ambiguous)
        ambiguous = ask('thread step-i', 'Candidates:')
        require(b'  thread step-in\n  thread step-inst\n  thread step-inst-over\n' in ambiguous, ambiguous)
        ask('wat', 'Did you mean "wait"')
        ask('pri', 'Usage: print EXPR')
        ask('status', 'prepared; no Wasm instruction executed', good=True)
        modules = ask('bw', 'Wasm modules', 'breakpoints', True)
        require(wasm.name.encode() in modules and b'source breakpoints available' in modules, modules)
        ask('bs 0', 'Usage: bs 0 FILE:LINE', good=True)
        ask('help bw', 'Set a Wasm breakpoint', good=True)
        ask('help bs', 'Set a source breakpoint', good=True)
        ask('help breaksource', 'Set a source breakpoint', good=True)
        ask('i mo', 'Page 1/1', good=True)
        ask('  i   mo   ', 'Page 1/1', good=True)
        ask('  help   wasm   set   memory   full   1  ', 'Arguments:', good=True)
        ask('info modules 2', 'out of range')
        ask('bw name.wasm', 'not a filename')
        ask('bs 0 1 0', 'Usage: bs MODULE_ID FILE:LINE')

        ask('help pri', 'print EXPR', good=True)
        ask('help i lo', 'List source local variables', good=True)
        ask('help wasm set memo', 'set wasm memory MODULE', good=True)
        ask('help  asm   reg\t', 'Native registers', good=True)
        ask('  h   wasm    set    memo\t', 'set wasm memory MODULE', good=True)
        ask('help   source\t', 'help language stepping', good=True)
        ask('help   \t\t\x15help', 'Command classes:', good=True)
        middle_help = '  help  asm   regZZ   full 1'
        middle_help += '\x1b[D' * (len(middle_help) - middle_help.index('ZZ')) + '\t'
        full_help = ask(middle_help, 'Usage:', good=True)
        require(b'Arguments:' in full_help and b'Full details:' not in full_help,
                'mid-word help completion retains the full-detail and page suffix')
        ask('help  asm   reg\t\x1f\t', 'Native registers', good=True)
        ask('help sta', 'help status', good=True)
        ask('help info w', 'help info wasm-events', good=True)
        ask('help thread step-i', 'help thread step-inst-over', good=True)
        ask('help wasm set m', 'help wasm set member', good=True)
        for typo, corrected in [('prinnt', 'print'), ('thr lsit', 'thread list'),
                                ('i waspi1 args', 'info wasip1 args'), ('info wasip1 agrs', 'info wasip1 args'),
                                ('waspi1 inspect args', 'wasip1 inspect args'), ('wasm set memroy', 'wasm set memory'),
                                ('wasip2', 'wasip1')]:
            suggestion = ask('help ' + typo, 'Did you mean:', good=True)
            require(('  help ' + corrected + '\n').encode() in suggestion and b'Usage:' not in suggestion, suggestion)
            opened = ask('help ' + corrected, good=True)
            require(b'Unknown help' not in opened and b'Ambiguous help' not in opened, opened)
        ask('help set wasip1 restroe full 2', 'help set wasip1 restore full 2', good=True)
        for selector in ['wasm set memroy 0', 'wasm set memroy 1x', 'print counter']:
            answer = ask('help ' + selector, 'Unknown help', good=True)
            require(b'Did you mean:' not in answer, answer)
        search = ask('apropos NATIVE register', 'help asm registers', good=True)
        require(b'matching commands' in search, search)
        search = ask('apr wasm', 'Next: apropos wasm 2', good=True)
        require(b'Page 1/' in search, search)
        ask('apropos wasm 2', 'Page 2/', good=True)
        ask('apr\tregister', 'help asm registers', good=True)
        ask('apropos unlikely-no-match', 'No matching commands', good=True)
        ask('apropos', 'Usage: apropos WORD')
        ask('apropos memory 0', 'Usage: apropos WORD')
        ask('apropos memory 999', 'out of range', good=True)
        ask('help missing-topic', 'apropos WORD', good=True)
        ask('info', 'info registers', 'inspection')
        ask('set', 'set wasm', 'wasm')
        for command, marker in [('thread', 'thread list'), ('thr', 'thread list'), ('process', 'process status'),
                                ('proc', 'process continue'), ('br', 'breakpoint list'), ('reg', 'register read'),
                                ('trace', 'trace wasip1'), ('catch', 'catch wasm'),
                                ('set wasip1', 'set wasip1 arg'), ('set wasm', 'help wasm set memory'),
                                ('info wasi', 'help wasip1 inspect args'), ('path', 'help wasm paths create'),
                                ('trace wasm', 'trace wasm on'), ('trace wasip1', 'trace wasip1 on')]:
            ask(command, marker, good=True)
        ask('help thr', 'thread list', good=True)
        ask('help reg', 'register read', good=True)
        ask('thr li\t', 'prepared; no Wasm instruction executed', good=True)
        ask('proc st\t', 'prepared; no Wasm instruction executed', good=True)
        ask('i th\t', 'prepared; no Wasm instruction executed', good=True)
        ask('set wasm memo\t', 'Usage: set wasm memory MODULE')
        ask('set wasip1 check\t', 'Usage: set wasip1 checkpoint MODULE SLOT')
        ask('set wasip1 fd-d\t', 'Usage: set wasip1 fd-dup MODULE FD')
        ask('unset wasip1 fd\t', 'Usage: unset wasip1 fd MODULE FD')
        ask('path cr\t', 'Usage: path create locals|operands|saved|globals|table')
        levels = ask('set \t\t\x15set', 'Set commands', good=True)
        require(b'set wasm' in levels and b'set wasip1' in levels, levels)
        levels = ask('trace \t\t\x15trace', 'Trace commands', good=True)
        require(b'trace wasm' in levels and b'trace wasip1' in levels, levels)
        ask('i \t\x15info', 'Inspect the program', good=True)
        ask('thr \t\x15thr', 'Thread commands', good=True)
        ask('i wasi \t\x15info wasi', 'Inspect WASIp1', good=True)
        ask('thr nonsense', 'help thread')
        ask('reg nonsense', 'help register')
        ask('set wasip1 nonsense', 'help set wasip1')
        ask('trace nonsense', 'help trace')
        for command, suggested in [('thr lsit', 'thread list'), ('reg reed', 'register read'),
                                   ('info threds', 'info threads'), ('i wasi agrs', 'info wasip1 args'),
                                   ('thr step-ovre', 'thread step-over'), ('trace wasmm', 'trace wasm')]:
            answer = ask(command, 'Did you mean "' + suggested + '"')
            require(b'No command was executed' in answer and ('help ' + suggested).encode() in answer, answer)
        ask('thr list extra', 'Usage: status')
        for command, suggested in [('info waspi1 args', 'info wasip1'),
                                   ('set waspi1 arg 0 0 6869', 'set wasip1'),
                                   ('trace wasip2 on fd_write', 'trace wasip1')]:
            answer = ask(command, 'Did you mean "' + suggested + '"')
            require(b'No command was executed' in answer and ('help ' + suggested).encode() in answer, answer)
        ask('help info threads', 'Show execution state and current thread/stop labels.', good=True)
        ask('pa', 'Candidates:')
        ask('status', 'prepared; no Wasm instruction executed', good=True)
        # Every help page is navigated through the actual production console.
        for item in json.loads(a.catalog.read_text()):
            if not item['usage']:
                ask('help ' + item['key'])
            else:
                brief = ask('help ' + item['key'], 'Example:')
                require(b'Full details:' in brief, (item['key'], brief))
        ask('trace wasm on memory', group='wasm', good=True)
        ask('trace wasip1 on fd_write', group='wasip1', good=True)
        line = next(i for i, text in enumerate(a.source.read_text().splitlines(), 1) if 'UX_LEAF_READY' in text)
        bp = ask(f'bs 0 {a.source}:{line}', 'breakpoint ', 'breakpoints', True)
        identifier = int(re.search(rb'breakpoint (\d+)', bp)[1])
        ask('info breakpoints', good=True)
        ask(f'disable {identifier}', good=True); ask(f'enable {identifier}', good=True)
        ask(f'ignore {identifier} 0', good=True)
        ask('run', 'stopped: breakpoint', 'execution', True)
        status = ask('status', 'stop-id ', good=True)
        thread = int(re.search(rb'thread (\d+) module=0', status)[1]); stop = int(re.search(rb'stop-id (\d+)', status)[1])
        stack = ask('bt', group='language', good=True)
        require(re.search(rb'(?m)^#0 ', stack) and b'inline metadata unavailable:' not in stack, stack)
        values = ask('info locals', good=True)
        require(b'counter = 2' in values and b'source-stop' not in values and b'source local ' not in values, values)
        printed = ask('print counter', good=True)
        require(b'counter = 2' in printed and b'source-value stop=' not in printed and b'source-value end' not in printed, printed)
        typed = ask('ptype counter', good=True)
        require(b'type = volatile int' in typed and b'type-node' not in typed, typed)
        expression = ask('print counter + 1', good=True)
        require(b'counter + 1 = 3' in expression, expression)
        spaced_expression = ask('  print   counter  +  1  ', good=True)
        require(b'= 3' in spaced_expression, spaced_expression)
        ask('  memory   0   0   0   4  ', '00 00 00 00', good=True)
        ask('  info   wasip1   args   0  ', good=True)
        ask('  trace   wasm   on   memory  ', good=True)
        ask('  trace   wasip1   on   fd_write  ', good=True)
        record = ask('print record', good=True)
        require(b'count = 2' in record and b'ratio = 1.5' in record and b'source-value end' not in record, record)
        layout = ask('ptype record', good=True)
        require(b'Record' in layout and b'count: int' in layout and b'offset 0, 4 bytes' in layout, layout)
        pointer = ask('print pointer', good=True)
        require(b'pointer = guest:0x' in pointer, pointer)
        completed = ask('pri coun\t', good=True)
        require(b'counter = 2' in completed, completed)
        completed_type = ask('pt rec\t', good=True)
        require(b'type = volatile Record' in completed_type, completed_type)
        completed_member = ask('fr v record.co\t', good=True)
        require(b'record.count = 2' in completed_member, completed_member)
        completed_middle = ask('pri counZZ\x1b[D\x1b[D\t', good=True)
        require(b'counter = 2' in completed_middle, completed_middle)
        selected_completion = ask(f'pri {thread} {stop} coun\t', 'source-value stop=', good=True)
        require(b'name=counter' in selected_completion, selected_completion)
        ask('prinnt counter', 'Did you mean "print"')
        before_typo = ask('memory 0 0 0 4', good=True)
        for command, suggested in [('set wasm memroy 0 0 1 0 bytes 0102', 'set wasm memory'),
                                   ('set wasip1 restroe 0 1', 'set wasip1 restore'),
                                   ('set wasip1 fd-dupp 0 1 2', 'set wasip1 fd-dup'),
                                   ('set waspi1 arg 0 0 6869', 'set wasip1'),
                                   ('set wasip2 restore 0 1', 'set wasip1'),
                                   ('set waspi1 file 0 0102', 'set wasip1')]:
            answer = ask(command, 'Did you mean "' + suggested + '"')
            require(b'No command was executed' in answer and ('help ' + suggested).encode() in answer, answer)
        after_typo = ask('memory 0 0 0 4', good=True)
        require(b'00 00 00 00' in before_typo and b'00 00 00 00' in after_typo,
                'typo suggestions leave guest memory unchanged')
        unchanged = ask('status', 'stop-id ', good=True)
        require(int(re.search(rb'stop-id (\d+)', unchanged)[1]) == stop, 'completion and typo diagnostics retain the stop')
        ask('apropos native register', 'help asm registers', good=True)
        ask('help pri', 'print EXPR', good=True)
        unchanged = ask('status', 'stop-id ', good=True)
        require(int(re.search(rb'stop-id (\d+)', unchanged)[1]) == stop, 'help and keyword discovery retain the same stop')
        for command, marker in [('thr', 'thread list'), ('reg', 'register read'), ('set', 'set wasip1'),
                                ('info wasi', 'help wasip1 inspect args')]:
            ask(command, marker, good=True)
        empty = ask('', good=True)
        require(b'stopped: breakpoint' not in empty and b'Continuing.' not in empty, 'Enter after a menu does not repeat an older command')
        unchanged = ask('status', 'stop-id ', good=True)
        require(int(re.search(rb'stop-id (\d+)', unchanged)[1]) == stop, 'prefix menus and empty Enter retain the same actual stop')
        current = ask('i threZZ\x1b[D\x1b[D\t', 'stop-id ', good=True)
        require(int(re.search(rb'stop-id (\d+)', current)[1]) == stop, 'mid-word subcommand completion retains the stop')
        current = ask('procXX status\x1b[H' + '\x1b[C' * 4 + '\t', 'stop-id ', good=True)
        require(int(re.search(rb'stop-id (\d+)', current)[1]) == stop, 'mid-word root completion retains arguments and stop')
        ask(f'print {thread} {stop} counter', 'source-value stop=', good=True)
        ask(f'locals source {thread}', 'source-stop ', good=True)
        ask(f'bt {thread}', 'stopped: breakpoint', good=True)
        ask('frame 0', '#0 ', good=True); ask('up', '#1 ', good=True); ask('down', '#0 ', good=True)
        ask('down', 'innermost frame (#0)'); ask('up 999', 'Selection unchanged')
        ask(f'frame {thread} {stop + 1000} 0', 'no longer current')
        ask(f'frame 999999 {stop} 0', 'not in the current stopped program')
        ask(f'locals wasm {thread}', group='wasm', good=True)
        ask(f'operands {thread}', good=True); ask(f'globals {thread} 0', good=True)
        ask('memory 0 0 0 4', good=True)
        edit = ask(f'set wasm memory 0 0 {thread} 0 bytes 01020304', group='wasm')
        memory_edit_supported = b'error:' not in edit
        changed = ask('memory 0 0 0 4', good=True)
        if memory_edit_supported:
            require(b'01 02 03 04' in changed, changed)
        else:
            require(b'No change was made' in edit and b'help set wasm memory' in edit, edit)
            require(b'00 00 00 00' in changed, changed)
        ask(f'controls {thread}', good=True); ask(f'handlers {thread}', good=True)
        ask('info wasm-events', good=True); ask('trace wasm read 0 16', good=True)
        ask('wasm-script status; info wasm-events', group='wasm', good=True)
        for name in ('args', 'env', 'fds', 'preopens'):
            ask('info wasip1 ' + name + ' 0', group='wasip1', good=True)
        ask('set wasip1 checkpoint 0 0', group='checkpoints', good=True)
        ask('set wasip1 arg 0 0 617070', good=True)
        ask('set wasip1 restore 0 0 bindings', good=True)
        ask('unset wasip1 checkpoint 0 0', good=True)
        body = a.out / 'future.body'; body.write_bytes(bytes.fromhex('00 41 08 0b'))
        ask(f'replace 0 {functions["future"]} 1 {body}', 'function replaced; generation 2', 'update', True)
        ask(f'replace 0 {functions["future"]} 1 {body}', 'function generation changed')
        bad = a.out / 'invalid.body'; bad.write_bytes(bytes.fromhex('00 ff 0b'))
        ask(f'replace 0 {functions["future"]} 2 {bad}', 'failed WebAssembly validation')
        ask(f'replace 0 {functions["leaf"]} 1 {body}', 'active on a stopped Wasm stack')
        # Native queries must not cross into the VM/host context.
        ask('info registers', 'Registers require a native instruction stop', 'asm')
        ask('reg re\t', 'Registers require a native instruction stop', 'asm')
        ask('i reg\t', 'Registers require a native instruction stop', 'asm')
        ask('si', 'stopped: native instruction step', 'asm', True)
        registers = ask('info registers', good=True)
        disassembly = ask('disas')
        disassembly_visible = b'error:' not in disassembly
        if not disassembly_visible:
            require(b'No instruction in this window' in disassembly and b'help disas' in disassembly, disassembly)
            require(b'instruction 31' not in disassembly, disassembly)
        ask('up', 'native instruction stop')
        ask('ni', 'stopped: native instruction step', 'asm', True)
        native_finish = ask('finish asm', group='asm')
        if b'error:' in native_finish:
            require(b'native finish unavailable:' in native_finish and b'current stop retained' in native_finish
                    and b'help asm finish' in native_finish, native_finish)
        else:
            require(b'stopped: native instruction step' in native_finish, native_finish)
        ask('tui enable\nlayout split\ntui disable', '\x1b[?1049l', 'tui')
        spaced_tui = ask('  tui   enable  \n  layout   regs   \n  focus   cmd   \n  tui   disable   ', '\x1b[?1049l', 'tui')
        require(b'layout regs' in spaced_tui and b'error: layout' not in spaced_tui and b'error: focus' not in spaced_tui and
                b'error: tui' not in spaced_tui, 'spaced TUI commands select register layout and restore the terminal')
        ask('run', 'stopped: breakpoint', 'execution', True)
        ended = ask('c', 'Program exited normally.', 'execution', True)
        require(re.search(rb'(?m)^3$', ended), ended)
        require(not re.search(rb'(?m)^running$', ended), ended)
        ask('up', 'program has exited'); ask('c', 'program has exited')
        ask('process', 'process status', good=True)
        ask('register', 'register read', good=True)
        ask('set', 'set wasip1', good=True)
        # Each stepping command starts at the same actual leaf breakpoint.
        for index, (command, group) in enumerate([('s', 'language'), ('n', 'language'), ('finish', 'language'),
                               ('step wasm {thread} into', 'wasm'), ('step wasm {thread} over', 'wasm'),
                               ('step wasm {thread} out', 'wasm')]):
            # The completed run retained an enabled breakpoint. Once exited,
            # restart before editing the live breakpoint policy again.
            if index:
                ask(f'enable {identifier}', good=True)
            ask('run', 'stopped: breakpoint', 'execution', True)
            current = ask('status', 'stop-id ', good=True)
            selected = int(re.search(rb'thread (\d+) module=0', current)[1])
            before = int(re.search(rb'stop-id (\d+)', current)[1])
            ask(f'disable {identifier}', good=True)
            ask(command.format(thread=selected), 'stopped: selected participant step', group, True)
            after = ask('status', 'stop-id ', good=True)
            require(int(re.search(rb'stop-id (\d+)', after)[1]) > before, (command, current, after))
        ask(f'enable {identifier}', good=True)
        ask('run', 'stopped: breakpoint', 'execution', True)
        c.finish()
        required = {'execution', 'breakpoints', 'language', 'wasm', 'wasip1', 'checkpoints', 'update', 'asm', 'tui', 'inspection'}
        require(classes == required, classes)
        result = {'passed': True, 'scope': 'actual native x86_64 LLVM-full PTY command classes and help navigation; not all Wasm-feature coverage',
                  'command_count': len(rows), 'classes': sorted(classes), 'binary_sha256': hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),
                  'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(), 'normal_quit': True, 'terminal_restored': True,
                  'native_finish_supported': b'error:' not in native_finish,
                  'memory_edit_supported_in_fixture': memory_edit_supported,
                  'disassembly_visible_at_first_native_stop': disassembly_visible,
                  'numeric_register_mapping_at_first_native_stop': b'Only the Wasm program counter' not in registers}
        (a.out / 'result.json').write_text(json.dumps(result, indent=2)); print(json.dumps(result), flush=True)
    finally:
        (a.out / 'transcript.txt').write_bytes(c.log); c.close()


if __name__ == '__main__':
    main()
