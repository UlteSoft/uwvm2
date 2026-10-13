int debug_source_c(int value);

__attribute__((export_name("_start")))
void _start(void)
{
    volatile int result = debug_source_c(5);
    if (result != 36) __builtin_trap();
}

__attribute__((export_name("debug_source_c")))
int debug_source_c(int value)
{
    int adjusted = value + 7;
    return adjusted * 3;
}
