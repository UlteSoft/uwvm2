// Real C++ instance method and its DWARF `this` parameter, with a guest result
// check. This is not a C function given a C++ source suffix.
volatile int observed;
struct LanguageThis
{
    int value;
    __attribute__((noinline, export_name("numeric_probe"))) int add(int amount) const
    {
        volatile int cookie = value + amount;
        volatile float decimal32 = 1.25f;
        volatile double decimal64 = -2.5;
        LanguageThis const * const saved_this = this;
        LanguageThis const & reference = *this;
        LanguageThis const && rvalue_reference = static_cast<LanguageThis const &&>(*this);
        LanguageThis const * const * pointer_to_saved = &saved_this;
        using ConstProbe = LanguageThis const;
        ConstProbe * alias_this = this;
        observed = saved_this->value + reference.value + rvalue_reference.value + (*pointer_to_saved)->value + alias_this->value;
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
