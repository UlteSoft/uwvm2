This test builds small `.wasm` modules from `.wat` and verifies runtime initializer dependency validation (import type checks).

Generate `.wasm`:
- `python3 test/0011.initializer/compile_wat.py`

Run initializer checks (calls `xmake run uwvm -- ...`):
- `python3 test/0011.initializer/run_initializer_checks.py`
