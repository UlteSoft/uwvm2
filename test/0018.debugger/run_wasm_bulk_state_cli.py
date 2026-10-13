#!/usr/bin/env python3
"""Run live bulk-memory/table stack and state checks in the Linux test cgroup.

Uses the existing CLI lifecycle and exact-stack assertions. Select cases and
target prefixes with the same arguments as run_wasm_operand_preview_cli.py.
"""
import run_wasm_operand_preview_cli as preview
from wasm_operand_bulk_state_cases import bulk_state_examples


if __name__ == '__main__':
    preview.examples = bulk_state_examples
    preview.main()
