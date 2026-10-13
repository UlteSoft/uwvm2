"""Reuse pinned checked Core 3 workloads; do not mistake a witness for exhaustiveness."""
import importlib.util


def cases(generator_path, count=2048):
    spec = importlib.util.spec_from_file_location('pinned_checked_core_generator', generator_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = []
    for name in module.CASES:
        # This witness declares >4 GiB of logical guest memory (mmap can commit
        # it sparsely). It needs a separate address-space/admission budget and
        # cannot be silently included in the ordinary 4 GiB runner.
        if name == 'memory64-high-random-store':
            continue
        wat, features = module.module(name, count)
        result.append(('core-' + name, wat, count, ['-WFE-' + feature for feature in features]))
    return result
