// Real C++ class/base/bit-field metadata; no C++ library or external imports.
static volatile int observed;
static volatile float observed_fraction;
static volatile double observed_double;
enum class LanguageMode : int { cold = -1, hot = 2 };
struct LanguageBase { int base; };
struct LanguagePacket : LanguageBase
{
    int tag;
    int grid[2][3];
    signed small : 5;
    unsigned flags : 7;
    LanguageMode mode;
};
__attribute__((always_inline)) static inline int language_inner_cpp(int value)
{
    volatile int shadow = value + 101;
    observed = shadow; /* LANG_FRAME_SHADOW_INNER */
    volatile int inner_cookie = value + 1;
    observed = inner_cookie; /* LANG_INLINE_INNER */
    return inner_cookie * 2;
}
__attribute__((always_inline)) static inline int language_middle_cpp(int value)
{
    volatile int shadow = value + 201;
    observed = shadow; /* LANG_FRAME_SHADOW_MIDDLE */
    volatile int middle_cookie = language_inner_cpp(value);
    observed = middle_cookie; /* LANG_INLINE_MIDDLE */
    return middle_cookie + 3;
}
extern "C" __attribute__((noinline, export_name("language_leaf_cpp"))) int language_leaf_cpp(int value)
{
    const int leaf_negative = -31;
    const unsigned int leaf_positive = 23u;
    const float leaf_fraction = 1.25f;
    const double leaf_double = -2.5;
    const float leaf_zero = -0.0f;
    const bool leaf_boolean = true;
    const char8_t leaf_octet = static_cast<char8_t>(0x80u);
    const char16_t leaf_unit = u'\u03bb';
    const char32_t leaf_point = U'\U0001f642';
    observed = static_cast<int>(leaf_octet);
    observed = static_cast<int>(leaf_unit);
    observed = static_cast<int>(leaf_point);
    observed = leaf_negative; observed = (int)leaf_positive; observed = (int)leaf_boolean; /* LANG_CONSTANT_VALUES */
    observed_fraction = leaf_fraction; observed_double = leaf_double; observed_fraction = leaf_zero;
    volatile int leaf_cookie = value + 11;
    observed = leaf_cookie; /* LANG_LEAF_READY */
    return leaf_cookie; /* LANG_LEAF_RETURN */
}
extern "C" __attribute__((noinline, export_name("language_outer_cpp"))) int language_outer_cpp(int value)
{
    volatile LanguagePacket packet;
    packet.base = 17; packet.tag = value;
    packet.grid[0][0] = 10; packet.grid[0][1] = 11; packet.grid[0][2] = 12;
    packet.grid[1][0] = 13; packet.grid[1][1] = 14; packet.grid[1][2] = 15;
    packet.small = -3; packet.flags = 42; packet.mode = LanguageMode::cold;
    volatile int shadow = 7;
    observed = packet.base + packet.small + static_cast<int>(packet.mode);
    observed = packet.grid[1][2]; /* LANG_OBJECT_READY */
    int nested = language_middle_cpp(packet.tag); /* LANG_BEFORE_INLINE */
    int child;
    {
        volatile int shadow = 23;
        observed = shadow; /* LANG_INNER_SHADOW */
        child = language_leaf_cpp(5); /* LANG_PHYSICAL_CALL */
        observed = child; /* LANG_AFTER_CALL */
    }
    observed = shadow; /* LANG_OUTER_SHADOW */
    return packet.grid[1][2] + nested + child;
}
extern "C" __attribute__((export_name("_start"))) void _start()
{
    if(language_outer_cpp(3) != 42) __builtin_trap();
}
