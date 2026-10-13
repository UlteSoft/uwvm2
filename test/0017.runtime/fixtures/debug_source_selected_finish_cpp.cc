// Actual -g C++ producer fixture for keeper qualification, not a synthetic
// source map. wasm32/wasm64 -O0/-O1 -gdwarf-4 -nostdlib; no host imports.
namespace finishlab
{
    volatile unsigned seed{73u};
    volatile unsigned observed{};
    __attribute__((always_inline)) inline unsigned inline_child(unsigned value)
    {
        unsigned copied{seed};                         // FINISH_INLINE_CHILD
        asm volatile("" : : "r"(value), "r"(copied) : "memory");
        return value + copied;
    }
    __attribute__((always_inline)) inline unsigned inline_parent(unsigned value)
    {
        unsigned first{inline_child(value)};            // FINISH_INLINE_PARENT
        unsigned second{inline_child(value + 1u)};
        return first + second;
    }
}
extern "C" __attribute__((noinline)) unsigned source_finish_leaf(unsigned argument)
{
    for(unsigned lap{}; lap != 6u; ++lap)
    {
        finishlab::observed = finishlab::observed + finishlab::inline_parent(argument + lap); // FINISH_LEAF_STOP
        asm volatile("" : : "r"(argument), "r"(lap) : "memory");
    }
    return finishlab::observed;
}
extern "C" __attribute__((noinline)) unsigned source_finish_caller(unsigned argument)
{
    unsigned saved{finishlab::seed};
    asm volatile("" : : "r"(argument), "r"(saved) : "memory");
    unsigned returned{source_finish_leaf(argument)};    // FINISH_CALLER_CALL
    finishlab::observed = finishlab::observed + saved;   // FINISH_CALLER_AFTER
    asm volatile("" : : "r"(argument), "r"(saved), "r"(returned) : "memory");
    return returned + saved;
}
extern "C" __attribute__((noinline)) unsigned source_finish_root(unsigned argument)
{
    unsigned returned{source_finish_caller(argument)};  // FINISH_ROOT_CALL
    finishlab::observed = finishlab::observed + argument; // FINISH_ROOT_AFTER
    asm volatile("" : : "r"(argument), "r"(returned) : "memory");
    return returned;
}
extern "C" __attribute__((visibility("default"))) void _start()
{
    unsigned argument{finishlab::seed};
    unsigned returned{source_finish_root(argument)};
    finishlab::observed = finishlab::observed + returned;
    asm volatile("" : : "r"(returned) : "memory");
}
