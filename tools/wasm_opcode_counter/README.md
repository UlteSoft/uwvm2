# wasm_opcode_counter

Count opcode occurrences in a `.wasm` file **code** section, e.g. how many `i32.const` instructions.

## Usage

This tool requires choosing exactly one mode: `--mnemonic ...` or `--top N`.

### Count a specific mnemonic

Count how many times a single instruction appears:

```bash
python3 tools/wasm_opcode_counter/opcode_counter.py build/wasm-wasip1/wasm32/release/uwvm.wasm --mnemonic i32.const
```

Options:

- `--mnemonic <name>`: mnemonic to count, e.g. `i32.const`, `local.get`, `call`
- `--exclude-structural`: exclude `block/loop/if/else/end`
- `--per-function`: also output per-function counts (function indices are 0-based within the code section)
- `--json`: output JSON

### Show top opcodes

Show the most frequent instructions:

```bash
python3 tools/wasm_opcode_counter/opcode_counter.py build/wasm-wasip1/wasm32/release/uwvm.wasm --top 50
```

Options:

- `--top N`: print top-N mnemonics
- `--exclude-structural`: exclude `block/loop/if/else/end`
- `--per-function`: also output per-function counts
- `--json`: output JSON
