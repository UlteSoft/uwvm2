#if defined(__cplusplus)
#define SOURCE_BOOL bool
extern "C" {
#else
#define SOURCE_BOOL _Bool
#endif
typedef int SourceTicket;
volatile unsigned int SOURCE_QUALIFIED_OBSERVED;
__attribute__((noinline))
void source_primitive_probe(int const retained, SOURCE_BOOL const enabled,
    float const fraction, double const wider, SourceTicket const tagged,
    int const volatile barrier)
{
    SOURCE_QUALIFIED_OBSERVED = 1u; /* SOURCE_QUALIFIED_DAP_STOP */
    if(retained != 73 || enabled != (SOURCE_BOOL)1 || fraction != 1.25f ||
       wider != -2.5 || tagged != 123 || barrier != 17) { __builtin_trap(); }
}
void _start(void)
{
    source_primitive_probe(73, (SOURCE_BOOL)1, 1.25f, -2.5, 123, 17);
    if(SOURCE_QUALIFIED_OBSERVED != 1u) { __builtin_trap(); }
}
#if defined(__cplusplus)
}
#endif
