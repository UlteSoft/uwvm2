"""24 lane checks per call, with actual T2 entry verified by the runner.

The 4M calls include T1 warmup; the runner does not credit every call to T2.
Full boundary coverage belongs to the separate unary/native component corpus.
"""
from ieee_oracle import IEEE
from numeric_cases import criterion


def fixture():
    globals_, checks = [], []
    for width in (32, 64):
        ieee = IEEE(width)
        lanes = 128 // width
        limit = (ieee.bias + ieee.frac) << ieee.frac
        values = ([((ieee.bias + 1) << ieee.frac) | (1 << (ieee.frac - 2)),
                   ieee.sign | ((ieee.bias - 1) << ieee.frac),
                   ieee.infinity | 1, limit - 1] if width == 32 else
                  [ieee.sign | 1, ieee.sign | (ieee.infinity | 17)])
        integer, floating = f'i{width}x{lanes}', f'f{width}x{lanes}'
        globals_.append(f'(global $a{width} (mut v128) '
                        f'(v128.const {integer} {" ".join(map(str, values))}))')
        for operation in ('ceil', 'floor', 'trunc', 'nearest'):
            checks.append(f'global.get $a{width} {floating}.{operation} local.set $v')
            for lane, value in enumerate(values):
                checks.append(f'local.get $v {integer}.extract_lane {lane} '
                              f'{criterion(f"i{width}", ieee.unary(operation, value))} if unreachable end')
    return f'''(module {' '.join(globals_)}
      (func $unused)
      (func $hot (result i32) (local $v v128) {' '.join(checks)} i32.const 42)
      (func (export "_start") (local $n i32)
        i32.const 4000000 local.set $n
        loop $warm call $hot i32.const 42 i32.ne if unreachable end
          local.get $n i32.const 1 i32.sub local.tee $n br_if $warm end))'''
