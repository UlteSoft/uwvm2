#if defined(__cplusplus)
#define SOURCE_BOOL bool
extern "C" {
#else
#define SOURCE_BOOL _Bool
#endif
volatile unsigned int SOURCE_TRANSFORM_OBSERVED;
struct source_transformed_record
{
    unsigned char low;
    unsigned char high;
    SOURCE_BOOL enabled;
    signed char narrowed;
};
__attribute__((noinline))
void source_primitive_probe(unsigned int seed, unsigned int truth, int signed_seed)
{
    struct source_transformed_record record = {
        (unsigned char)seed, (unsigned char)(seed >> 8),
        (SOURCE_BOOL)(truth & 1u), (signed char)signed_seed
    };
    SOURCE_TRANSFORM_OBSERVED = 1u;
    if(record.low != 165u /* SOURCE_TRANSFORM_DAP_STOP */ || record.high != 90u ||
       record.enabled != (SOURCE_BOOL)0 || record.narrowed != -128)
    { __builtin_trap(); }
    SOURCE_TRANSFORM_OBSERVED = 2u;
}
void _start(void)
{
    source_primitive_probe(0x12345aa5u, 0x80feu, 0x123480);
    if(SOURCE_TRANSFORM_OBSERVED != 2u) { __builtin_trap(); }
}
#if defined(__cplusplus)
}
#endif
