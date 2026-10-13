// Freestanding DWARF producer for real console completion and TUI tests.
struct CompletionPair { int first; int second; };
static volatile int completion_observed;
extern "C" __attribute__((noinline, export_name("completion_target")))
int completion_target(int parameter)
{
    CompletionPair packet{parameter, 19};
    CompletionPair* pointer = &packet;
    int first = parameter + 3;
    for(;;)
    {
        completion_observed = first + pointer->first + packet.second; // COMPLETION_READY
        asm volatile("" : : "r"(parameter), "r"(first), "r"(&packet), "r"(pointer) : "memory");
    }
}
extern "C" __attribute__((export_name("_start"))) void _start()
{
    completion_observed = completion_target(7);
}
