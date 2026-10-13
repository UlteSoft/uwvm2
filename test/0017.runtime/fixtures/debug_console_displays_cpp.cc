// Real -g guest producer. The repeated line is used for native hit-policy tests.
static volatile int display_observed;
extern "C" __attribute__((noinline, export_name("display_target")))
int display_target(int parameter)
{
    int counter{};
    for(;;)
    {
        ++counter;
        display_observed = parameter + counter; // DISPLAY_READY
        asm volatile("" : : "r"(parameter), "r"(counter) : "memory");
    }
}
extern "C" __attribute__((export_name("_start"))) void _start()
{ display_observed = display_target(7); }
