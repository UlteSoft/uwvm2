#!/usr/bin/env python3
"""Check empty table32/table64 pages and real grow/dispatch in the Linux cgroup."""
import run_wasm_live_containers_cli as containers
from wasm_container_boundary_cases import empty_table_examples


if __name__ == '__main__':
    containers._cases = {case['name']: case for case in empty_table_examples()}
    containers.preview.examples = empty_table_examples
    containers.preview.OperandConsole = containers.ContainerConsole
    containers.preview.operands = containers.inspect
    containers.preview.main()
