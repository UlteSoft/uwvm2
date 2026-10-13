/* Real compiler DWARF: caller frames, inline scopes and owned stack objects. */
static volatile int observed;
static volatile float observed_fraction;
static volatile double observed_double;
struct LanguagePacket { int tag; int grid[2][3]; };
__attribute__((always_inline)) static inline int language_inner_c(int value)
{
    volatile int shadow = value + 101;
    observed = shadow; /* LANG_FRAME_SHADOW_INNER */
    volatile int inner_cookie = value + 1;
    observed = inner_cookie; /* LANG_INLINE_INNER */
    return inner_cookie * 2;
}
__attribute__((always_inline)) static inline int language_middle_c(int value)
{
    volatile int shadow = value + 201;
    observed = shadow; /* LANG_FRAME_SHADOW_MIDDLE */
    volatile int middle_cookie = language_inner_c(value);
    observed = middle_cookie; /* LANG_INLINE_MIDDLE */
    return middle_cookie + 3;
}
__attribute__((noinline, export_name("language_leaf_c"))) int language_leaf_c(int value)
{
    const int leaf_negative = -31;
    const unsigned int leaf_positive = 23u;
    const float leaf_fraction = 1.25f;
    const double leaf_double = -2.5;
    const float leaf_zero = -0.0f;
    const _Bool leaf_boolean = 1;
    observed = leaf_negative; observed = (int)leaf_positive; observed = (int)leaf_boolean; /* LANG_CONSTANT_VALUES */
    observed_fraction = leaf_fraction; observed_double = leaf_double; observed_fraction = leaf_zero;
    volatile int leaf_cookie = value + 11;
    observed = leaf_cookie; /* LANG_LEAF_READY */
    return leaf_cookie; /* LANG_LEAF_RETURN */
}
__attribute__((noinline, export_name("language_outer_c"))) int language_outer_c(int value)
{
    volatile struct LanguagePacket packet;
    packet.tag = value;
    packet.grid[0][0] = 10; packet.grid[0][1] = 11; packet.grid[0][2] = 12;
    packet.grid[1][0] = 13; packet.grid[1][1] = 14; packet.grid[1][2] = 15;
    volatile int shadow = 7;
    observed = packet.grid[1][2]; /* LANG_OBJECT_READY */
    int nested = language_middle_c(packet.tag); /* LANG_BEFORE_INLINE */
    int child;
    {
        volatile int shadow = 23;
        observed = shadow; /* LANG_INNER_SHADOW */
        child = language_leaf_c(5); /* LANG_PHYSICAL_CALL */
        observed = child; /* LANG_AFTER_CALL */
    }
    observed = shadow; /* LANG_OUTER_SHADOW */
    return packet.grid[1][2] + nested + child;
}
__attribute__((export_name("_start"))) void _start(void)
{
    if(language_outer_c(3) != 42) __builtin_trap();
}
