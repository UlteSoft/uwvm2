#!/usr/bin/env python3
"""Real PTY: cooperative and native stops display the exact owned ASM resume PC."""
import argparse, hashlib, json, os, re
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_run_cli import require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.pop('NO_COLOR', None)
    os.environ['TERM'] = 'xterm-256color'
    c = Driver([str(args.uwvm.resolve()), '-Rdbg', '-Rct', '0',
                '-Rllvm-cache-path', 'disable', '--run', str(args.wasm.resolve())])
    actions, checks = [], []

    def command(text):
        c.send(text + '\n')
        actions.append({'command': text, 'screen': c.model.text()})

    def check(ok, label):
        require(ok, (label, c.model.text()))
        checks.append(label)

    def asm_pc():
        body = c.model.text().split('[io]')[0]
        match = re.search(r'=> (0x[0-9a-f]+)', body)
        check(match is not None and 'N>' not in body, 'ASM shows exact owned resume marker')
        return int(match[1], 16)

    try:
        c.wait(lambda b: b'(uwvm-debug) ' in b, 0)
        c.drain()
        c.ask('startw')
        command('layout asm')
        cooperative_pc = asm_pc()
        command('layout wasm')
        check('=>' in c.model.text().split('[io]')[0], 'Wasm entry is exact')
        command('layout asm')
        check(asm_pc() == cooperative_pc, 'layout switching retains exact cooperative PC')
        command('tui disable')
        before = c.ask('status')
        rejected = c.ask('jump asm *' + hex(cooperative_pc))
        check(b'Execution moved' not in rejected and b'error:' in rejected,
              'display marker does not authorize relocation at a cooperative stop')
        check(c.ask('status') == before, 'rejected cooperative relocation retains stop')
        command('tui enable')
        for action in ('si', 'ni', 'nw', 'startw', 'starti', 'ni'):
            command(action)
            asm_pc()
        command('layout horizontal src wasm asm')
        text = c.model.text().split('[io]')[0]
        check('[src' in text and '[wasm' in text and '[asm' in text,
              'three levels remain composed')
        check('=> 0x' in text, 'composed native pane keeps exact marker')
        command('tui disable')
        c.finish()
    finally:
        (args.out / 'transcript.bin').write_bytes(c.log)
        (args.out / 'actions.json').write_text(json.dumps(actions, indent=2))
        c.close()
    (args.out / 'result.json').write_text(json.dumps({
        'passed': True, 'checks': checks,
        'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest()}, indent=2))
    print('PASS native pane markers:', len(checks), 'checks')


if __name__ == '__main__':
    main()
