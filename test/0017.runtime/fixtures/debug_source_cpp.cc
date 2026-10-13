extern "C" int debug_source_cpp(int value);

extern "C" __attribute__((export_name("_start")))
void _start()
{
    volatile int result = debug_source_cpp(5);
    if (result != 13) __builtin_trap();
}

template <typename T> static T twice(T value) { return value + value; }

extern "C" __attribute__((export_name("debug_source_cpp")))
int debug_source_cpp(int value)
{
    int adjusted = twice(value + 2);
    return adjusted - 1;
}
