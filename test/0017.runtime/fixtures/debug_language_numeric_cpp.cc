/* Original compiler output supplies values and debug locations. */
static volatile double observed;
struct NumericPacket { int value; char payload[24]; };
struct NumericBits { unsigned flags : 3; };
__attribute__((noinline, export_name("numeric_probe"))) void numeric_probe(void) {
    volatile float decimal32 = 1.25f;
    volatile double decimal64 = -2.5;
    volatile struct NumericPacket packet = {7, {0}};
    volatile struct NumericPacket* null_packet = (volatile struct NumericPacket*)0;
    volatile struct NumericBits bits = {5};
    observed = decimal32 + decimal64; /* NUMERIC_READY */
    if(decimal32 != 1.25f || decimal64 != -2.5 || packet.value != 7 || null_packet != 0 || bits.flags != 5) __builtin_trap();
}
__attribute__((export_name("_start"))) void _start(void) { numeric_probe(); }
