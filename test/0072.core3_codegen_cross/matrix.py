"""Explicit build and runtime axes; accepting a flag is not execution evidence."""

COMBINE_BUILDS = {
    'none': (),
    'soft': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS',),
    'heavy': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS', 'UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS'),
    'extra': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS', 'UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS',
              'UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS'),
}
DELAY_BUILDS = {
    'none': (),
    'soft': ('UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT',),
    'heavy': ('UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT', 'UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY'),
}
from architecture_catalog import LINUX_PROFILES

TARGETS = tuple(row['id'] for row in LINUX_PROFILES)
FEATURES = ('scalar-float', 'simd', 'relaxed-simd', 'tail-call', 'multi-value',
            'reference-types', 'function-references', 'gc', 'exceptions',
            'bulk-memory', 'multi-memory', 'memory64', 'table64', 'threads', 'extended-const')


def runtime_profiles(ros=False, all_interpreter_peepholes=False):
    rows = []
    for mode in (('full',) if ros else ('full', 'lazy', 'lazy+verification')):
        for combine in (('disable', 'soft', 'heavy', 'extra') if all_interpreter_peepholes else (None,)):
            for delayed in ((True, False) if all_interpreter_peepholes else (True,)):
                args = ['-Rint'] if ros else ['-Rcc', 'int', '-Rcm', mode]
                if combine is not None:
                    args += ['-Rint-op-conbine-level', combine]
                if not delayed:
                    args += ['-Rint-no-delay-local']
                rows.append(dict(name=f'int-{mode}-{combine or "compiled-default"}-delay-{int(delayed)}',
                    backend='uwvm-int', mode=mode, combine=combine, delay_local_requested=delayed,
                    effective_delay_local=(False if combine == 'disable' or not delayed else
                                           True if all_interpreter_peepholes else None),
                    delay_local_requires_combination=True,
                    requires_compiled_peepholes=all_interpreter_peepholes, argv=args))
    args = ['-Raot'] if ros else ['-Rcc', 'jit', '-Rcm', 'full']
    rows.append(dict(name='llvm-full-debug', backend='llvm-jit', mode='full',
                     optimization='O0', argv=args + ['-Rllvm-full-policy', 'debug']))
    for level in (1, 2, 3):
        args = ['-Raot'] if ros else ['-Rcc', 'jit', '-Rcm', 'full']
        rows.append(dict(name=f'llvm-full-O{level}', backend='llvm-jit', mode='full',
                         optimization=f'O{level}', argv=args + ['-Rllvm-full-policy', f'pb-o{level}']))
    if not ros:
        # Lazy has named scoped policies, rather than the full compiler's O1/2/3 interface.
        # The executed tier/optimization needs a separate witness before qualification.
        for policy in ('debug', 'light', 'balanced'):
            rows.append(dict(name=f'llvm-lazy-{policy}', backend='llvm-jit', mode='lazy',
                lazy_policy=policy, argv=['-Rcc', 'jit', '-Rcm', 'lazy', '-Rllvm-lazy-policy', policy]))
        for level in (1, 2, 3):
            for disable in ((), ('t0',), ('t2',), ('t0', 't2')):
                rows.append(dict(name=f'tiered-O{level}-' + ('enabled' if not disable else '-'.join(disable)),
                    backend='tiered', mode='tiered', optimization=f'O{level}',
                    execution_tier_verified=False,
                    argv=['-Rtiered', '-Rllvm-full-policy', f'pb-o{level}',
                          *['-Rtiered-disable-' + tier for tier in disable]]))
    return rows
