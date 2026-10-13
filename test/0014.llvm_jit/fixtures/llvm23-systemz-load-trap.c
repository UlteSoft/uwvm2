#include <stdint.h>
#include <stdlib.h>

extern uint64_t load_before_alias_store(uint64_t *);
extern uint64_t adjacent_load_and_trap(uint64_t *);
extern void load_trap_with_intervening_store(void **, void **, void **);

static uint64_t expected;
static uint64_t object;

void observe(void *pointer, uint64_t value, uint32_t *output)
{
    if (pointer != &object || value != expected) abort();
    *output = (uint32_t)value;
}

int main(void)
{
    for (uint64_t value = 1; value != 1000; ++value)
    {
        uint64_t memory = value;
        if (load_before_alias_store(&memory) != value || memory != 0) abort();
        memory = value;
        if (adjacent_load_and_trap(&memory) != value || memory != value) abort();
        uintptr_t words[3] = {0, (uintptr_t)&object, (uintptr_t)value};
        uint32_t output = 0;
        void *code = words, *stack = &output, *unused = 0;
        expected = value;
        load_trap_with_intervening_store(&code, &stack, &unused);
        if (code != words + 3 || stack != (char *)&output + sizeof output || output != value) abort();
    }
    return 0;
}
