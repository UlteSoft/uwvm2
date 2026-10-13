struct payload { int value; };
static volatile int conditional_observed;
extern "C" __attribute__((noinline, export_name("conditional_target")))
int conditional_target(int parameter)
{
    int counter{};
    volatile payload cell{};
    for(;;)
    {
        ++counter;
        cell.value = counter;
        conditional_observed = parameter + counter; // CONDITION_READY
        asm volatile("" : : "r"(parameter), "r"(counter), "r"(&cell) : "memory");
    }
}
extern "C" __attribute__((export_name("_start"))) void _start()
{ conditional_observed = conditional_target(7); }
