volatile int builtin_name_witness;
__attribute__((noinline)) int builtin_name_probe(int len, int cap)
{
    builtin_name_witness = len + cap; /* BUILTIN_NAMES_READY */
    return builtin_name_witness;
}
void _start(void)
{
    if (builtin_name_probe(7, 11) != 18) __builtin_trap();
}
