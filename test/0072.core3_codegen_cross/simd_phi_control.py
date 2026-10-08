#!/usr/bin/env python3
"""Verify v128 control-flow joins and their real JIT debug operand snapshots.

Run through an owned cgroup supervisor. The prefix may select genuine QEMU;
this is Wasm-level coverage and acquires no native-address debugging authority.
"""
from pathlib import Path
import hashlib, json, os, selectors, subprocess, sys, time
r = Path(__file__).resolve().parent
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()

def literal(value):
    return 'v128.const i8x16 ' + ' '.join(str(x) for x in value)

def fixture():
    a = bytes.fromhex('ffffffff000000804523817f01000000')
    b = bytes.fromhex('5634c2ff0000803f0000000001000080')
    pick = '''(func $pick (param $flag i32) (param $a v128) (param $b v128) (result v128) (local v128)
      block $out (result v128)
        local.get $a local.get $flag br_if $out
        drop local.get $b
      end
      f32x4.abs local.tee 3)'''
    swap = '''(func $swap (param $n i32) (param $a v128) (param $b v128) (result v128 v128 v128) (local $original v128)
      local.get $a local.set $original
      block $out (result v128 v128)
        local.get $a local.get $b local.get $n
        loop $again (param v128 v128 i32) (result v128 v128)
          local.set $n local.set $b local.set $a
          local.get $n i32.eqz
          if local.get $a local.get $b br $out end
          local.get $b local.get $a local.get $n i32.const 1 i32.sub br $again
        end
      end
      local.get $original)'''
    body = []
    checks = 0
    # The first two calls are also the actual paused debug oracle.
    for flag in (0, 1):
        expected = bytearray(a if flag else b)
        for i in (3, 7, 11, 15):
            expected[i] &= 127
        body.append('i32.const ' + str(flag) + ' ' + literal(a) + ' ' + literal(b) +
                    ' call $pick ' + literal(expected) + ' i8x16.ne v128.any_true if unreachable end')
        checks += 16
    seed = 0xa24b64bdd548917f
    for sample in range(16):
        values = []
        for _ in range(32):
            seed ^= (seed << 13) & ((1 << 64) - 1)
            seed ^= seed >> 7
            seed ^= (seed << 17) & ((1 << 64) - 1)
            values.append(seed & 255)
        x, y = bytes(values[:16]), bytes(values[16:])
        for n in range(10):
            body.append('i32.const ' + str(n) + ' ' + literal(x) + ' ' + literal(y) +
                        ' call $swap local.set 2 local.set 1 local.set 0')
            for index, expected in enumerate((y if n % 2 else x, x if n % 2 else y, x)):
                body.append('local.get ' + str(index) + ' ' + literal(expected) +
                            ' i8x16.ne v128.any_true if unreachable end')
                checks += 16
    wat = '(module\n' + pick + '\n' + swap + '\n(func (export "_start") (local v128 v128 v128)\n' + '\n'.join(body) + '))\n'
    expected_debug = []
    for value in (b, a):
        value = bytearray(value)
        for i in (3, 7, 11, 15):
            value[i] &= 127
        expected_debug.append(bytes(value).hex())
    return wat, checks, expected_debug

def first_expression(data):
    def leb(at):
        value = 0
        for shift in range(0, 70, 7):
            byte = data[at]; at += 1; value |= (byte & 127) << shift
            if byte < 128:
                return value, at
        raise AssertionError('invalid Wasm LEB')
    at = 8
    assert data[:8] == b'\0asm\x01\0\0\0'
    while at < len(data):
        kind = data[at]; at += 1
        size, at = leb(at); end = at + size
        if kind == 10:
            count, at = leb(at); assert count == 3
            size, at = leb(at); body_end = at + size
            groups, at = leb(at)
            for _ in range(groups):
                _, at = leb(at); assert data[at] == 0x7b; at += 1
            return data[at:body_end]
        at = end
    raise AssertionError('no code section')

class Console:
    def __init__(self, argv, log):
        self.log_path = log; self.log = bytearray(); self.pending = bytearray()
        self.child = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.child.stdout, selectors.EVENT_READ)
        try:
            self.prompt()
        except BaseException:
            self.close(False)
            raise
    def prompt(self):
        deadline = time.monotonic() + 120
        marker = b'(uwvm-debug) '
        while marker not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0, ('prompt timeout', bytes(self.log)[-4000:])
            assert self.selector.select(remaining), ('prompt timeout', bytes(self.log)[-4000:])
            chunk = os.read(self.child.stdout.fileno(), 65536)
            assert chunk, ('early exit', self.child.poll(), bytes(self.log)[-4000:])
            self.pending.extend(chunk); self.log.extend(chunk)
        end = self.pending.index(marker) + len(marker)
        reply = bytes(self.pending[:end]); del self.pending[:end]
        return reply
    def send(self, value):
        self.child.stdin.write(value.encode() + b'\n'); self.child.stdin.flush()
        return self.prompt()
    def until(self, marker):
        deadline = time.monotonic() + 60
        while True:
            reply = self.send('status')
            if marker in reply:
                return reply
            assert time.monotonic() < deadline, ('state timeout', marker, reply)
            time.sleep(.02)
    def close(self, successful):
        try:
            if successful:
                self.child.stdin.write(b'quit\n'); self.child.stdin.flush(); self.child.stdin.close()
                assert self.child.wait(timeout=30) == 0
                self.log.extend(self.child.stdout.read())
            else:
                if self.child.poll() is None:
                    self.child.kill()
                self.child.wait(timeout=30)
        finally:
            self.log_path.write_bytes(self.log); self.selector.close()

def main():
    c = json.loads(Path(sys.argv[1]).read_text())
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    for p, h in c['pins'].items():
        assert sha(p) == h, ('changed bound input', p)
    q = json.loads(Path(c['product_qualification']).read_text())
    assert q['passed'] and q['product_sha256'] == sha(c['product'])
    assert q['source_identities'] == c['source_identities']
    semantic_receipt = json.loads(Path(c['semantic_execution_receipt']).read_text())
    semantic_plan = json.loads(Path(c['semantic_plan']).read_text())
    assert len(semantic_receipt) == 1 and semantic_receipt[0]['passed'] and semantic_receipt[0]['returncode'] == 0
    assert semantic_receipt[0]['argv'] == semantic_plan[c['semantic_plan_index']][1]
    prior_sha = sha(c['product_qualification'])
    out = Path(c['output']); out.mkdir(exist_ok=False)
    wat, checks, expected_debug = fixture()
    wp, wasm = out/'phi.wat', out/'phi.wasm'; wp.write_text(wat)
    commands = []
    for label, argv in [('parse', [c['wasm_tools'], 'parse', str(wp), '-o', str(wasm)]),
                        ('validate', [c['wasm_tools'], 'validate', '--features', 'all', str(wasm)])]:
        cp = subprocess.run(argv, capture_output=True, timeout=60)
        (out/(label+'.log')).write_bytes(cp.stdout+cp.stderr)
        assert cp.returncode == 0, (label, cp.stderr)
        commands.append(dict(label=label, argv=argv, exit=cp.returncode))
    expression = first_expression(wasm.read_bytes())
    opcode = bytes.fromhex('fde001')
    assert expression.count(opcode) == 1
    pause = expression.index(opcode) + len(opcode)
    assert expression[pause:pause+2] == bytes.fromhex('2203')
    rows = []
    for level in range(4):
        policy = 'debug' if level == 0 else 'pb-o'+str(level)
        args = (['-Raot'] if c['ros'] else ['-Rcc', 'jit', '-Rcm', 'full'])
        base = [*c['prefix'], c['product'], *args, '-Rllvm-full-policy', policy,
                '-Rct', '0', '-Rllvm-cache-path', 'disable', '-WFE-simd']
        argv = [*base, '--run', str(wasm)]
        cp = subprocess.run(argv, capture_output=True, timeout=180)
        log = out/('O'+str(level)+'-semantic.log'); log.write_bytes(cp.stdout+cp.stderr)
        assert cp.returncode == 0, (argv, cp.returncode, cp.stderr[-4000:])
        rows.append(dict(level=level, semantic_checks=checks, log_sha256=sha(log), argv=argv, exit=0))
        debug_argv = [*base, '-Rdbg', '--run', str(wasm)]
        console = None; actions = []; success = False
        try:
            console = Console(debug_argv, out/('O'+str(level)+'-debug.log'))
            reply = console.send('break 0 0 '+str(pause))
            assert b'breakpoint 1' in reply, reply
            for expected in expected_debug:
                console.send('continue'); console.until(b'stopped: breakpoint')
                reply = console.send('operands 1 0 0 16')
                assert b'Note: Last Wasm safepoint snapshot' in reply and b'v128 = bytes='+expected.encode() in reply, reply
                assert b'unavailable' not in reply and reply == console.send('operands 1 0 0 16'), reply
                actions.append(dict(command='operands 1 0 0 16', expected_bytes=expected, reply=reply.decode()))
            assert b'breakpoint deleted' in console.send('delete 1')
            console.send('continue'); console.until(b'guest exited: 0')
            success = True
        finally:
            if console is not None:
                console.close(success)
            (out/('O'+str(level)+'-debug-actions.json')).write_text(json.dumps(actions, indent=2)+'\n')
        assert success
        rows[-1].update(debug_argv=debug_argv, debug_checks=checks, debug_values=len(actions),
                        debug_log_sha256=sha(out/('O'+str(level)+'-debug.log')))
        print('PASS v128 joins and exact debug operands', c['repo'], 'O'+str(level), flush=True)
    for p, h in c['pins'].items():
        assert sha(p) == h
    assert sha(c['product_qualification']) == prior_sha
    (out/'qualification.json').write_text(json.dumps(dict(passed=True, product_sha256=sha(c['product']),
        source_identities=q['source_identities'], product_qualification_sha256=prior_sha, semantic_execution_receipt_sha256=sha(c['semantic_execution_receipt']),
        config_sha256=sha(sys.argv[1]), test_code_sha256=sha(__file__), wat_sha256=sha(wp), wasm_sha256=sha(wasm),
        pause_offset=pause, commands=commands, rows=rows, semantic_checks=sum(v['semantic_checks'] for v in rows),
        debug_checks=sum(v['debug_checks'] for v in rows), debug_values=sum(v['debug_values'] for v in rows),
        scope='Genuine full O0/O1/O2/O3 v128 branch joins, typed loop parameters, multi-value results, call ABI and exact opcode-stop operands; no ASM debugger access, WASIp1 or whole-Core3 claim'), indent=2)+'\n')

if __name__ == '__main__':
    main()
