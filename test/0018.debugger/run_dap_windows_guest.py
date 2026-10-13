#!/usr/bin/env python3
"""Exercise the real Windows broker through DAP stdio without persisting its secret.

The trusted PowerShell host launcher sends one JSON line on anonymous stdin
with pipeName and capability. Neither value is accepted as a command argument
or included in this probe's result or logs.
"""

import argparse
import hashlib
import json
from pathlib import Path
import queue
import subprocess
import sys
import threading


MAX_MESSAGE = 1 << 20


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_exact(stream, count: int) -> bytes:
    value = bytearray()
    while len(value) != count:
        part = stream.read(count - len(value))
        if not part:
            raise EOFError('DAP output closed')
        value.extend(part)
    return bytes(value)


def read_message(stream) -> dict:
    total = 0
    length = None
    while True:
        line = stream.readline(4097)
        total += len(line)
        if not line or total > 4096 or not line.endswith(b'\r\n'):
            raise EOFError('invalid DAP output header')
        if line == b'\r\n':
            break
        if line.startswith(b'Content-Length: ') and length is None:
            text = line[len(b'Content-Length: '):-2]
            if not text.isdigit() or len(text) > 7:
                raise ValueError('invalid DAP output size')
            length = int(text)
    if length is None or not 2 <= length <= MAX_MESSAGE:
        raise ValueError('DAP output size out of bounds')
    message = json.loads(read_exact(stream, length))
    if not isinstance(message, dict):
        raise ValueError('DAP output is not an object')
    return message


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adapter', required=True, type=Path)
    parser.add_argument('--adapter-sha256', required=True)
    parser.add_argument('--source-step', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('real Win11 Python is required')
    adapter = args.adapter.resolve(strict=True)
    if digest(adapter).lower() != args.adapter_sha256.lower():
        raise ValueError('staged DAP adapter hash changed')
    host_data = json.loads(sys.stdin.readline(MAX_MESSAGE))
    if (not isinstance(host_data, dict) or
            not isinstance(host_data.get('pipeName'), str) or
            not isinstance(host_data.get('capability'), str) or
            len(host_data['capability']) != 64):
        raise ValueError('trusted host did not provide an in-memory capability')
    process = subprocess.Popen([sys.executable, str(adapter)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                               bufsize=0)
    messages = queue.Queue()
    def reader() -> None:
        try:
            while True:
                messages.put(read_message(process.stdout))
        except (EOFError, ValueError, json.JSONDecodeError) as error:
            messages.put(error)
    threading.Thread(target=reader, daemon=True).start()
    checked = []
    events = []
    def request(number: int, command: str, arguments=None) -> dict:
        payload = json.dumps({'seq': number, 'type': 'request', 'command': command,
                              'arguments': arguments or {}}, separators=(',', ':')).encode()
        process.stdin.write(b'Content-Length: ' + str(len(payload)).encode() +
                            b'\r\n\r\n' + payload)
        process.stdin.flush()
        for _ in range(64):
            message = messages.get(timeout=20)
            if isinstance(message, BaseException):
                raise RuntimeError('DAP adapter stopped before replying')
            if message.get('type') == 'event':
                name = message.get('event')
                if isinstance(name, str) and len(name) < 64:
                    events.append(name)
            if message.get('type') == 'response' and message.get('request_seq') == number:
                if message.get('command') != command or message.get('success') is not True:
                    raise RuntimeError('DAP request was rejected: ' + command)
                checked.append(command)
                return message
        raise RuntimeError('DAP response budget exceeded: ' + command)
    try:
        request(1, 'initialize', {'adapterID': 'uwvm-llvm-full'})
        request(2, 'attach', {'stepLevel': 'wasm', 'pipeName': host_data['pipeName'],
                              'capability': host_data['capability'], 'moduleId': 0})
        host_data = None
        request(3, 'configurationDone')
        request(4, 'pause', {'threadId': 1})
        threads = request(5, 'threads').get('body', {}).get('threads', [])
        if not threads or threads[0].get('id') != 1:
            raise RuntimeError('DAP did not expose the selected Wasm thread')
        request(6, 'stepIn', {'threadId': 1, 'granularity': 'statement'})
        sequence = 7
        if args.source_step:
            request(sequence, 'stepIn', {'threadId': 1, 'granularity': 'line'})
            sequence += 1
            frames = request(sequence, 'stackTrace', {'threadId': 1}).get('body', {}).get('stackFrames', [])
            if not frames or not frames[0].get('source', {}).get('path'):
                raise RuntimeError('DAP source frame has no embedded source path')
            sequence += 1
        request(sequence, 'stepIn', {'threadId': 1, 'granularity': 'instruction'})
        request(sequence + 1, 'disconnect')
        if 'initialized' not in events or 'stopped' not in events:
            raise RuntimeError('DAP omitted initialized or stopped event')
        print(json.dumps({'passed': True, 'platform': 'win32',
                          'adapter_sha256': args.adapter_sha256.lower(),
                          'checked_commands': checked,
                          'events': sorted(set(events)),
                          'source_step': args.source_step}, separators=(',', ':')))
        return 0
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=10)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, queue.Empty, TimeoutError) as error:
        print(json.dumps({'passed': False, 'platform': 'win32',
                          'failure_type': type(error).__name__}, separators=(',', ':')))
        raise SystemExit(1)
