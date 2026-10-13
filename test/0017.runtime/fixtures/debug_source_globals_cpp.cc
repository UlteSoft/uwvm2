namespace outer {
int public_counter = 19;
static int file_counter = 23;
class Counter {
public:
    static int class_counter;
};
int Counter::class_counter = 29;
}
extern "C" __attribute__((noinline, used)) int debug_source_globals_cpp(int seed)
{
    static int function_counter = 31;
    int public_counter = seed;
    function_counter += seed;
    return public_counter + outer::file_counter + function_counter + outer::Counter::class_counter;
}
extern "C" __attribute__((noinline, used)) int debug_source_global_public_cpp() { return outer::public_counter; }
