// Exact C++ language fixture; no external standard-library/runtime imports.
static volatile int seed = 5;
static volatile int observed;

__attribute__((always_inline)) static inline int source_step_inner(int value)
{
    int inner = value + 2;
    observed = inner; /* STEP_INNER_WRITE */
    return inner * 2;
}
__attribute__((always_inline)) static inline int source_step_middle(int value)
{
    int middle = source_step_inner(value);
    observed = middle; /* STEP_MIDDLE_AFTER */
    return middle + 3;
}
extern "C" __attribute__((noinline, export_name("source_step_leaf")))
int source_step_leaf(int value)
{
    observed = value; /* STEP_LEAF_ENTRY */
    return value + 11;
}
extern "C" __attribute__((noinline, export_name("source_step_outer")))
int source_step_outer(int value)
{
    observed = value; /* STEP_BEFORE_INLINE */
    int first = source_step_middle(value); /* STEP_INLINE_ONE */
    int second = source_step_middle(first); /* STEP_INLINE_TWO */
    int physical = source_step_leaf(second); /* STEP_PHYSICAL_CALL */
    observed = physical; /* STEP_AFTER_CALL */
    return physical;
}
extern "C" __attribute__((noinline, export_name("source_step_recursive")))
int source_step_recursive(int remaining, int value)
{
    observed = remaining; /* STEP_RECURSIVE_ENTRY */
    if (remaining == 0) return value;
    int previous = source_step_recursive(remaining - 1, value + 1);
    observed = previous; /* STEP_RECURSIVE_RETURN */
    return previous + 1;
}
extern "C" __attribute__((export_name("_start"))) void _start(void)
{
    int result = source_step_outer(seed);
    if (result != 52) __builtin_trap();
    if (source_step_recursive(3, result) != 58) __builtin_trap();
}
