static volatile int seed = 5;
static volatile int observed;

template<typename T>
__attribute__((always_inline)) static inline T source_inner_cpp(T inner_arg)
{
    T adjusted = inner_arg + 2;
    return adjusted + adjusted;
}

extern "C" __attribute__((noinline, export_name("source_outer_cpp")))
int source_outer_cpp(int value)
{
    int adjusted = source_inner_cpp(value) - 1;
    observed = adjusted;
    return adjusted;
}

extern "C" __attribute__((export_name("_start"))) void _start()
{
    if (source_outer_cpp(seed) != 13) __builtin_trap();
}
