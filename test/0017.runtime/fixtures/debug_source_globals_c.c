// DWARF4 DW_OP_addr and DWARF5 DW_OP_addrx are separately qualified after link.
int public_counter = 19;
static int file_counter = 23;
__attribute__((noinline, used)) int debug_source_globals_c(int seed)
{
    static int function_counter = 29;
    int public_counter = seed; // actual lexical shadowing must remain intact.
    function_counter += seed;
    return public_counter + file_counter + function_counter;
}
__attribute__((noinline, used)) int debug_source_global_public_c(void) { return public_counter; }
