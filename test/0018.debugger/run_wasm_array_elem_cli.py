#!/usr/bin/env python3
"""Run live array.new_elem/array.init_elem checks in the Linux test cgroup."""
import run_wasm_live_containers_cli as containers
from wasm_array_elem_cases import array_elem_examples


if __name__ == '__main__':
    containers._cases = {case['name']: case for case in array_elem_examples()}
    containers.preview.examples = array_elem_examples
    containers.preview.OperandConsole = containers.ContainerConsole
    containers.preview.operands = containers.inspect
    containers.preview.main()
