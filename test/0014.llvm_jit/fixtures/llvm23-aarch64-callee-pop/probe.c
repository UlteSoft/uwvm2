/**
 * @file probe.c
 * @brief Execute the generated AArch64 caller-frame regression functions.
 * @details The two callee-pop calls and the ordinary ABI control must preserve
 * their independent spill-slot values. This checks observable execution rather
 * than treating an assembly-pattern match as a successful runtime result.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

extern uint64_t direct_reload(void);
extern uint64_t indirect_reload(void (*target)(void));
extern uint64_t ordinary_reload(void);
extern void callee(void);

int main(void)
{
    uint64_t const direct = direct_reload();
    uint64_t const indirect = indirect_reload(callee);
    uint64_t const ordinary = ordinary_reload();
    printf("direct=%" PRIu64 " indirect=%" PRIu64 " ordinary=%" PRIu64 "\n",
           direct, indirect, ordinary);
    return direct != UINT64_C(42) || indirect != UINT64_C(43) || ordinary != UINT64_C(44);
}
