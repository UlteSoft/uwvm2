"""Check disassembler aliases without counting returns as tail transfers."""
from inspect_jit import instruction_evidence


def check():
    # LLVM's JR alias and canonical JIRL encode the same zero-link transfer.
    transfers = ('jr $t0', 'jr $a4', 'jirl $zero, $t0, 0',
                 'jirl $r0, $r12, 0')
    returns = ('ret', 'jr $ra', 'jr $r1', 'jirl $zero, $ra, 0',
               'jirl $r0, $r1, 0')
    calls = ('bl 0x40', 'jirl $ra, $t0, 0', 'jirl $r1, $r12, 0')
    for instruction in transfers + returns + calls:
        assembly = f'  10: {instruction}\n  14: R_LARCH_64 uwvm_m_123_func_1\n'
        result = instruction_evidence(assembly, 'loongarch64')
        assert result['instruction_lines'] == 1, (instruction, result)
        assert result['indirect_linkless_jumps'] == int(instruction in transfers), (instruction, result)
        assert result['link_setting_calls'] == int(instruction in calls), (instruction, result)
    print('PASS LoongArch instruction aliases, returns, calls and relocation exclusion: 12 cases')


if __name__ == '__main__':
    check()
