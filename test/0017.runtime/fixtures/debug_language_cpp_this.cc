// Real C++ instance method and its DWARF `this` parameter, with a guest result
// check. This is not a C function given a C++ source suffix.
volatile int observed;
struct LanguageThis
{
    int value;
    __attribute__((noinline, export_name("numeric_probe"))) int add(int amount)
    {
        volatile int cookie = value + amount;
        volatile float decimal32 = 1.25f;
        volatile double decimal64 = -2.5;
        observed = cookie; /* NUMERIC_READY */
        observed = static_cast<int>(decimal32 + decimal64);
        return cookie;
    }
};
extern "C" __attribute__((export_name("_start"))) void start()
{
    LanguageThis object{7};
    if(object.add(4) != 11) { __builtin_trap(); }
}
