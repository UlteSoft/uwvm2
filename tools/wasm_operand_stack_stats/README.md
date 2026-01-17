# wasm_operand_stack_stats

Count operand stack height after each opcode in the Wasm **code** section (Wasm 1.0 / MVP).

## Usage

```bash
python3 tools/wasm_operand_stack_stats/stack_stats.py build/wasm-wasip1/wasm32/release/uwvm.wasm --threshold 1
```

By default it **does not** count structural opcodes: `block`, `loop`, `if`, `else`, `end` (matches the example expectation).

To include them:

```bash
python3 tools/wasm_operand_stack_stats/stack_stats.py build/wasm-wasip1/wasm32/release/uwvm.wasm --threshold 1 --include-structural
```

