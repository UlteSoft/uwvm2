// Execute real FP opfuncs in a long indirect chain. Checking only a tiny call
// sequence cannot detect loss of musttail: recursion can look correct until the
// native stack overflows. Keep the program on the heap and inputs/results in
// integer bits so neither test-side FP returns nor constant folding hide sNaNs.
#include <uwvm2/runtime/compiler/uwvm_int/optable/numeric.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <cstdio>
#include <vector>

namespace o = uwvm2::runtime::compiler::uwvm_int::optable;
namespace n = o::numeric_details;
static std::uintptr_t end_stack;
static std::byte const* end_ip;
UWVM_INTERPRETER_OPFUNC_HOT_MACRO UWVM_NOINLINE void finish(std::byte const* ip, std::byte*, std::byte*) noexcept
{
    volatile unsigned char marker{};
    end_stack = reinterpret_cast<std::uintptr_t>(&marker);
    end_ip = ip;
}

template <class UInt>
UWVM_NOINLINE bool run(UInt input)
{
    constexpr o::uwvm_interpreter_translate_option_t option{.is_tail_call = true};
    using fn = o::uwvm_interpreter_opfunc_t<std::byte const*, std::byte*, std::byte*>;
    constexpr std::size_t count{262145}; // Odd count: negation must change exactly the sign.
    fn operation;
    if constexpr(sizeof(UInt) == 4) { operation = &o::uwvmint_f32_unop<option, n::float_unop::neg, SIZE_MAX, std::byte const*, std::byte*, std::byte*>; }
    else { operation = &o::uwvmint_f64_unop<option, n::float_unop::neg, SIZE_MAX, std::byte const*, std::byte*, std::byte*>; }
    std::vector<std::byte> code((count + 1) * sizeof(fn));
    for(std::size_t i{}; i != count; ++i) { std::memcpy(code.data() + i * sizeof(fn), &operation, sizeof(fn)); }
    fn end = &finish;
    std::memcpy(code.data() + count * sizeof(fn), &end, sizeof(fn));
    std::byte value[sizeof(UInt)], locals[8]{};
    std::memcpy(value, &input, sizeof(input));
    volatile unsigned char marker{};
    auto start_stack = reinterpret_cast<std::uintptr_t>(&marker);
    operation(code.data(), value + sizeof(UInt), locals);
    UInt result{};
    std::memcpy(&result, value, sizeof(result));
    auto distance = end_stack > start_stack ? end_stack - start_stack : start_stack - end_stack;
    // Generous allowance for different ABIs/debug frames, but independent of the
    // instruction count. No QEMU wall-clock number is treated as a CPU benchmark.
    bool okay = distance < 16384 && end_ip == code.data() + count * sizeof(fn) &&
                result == (input ^ (UInt{1} << (sizeof(UInt) * 8 - 1)));
    std::printf("f%zu tail depth=%zu bits=%s\n", sizeof(UInt) * 8, static_cast<std::size_t>(distance), okay ? "PASS" : "FAIL");
    return okay;
}

int main()
{
    bool okay = run<std::uint32_t>(0x7f800001u);
    okay = run<std::uint64_t>(0x7ff0000000000001ull) && okay;
    return !okay;
}
