volatile int phase = 0;
volatile int counter = 7;
__attribute__((noinline)) void point(void) { __asm__(""); }
__attribute__((noinline)) int main(void)
{
    phase = 2;
    ++counter;
    point();
    return 0;
}
void _start(void)
{
    phase = 1;
    (void)main();
    phase = 3;
}
