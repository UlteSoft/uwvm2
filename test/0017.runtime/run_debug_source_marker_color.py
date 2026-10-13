#!/usr/bin/env python3
"""Check source marker colors after real Wasm steps and retained-frame updates."""
import argparse
import codecs
import copy
import hashlib
import json
import os
from pathlib import Path
import re

from run_debug_console_cursor import cells
from run_debug_console_tui_color import Driver, Terminal
from run_debug_run_cli import require


class ColoredTerminal(Terminal):
    def __init__(self):
        super().__init__()
        self.foreground = None
        self.colors = self.blank_colors()
        self.primary_colors = None
        self.stream_decoder = codecs.getincrementaldecoder('utf-8')()
        self.stream_pending = ''

    def blank_colors(self):
        return [[None] * self.columns for _ in range(self.rows)]

    def csi(self, params, final):
        if final == 'm':
            for value in map(int, (params or '0').split(';')):
                if value in (0, 39):
                    self.foreground = None
                elif 30 <= value <= 37 or 90 <= value <= 97:
                    self.foreground = value
        elif params == '?1049':
            if final == 'h' and not self.alternate:
                self.primary_colors = copy.deepcopy(self.colors)
                self.colors = self.blank_colors()
            elif final == 'l' and self.alternate:
                self.colors = self.primary_colors
                self.primary_colors = None
        elif final == 'J' and params == '2':
            self.colors = self.blank_colors()
        elif final == 'K':
            first = 0 if params == '2' else self.x
            self.colors[self.y][first:] = [self.foreground] * (self.columns - first)
        super().csi(params, final)

    def feed(self, data):
        self.stream_pending += self.stream_decoder.decode(data)
        while self.stream_pending:
            text = self.stream_pending
            c = text[0]
            if c == '\x1b':
                if len(text) < 2:
                    return
                if text[1] == '[':
                    end = next((i for i in range(2, len(text)) if '@' <= text[i] <= '~'), None)
                    if end is None:
                        return
                elif text[1] == ']':
                    end_marker = re.search(r'\x07|\x1b\\', text)
                    if end_marker is None:
                        return
                    self.stream_pending = text[end_marker.end():]
                    continue
                else:
                    end = 1
                super().feed(text[:end + 1].encode())
                self.stream_pending = text[end + 1:]
                continue
            self.stream_pending = text[1:]
            if c == '\n' and self.y == self.rows - 1:
                self.colors.pop(0)
                self.colors.append([self.foreground] * self.columns)
            super().feed(c.encode())
            width = cells(c) if c >= ' ' else 0
            if width:
                for x in range(self.x - width, self.x):
                    self.colors[self.y][x] = self.foreground


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.pop('NO_COLOR', None)
    os.environ['TERM'] = 'xterm-256color'
    console = Driver([str(args.uwvm.resolve()), '-Rdbg', '-Rct', '0',
                      '-Rllvm-cache-path', 'disable', '--run', str(args.wasm.resolve())])
    console.model = ColoredTerminal()
    checks = []

    def check_marker(marker, action):
        source = False
        for y, row in enumerate(console.model.grid):
            text = ''.join(row)
            if '[src' in text:
                source = True
                continue
            if source and text.startswith('+- ['):
                break
            match = re.search(r'\| (=>|N>) (\d+) ', text) if source else None
            if match:
                require(match[1] == marker, (action, text))
                colors = console.model.colors[y][match.start(1):match.end(2)]
                require(all(color == 93 for color in colors), (action, text, colors))
                checks.append({'action': action, 'prefix': match[0][2:].rstrip(),
                               'foreground': colors, 'screen': console.model.text()})
                return
        raise AssertionError((action, 'source marker missing', console.model.text()))

    try:
        console.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        console.drain()
        console.ask('start', b'stopped: breakpoint')
        console.send('tui enable\n')
        check_marker('=>', 'start')
        console.send('nw\n')
        check_marker('N>', 'nw')
        # Editing redraws only the input row. Retained source cells must keep
        # their colors even when no source prefix is retransmitted.
        console.send('draft')
        check_marker('N>', 'unsubmitted input')
        console.send(b'\x15')
        console.send('nw\n')
        check_marker('N>', 'second nw')
        console.send('start\n')
        check_marker('=>', 'start after nw')
        console.send('tui disable\n')
        console.finish()
        (args.out / 'result.json').write_text(json.dumps({
            'passed': True, 'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
            'checks': checks, 'terminal_restored': True}, ensure_ascii=False, indent=2))
        print('PASS source markers: => and N> with yellow line numbers after nw')
    except BaseException as exc:
        console.failure(args.out / 'source-marker', exc)
        raise
    finally:
        (args.out / 'transcript.bin').write_bytes(console.log)
        console.close()


if __name__ == '__main__':
    main()
