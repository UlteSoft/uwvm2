/* Real -g producer and guest oracle for enum display inside copied aggregates. */
#ifdef __cplusplus
enum class ObjectShade : int { negative = -1, warm = 3 };
enum class ObjectSmall : signed char { negative = -3 };
enum class ObjectWide : unsigned long long { maximum = 18446744073709551615ULL };
#define SHADE_WARM ObjectShade::warm
#define SHADE_NEGATIVE ObjectShade::negative
#define SHADE_UNKNOWN static_cast<ObjectShade>(13)
#else
typedef enum ObjectShade { negative = -1, warm = 3 } ObjectShade;
#define SHADE_WARM warm
#define SHADE_NEGATIVE negative
#define SHADE_UNKNOWN ((ObjectShade)13)
#endif
typedef struct ObjectPacket {
    int value;
    ObjectShade shade;
    ObjectShade negative_shade;
    ObjectShade unnamed_shade;
#ifdef __cplusplus
    ObjectSmall small;
    ObjectWide wide;
#endif
    int grid[2][3];
    struct ObjectPacket* next;
} ObjectPacket;
static volatile int object_observed;
#ifdef __cplusplus
extern "C"
#endif
__attribute__((noinline, export_name("source_enum_probe")))
int source_enum_probe(int seed)
{
    ObjectPacket object;
    object.value = seed;
    object.shade = SHADE_WARM;
    object.negative_shade = SHADE_NEGATIVE;
    object.unnamed_shade = SHADE_UNKNOWN;
#ifdef __cplusplus
    object.small = ObjectSmall::negative;
    object.wide = ObjectWide::maximum;
#endif
    object.grid[0][0] = 1; object.grid[0][1] = 2; object.grid[0][2] = 3;
    object.grid[1][0] = 4; object.grid[1][1] = 5; object.grid[1][2] = 6;
    object.next = &object;
    object_observed = object.value; /* OBJECT_ENUM_DAP_STOP */
    int result = object.value + (int)object.shade + (int)object.negative_shade
        + (int)object.unnamed_shade;
    for(int i = 0; i != 2; ++i)
        for(int j = 0; j != 3; ++j) result += object.grid[i][j];
    if(object.next != &object) __builtin_trap();
#ifdef __cplusplus
    if((int)object.small != -3 || (unsigned long long)object.wide != 18446744073709551615ULL)
        __builtin_trap();
#endif
    return result;
}
#ifdef __cplusplus
extern "C"
#endif
__attribute__((export_name("_start"))) void _start(void)
{
    /* A bounded repeating guest remains debuggable after asynchronous pause. */
    for(int iteration = 0; iteration != 4096; ++iteration)
        if(source_enum_probe(5) != 41) __builtin_trap();
}
