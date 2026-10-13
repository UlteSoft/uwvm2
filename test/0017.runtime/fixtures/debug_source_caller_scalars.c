#if defined(__cplusplus)
#define SOURCE_BOOL bool
extern "C" {
#else
#define SOURCE_BOOL _Bool
#endif
volatile unsigned int SOURCE_CALLER_OBSERVED;
__attribute__((noinline))
void source_primitive_probe(unsigned int argument)
{
    volatile int retained = -99;
    volatile SOURCE_BOOL enabled = (SOURCE_BOOL)0;
    volatile float fraction = 9.5f;
    volatile double wider = 7.25;
    SOURCE_CALLER_OBSERVED = 1u;
    if(argument != 73u || retained != -99 || enabled != (SOURCE_BOOL)0 || /* SOURCE_CALLER_DAP_STOP */
       fraction != 9.5f || wider != 7.25) { __builtin_trap(); }
    SOURCE_CALLER_OBSERVED = 2u;
}
__attribute__((noinline))
void source_caller(unsigned int seed)
{
    volatile int retained = (int)seed;
    volatile SOURCE_BOOL enabled = (SOURCE_BOOL)1;
    volatile float fraction = 1.25f;
    volatile double wider = -2.5;
    source_primitive_probe((unsigned int)retained);
    if(retained != 73 || enabled != (SOURCE_BOOL)1 || fraction != 1.25f ||
       wider != -2.5 || SOURCE_CALLER_OBSERVED != 2u) { __builtin_trap(); }
}
void _start(void) { source_caller(73u); }
#if defined(__cplusplus)
}
#endif
