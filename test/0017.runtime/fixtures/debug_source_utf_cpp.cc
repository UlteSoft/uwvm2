// Real C++20 UTF-8/16/32 type DWARF. Compile for Wasm32 with -O0 -g
// -gdwarf-5 -ffreestanding -nostdlib -fno-exceptions -fno-rtti.
struct UtfPacketCpp
{
    char8_t octet;
    char16_t unit;
    char32_t point;
    char16_t pair[2];
};
static_assert(sizeof(char8_t) == 1 && sizeof(char16_t) == 2 && sizeof(char32_t) == 4);
static_assert(sizeof(UtfPacketCpp) == 12);
static volatile unsigned int utf_observed_cpp;
extern "C" __attribute__((noinline, export_name("debug_utf_outer_cpp")))
unsigned int debug_utf_outer_cpp(unsigned int seed)
{
    UtfPacketCpp object{static_cast<char8_t>(0x80u), u'\u03bb', U'\U0001f642',
        {static_cast<char16_t>(0xd83du), static_cast<char16_t>(0xde42u)}};
    unsigned int result = static_cast<unsigned int>(object.octet) + static_cast<unsigned int>(object.unit) +
        static_cast<unsigned int>(object.point) + static_cast<unsigned int>(object.pair[0]) +
        static_cast<unsigned int>(object.pair[1]) + seed;
    utf_observed_cpp = result; // UTF_CPP_STOP: real UTF fields remain in scope.
    return result;
}
extern "C" __attribute__((export_name("_start"))) void _start()
{ if(debug_utf_outer_cpp(7u) != 0x3b103u) { __builtin_trap(); } }
