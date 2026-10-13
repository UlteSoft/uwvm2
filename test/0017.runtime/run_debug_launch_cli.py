#!/usr/bin/env python3
"""Real start/startw/starti/kill lifecycle, only in the original Linux 64 GiB cgroup."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from run_debug_run_cli import Console, require, wasm_fixture


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--main-wasm', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--expected-cgroup', required=True)
    a = p.parse_args()
    require(Path('/proc/self/cgroup').read_text() == '0::' + a.expected_cgroup + '\n', 'original cgroup')
    cg = Path('/sys/fs/cgroup') / a.expected_cgroup.lstrip('/')
    require((cg / 'memory.max').read_text().strip() == str(64 << 30), '64GiB')
    require((cg / 'memory.swap.max').read_text().strip() == '0', 'swap0')
    a.out.mkdir(exist_ok=False)
    wasm = a.out / 'launch.wasm'
    wasm.write_bytes(wasm_fixture())
    rows = []

    def open_console(path):
        c = Console([str(a.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', str(path)])
        c.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        return c

    c = open_console(wasm)
    try:
        def command(text, marker=None):
            out = c.command(text, marker)
            rows.append({'command': text, 'reply': out.decode(errors='replace')})
            return out

        command('wait', b'prepared; no Wasm instruction executed')
        def wasm_entry(word):
            reply = command(word, b'stopped: first Wasm instruction')
            require(b'JIT entry' not in reply and b'error:' not in reply, repr(reply))
            status = command('status', b'stop-id ')
            require(b'module=0 function=0 byte-offset=0' in status, repr(status))
            thread = int(re.search(rb'thread ([0-9]+) module=0 function=0', status)[1])
            require(b'native-pc=' not in status, 'Wasm entry is a cooperative opcode stop')
            g = command(f'globals {thread} 0')
            require(re.search(rb'0 i32[^\r\n]*\b7\b', g), repr(g))
            require(b'error:' in command('memory 0 0 65536 1'), 'no memory.grow executed')
            require(b'memory: 0' in command('memory 0 0 32 1').lower(), 'fresh bytes')
            return thread, int(re.search(rb'stop-id ([0-9]+)', status)[1])

        def native_entry(word):
            reply = command(word, b'stopped: first Wasm instruction JIT entry')
            require(b'error:' not in reply and b'stopped: native instruction step' not in reply, repr(reply))
            status = command('status', b'stop-id ')
            require(b'native-pc=' in status, 'real native PC at initial trap: ' + repr(status))
            registers = command('info registers')
            require(b'error:' not in registers and re.search(rb'(?:pc|rip|eip)=0x', registers), 'actual filtered registers: ' + repr(registers))
            code = command('disassemble')
            require(b'error:' not in code and b'native' in code.lower(), 'actual owned JIT disassembly: ' + repr(code))
            require(b'No Wasm call stack was captured at this stop.' in command('bt'), 'native stop does not reuse cooperative backtrace')
            require(b'error:' in command('info locals'), 'native stop does not reuse cooperative locals')
            require(b'memory: 00' in command('memory 0 0 0 1'), 'linear memory is queried from the stopped owner')
            require(b'error:' in command('memory 0 0 65536 1'), 'native stop retains memory bounds')
            return status

        thread, old_stop = wasm_entry('stw\t')
        command('breakwasm 0 1 0', b'breakpoint ')
        command(f'set wasm memory 0 0 {thread} 32 bytes ff', b'applied=1')
        native_entry('sti\t')
        command('si', b'stopped: native instruction step')
        thread, _ = wasm_entry('startw')
        command('kill', b'Program terminated.')
        for query in ('status', 'c', 'bt', 's', 'n', 'print counter', 'info locals', 'memory 0 0 0 1', 'kill'):
            require(b'No program is running.' in command(query), query)
        command('info breakpoints', b'breakpoint ')
        for word in ('st', 'stw', 'sti'):
            help_reply = command('help ' + word + ' full')
            require(b'No arguments' in help_reply and b'Unknown' not in help_reply, repr(help_reply))
        native_entry('starti')
        command('kill', b'Program terminated.')
        thread, _ = wasm_entry('stw')
        require(b'error:' in command(f'print {thread} {old_stop} counter'), 'old stop cannot read new instance')
        for bad in ('starti 1', 'startw 1', 'sti 1', 'stw 1', 'start&', 'startw&', 'starti&', 'kill 1', 'restart 0'):
            require(b'error:' in command(bad), 'invalid or removed launch syntax: ' + bad)
        command('c', b'stopped: breakpoint')
        require(re.search(rb'0 i32[^\r\n]*\b8\b', command(f'globals {thread} 0')), 'continued from first opcode')
        command('c', b'Program exited normally.')
        wasm_entry('startw')
        command('kill', b'Program terminated.')
        c.finish()
    finally:
        (a.out / 'pure-transcript.txt').write_bytes(c.log)
        c.close()

    c = open_console(a.main_wasm)
    try:
        first = c.command('st\t', b'stopped: breakpoint')
        require(b'Temporary breakpoint ' in first and b'error:' not in first, repr(first))
        require(b'$1' not in c.command('print phase'), 'no GDB value-history fiction')
        phase = c.command('print phase')
        require(re.search(rb'\bphase = 1\b', phase), 'startup executed, main body not executed: ' + repr(phase))
        count = c.command('print counter')
        require(re.search(rb'\bcounter = 7\b', count), repr(count))
        points = c.command('info breakpoints')
        require(b'breakpoint ' not in points, 'temporary point deleted: ' + repr(points))
        c.command('c', b'Program exited normally.')
        # No temporary main point survives ordinary run after exit.
        c.command('run', b'Program exited normally.')
        c.command('start', b'stopped: breakpoint')
        require(re.search(rb'\bphase = 1\b', c.command('print phase')), 'start restores initial data')
        user = c.command('bw 0 main', b'breakpoint ')
        user_id = int(re.search(rb'breakpoint ([0-9]+) ', user)[1])
        c.command(f'condition {user_id} 0', b'breakpoint policy updated')
        c.command('kill', b'Program terminated.')
        c.command('start', b'stopped: breakpoint')
        require(re.search(rb'\bphase = 1\b', c.command('print phase')), 'false user condition does not hide temporary main')
        enabled = c.command('info breakpoints')
        require(b'condition=0' in enabled and b'enabled=yes' in enabled and b'ignore=0' in enabled,
                'enabled false condition retained: ' + repr(enabled))
        current_id = int(re.search(rb'breakpoint ([0-9]+) ', enabled)[1])
        c.command(f'ignore {current_id} 3', b'breakpoint policy updated')
        c.command(f'disable {current_id}', b'breakpoint policy updated')
        c.command('kill', b'Program terminated.')
        c.command('start', b'stopped: breakpoint')
        require(re.search(rb'\bphase = 1\b', c.command('print phase')), 'disabled user point does not hide temporary main')
        retained = c.command('info breakpoints')
        require(b'condition=0' in retained and b'enabled=no' in retained and b'ignore=3' in retained,
                'user condition, enable state and ignore count retained: ' + repr(retained))
        c.command('stw', b'stopped: first Wasm instruction')
        c.command('c', b'Program exited normally.')
        c.finish()
    finally:
        (a.out / 'main-transcript.txt').write_bytes(c.log)
        c.close()
    # A real running guest, with no breakpoint or guest I/O, is physically
    # retired by kill. It cannot keep executing behind the retained prompt.
    def section(tag, payload):
        require(len(payload) < 128, 'small loop section')
        return bytes([tag, len(payload)]) + payload
    loop = (b'\0asm\x01\0\0\0' + section(1, bytes.fromhex('01 60 00 00'))
            + section(3, bytes.fromhex('01 00')) + section(7, b'\x01\x06_start\x00\x00')
            + section(10, bytes.fromhex('01 08 00 03 40 01 0c 00 0b 0b')))
    loop_path = a.out / 'running.wasm'; loop_path.write_bytes(loop)
    c = open_console(loop_path)
    try:
        c.command('r&', b'Starting program: ')
        c.command('kill', b'Program terminated.')
        require(b'No program is running.' in c.command('status'), 'running guest retired')
        c.command('stw', b'stopped: first Wasm instruction')
        c.command('kill', b'Program terminated.')
        c.finish()
    finally:
        (a.out / 'running-transcript.txt').write_bytes(c.log)
        c.close()
    result = {'passed': True, 'binary_sha256': hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),
              'main_wasm_sha256': hashlib.sha256(a.main_wasm.read_bytes()).hexdigest(),
              'startw_from_native_stop': True, 'startw_resets_real_memory': True,
              'starti_real_zero_step_native_trap': True, 'startw_real_zero_opcode_stop': True,
              'explicit_aliases_help_and_tab': True,
              'kill_blocks_guest_queries': True, 'main_startup_before_temporary_stop': True,
              'temporary_main_point_removed': True, 'launch_after_kill_and_exit': True,
              'false_user_main_condition_retained': True, 'kill_running_guest': True,
              'unsupported_restart_removed': True, 'terminal_restored': True,
              'pure_command_count': len(rows)}
    (a.out / 'pure-actions.json').write_text(json.dumps(rows, indent=2))
    (a.out / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
