static volatile int seed = 5;
static volatile int observed;

__attribute__((always_inline)) static inline int source_inner_c(int inner_arg)
{
    int adjusted = inner_arg + 7;
    return adjusted * 3;
}

__attribute__((noinline, export_name("source_outer_c")))
int source_outer_c(int value)
{
    int adjusted = source_inner_c(value);
    observed = adjusted;
    return adjusted;
}

__attribute__((export_name("_start"))) void _start(void)
{
    if (source_outer_c(seed) != 36) __builtin_trap();
}
