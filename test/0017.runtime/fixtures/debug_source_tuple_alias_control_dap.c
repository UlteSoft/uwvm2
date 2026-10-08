/* Identical producer field spellings in a non-Rust CU must remain exact names. */
struct Pair { int __0; unsigned __1; };
struct ReversePair { unsigned __0; int __1; };
struct Nested { struct Pair __0; struct ReversePair __1; };
struct TuplePacket {
    int seed;
    struct Pair pair;
    struct Pair tuple_struct;
    struct Nested nested;
    struct Pair arrays[2];
    struct TuplePacket const* next;
};
static volatile int TUPLE_OBSERVED;
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noinline)) int source_tuple_control_probe(int seed) {
    struct TuplePacket object = {
        seed, {-9, 42}, {-13, 55}, {{-7, 21}, {34, -11}}, {{-3, 8}, {-5, 13}}, 0
    };
    object.next = &object;
    TUPLE_OBSERVED = object.seed; /* OBJECT_TUPLE_DAP_STOP */
    if (object.next != &object) __builtin_trap();
    int pair_total = object.pair.__0 + (int)object.pair.__1;
    int struct_total = object.tuple_struct.__0 + (int)object.tuple_struct.__1;
    int nested_total = object.nested.__0.__0 + (int)object.nested.__0.__1
        + (int)object.nested.__1.__0 + object.nested.__1.__1;
    int arrays_total = object.arrays[0].__0 + (int)object.arrays[0].__1
        + object.arrays[1].__0 + (int)object.arrays[1].__1;
    if (pair_total != 33 || struct_total != 42 || nested_total != 37 || arrays_total != 13) __builtin_trap();
    unsigned high = object.pair.__1 + 2147483606u;
    unsigned maximum = object.pair.__1 - 43u;
    unsigned zero = object.pair.__1 - 42u;
    if (high != 2147483648u || maximum != 4294967295u || zero != 0u) __builtin_trap();
    return object.seed + pair_total + struct_total + nested_total + arrays_total;
}
void _start(void) {
    for (unsigned i = 0; i != 4096; ++i) {
        if (source_tuple_control_probe(5) != 130) __builtin_trap();
    }
}
#ifdef __cplusplus
}
#endif
