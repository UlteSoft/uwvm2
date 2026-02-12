# wasm_operand_stack_stats

Count operand stack height after each opcode in the Wasm **code** section (Wasm 1.0 / MVP).

## Usage

```bash
python3 tools/wasm_operand_stack_stats/stack_stats.py build/wasm-wasip1/wasm32/release/uwvm.wasm --threshold 1
```

By default it **does not** count structural opcodes: `block`, `loop`, `if`, `else`, `end` (matches the example expectation).
The output also includes a `stack_histogram` that counts how many times each operand stack height occurs after an instruction.

To include them:

```bash
python3 tools/wasm_operand_stack_stats/stack_stats.py build/wasm-wasip1/wasm32/release/uwvm.wasm --threshold 1 --include-structural
```

## Parameters

- `--threshold N` (required): count `> threshold` vs `<= threshold` based on the operand stack height **after** each counted instruction
- `--include-structural`: also count `block/loop/if/else/end` in the statistics
- `--per-function`: also output per-function stats (function indices are the module function indices; imported functions are skipped)
- `--json`: output JSON (includes `stack_histogram`)
