// Production raw-byte entry tests, including native imported-global boundaries.
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include "../0014.llvm_jit/fixtures/fp_rounding_oracle.h"
namespace strict = uwvm2test::uwvm_int_strict;
namespace runtime = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace type = uwvm2::uwvm::wasm::type;
namespace value = uwvm2::parser::wasm::standard::wasm1::type;

template <class Float>
struct reference_global
{
    inline static constexpr uwvm2::utils::container::u8string_view global_name{sizeof(Float) == 4 ? u8"f32" : u8"f64"};
    inline static constexpr bool is_mutable{true};
    using value_type = Float;
    Float value{};

    friend Float const& global_get(reference_global& g) noexcept { return g.value; }

    friend void global_set(reference_global& g, Float const& v) noexcept { std::memcpy(&g.value, &v, sizeof(v)); }
};

struct bit_provider
{
    uwvm2::utils::container::u8string_view module_name{u8"bits-host"};
    using local_global_tuple = uwvm2::utils::container::tuple<reference_global<value::wasm_f32>, reference_global<value::wasm_f64>>;
    local_global_tuple local_global{};
};

static_assert(type::has_local_global_tuple<bit_provider>);
constexpr unsigned operations = 18;

strict::byte_vec build_bits_module()
{
    strict::module_builder m{};
    m.has_memory = true;
    m.memory_min = 1;
    m.has_table = true;
    m.table_min = 2;
    m.add_import_global("bits-host", "f32", strict::k_val_f32, true);
    m.add_import_global("bits-host", "f64", strict::k_val_f64, true);
    strict::element_segment elem{};
    elem.offset_expr = {std::byte{0x41}, std::byte{}, std::byte{0x0b}};
    elem.func_indices = {0u, operations};
    m.elements.push_back(elem);
    for(unsigned wide = 0; wide != 2; ++wide)
    {
        auto vt = wide ? strict::k_val_f64 : strict::k_val_f32;
        strict::global_entry g{vt, true, {}};
        strict::append_u8(g.init_expr, wide ? 0x44 : 0x43);
        for(unsigned b = 0; b != (wide ? 8u : 4u); ++b) { strict::append_u8(g.init_expr, 0); }
        strict::append_u8(g.init_expr, 0x0b);
        m.globals.push_back(g);
        for(unsigned op = 0; op != operations; ++op)
        {
            strict::func_body body{};
            auto emit = [&](strict::wasm_op code) { strict::append_u8(body.code, strict::u8(code)); };
            auto imm = [&](unsigned n) { strict::append_u32_leb(body.code, n); };
            auto local = [&](unsigned n)
            {
                emit(strict::wasm_op::local_get);
                imm(n);
            };
            auto zero = [&]
            {
                emit(strict::wasm_op::i32_const);
                imm(0);
            };
            if(op == 8) { zero(); }
            if(op == 12)
            {
                emit(wide ? strict::wasm_op::f64_const : strict::wasm_op::f32_const);
                std::uint64_t bits = wide ? 0x7ff0000000000001ull : 0x7f800001u;
                for(unsigned b = 0; b != (wide ? 8u : 4u); ++b) { strict::append_u8(body.code, static_cast<std::uint8_t>(bits >> (8 * b))); }
            }
            else if(op != 13) { local(0); }
            switch(op)
            {
                case 1: emit(wide ? strict::wasm_op::f64_abs : strict::wasm_op::f32_abs); break;
                case 2: emit(wide ? strict::wasm_op::f64_neg : strict::wasm_op::f32_neg); break;
                case 3:
                    local(1);
                    emit(wide ? strict::wasm_op::f64_copysign : strict::wasm_op::f32_copysign);
                    break;
                case 4:
                    local(1);
                    local(2);
                    emit(strict::wasm_op::select);
                    break;
                case 5:
                    emit(strict::wasm_op::local_set);
                    imm(0);
                    local(0);
                    emit(strict::wasm_op::local_tee);
                    imm(0);
                    break;
                case 6:
                    local(1);
                    local(2);
                    emit(strict::wasm_op::call);
                    imm(wide * operations);
                    break;
                case 7:
                    local(1);
                    local(2);
                    emit(strict::wasm_op::i32_const);
                    imm(wide);
                    emit(strict::wasm_op::call_indirect);
                    imm(wide * operations);
                    imm(0);
                    break;
                case 8:
                    emit(wide ? strict::wasm_op::f64_store : strict::wasm_op::f32_store);
                    imm(wide ? 3 : 2);
                    imm(0);
                    zero();
                    emit(wide ? strict::wasm_op::f64_load : strict::wasm_op::f32_load);
                    imm(wide ? 3 : 2);
                    imm(0);
                    break;
                case 9:
                case 10:
                    emit(strict::wasm_op::global_set);
                    imm(wide + (op == 9 ? 2 : 0));
                    emit(strict::wasm_op::global_get);
                    imm(wide + (op == 9 ? 2 : 0));
                    break;
                case 11:
                    emit(wide ? strict::wasm_op::i64_reinterpret_f64 : strict::wasm_op::i32_reinterpret_f32);
                    emit(wide ? strict::wasm_op::f64_reinterpret_i64 : strict::wasm_op::f32_reinterpret_i32);
                    break;
                case 13:
                    emit(strict::wasm_op::global_get);
                    imm(4 + wide);
                    break;
                case 14: emit(wide ? strict::wasm_op::f64_ceil : strict::wasm_op::f32_ceil); break;
                case 15: emit(wide ? strict::wasm_op::f64_floor : strict::wasm_op::f32_floor); break;
                case 16: emit(wide ? strict::wasm_op::f64_trunc : strict::wasm_op::f32_trunc); break;
                case 17: emit(wide ? strict::wasm_op::f64_nearest : strict::wasm_op::f32_nearest); break;
            }
            emit(strict::wasm_op::end);
            m.add_func(
                {
                    {vt, vt, strict::k_val_i32},
                    {vt}
            },
                std::move(body));
        }
    }
    for(unsigned wide = 0; wide != 2; ++wide)
    {
        strict::global_entry g{wide ? strict::k_val_f64 : strict::k_val_f32, false, {}};
        strict::append_u8(g.init_expr, wide ? 0x44 : 0x43);
        std::uint64_t bits = wide ? 0x7ff0000000000001ull : 0x7f800001u;
        for(unsigned b = 0; b != (wide ? 8u : 4u); ++b) { strict::append_u8(g.init_expr, static_cast<std::uint8_t>(bits >> (8 * b))); }
        strict::append_u8(g.init_expr, 0x0b);
        m.globals.push_back(g);
    }
    return m.build();
}

template <class UInt>
unsigned check_bits(UInt bits, bool lazy)
{
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    for(unsigned op = 0; op != operations; ++op)
    {
        for(value::wasm_i32 condition: {0, 1})
        {
            std::byte parameters[2 * sizeof(UInt) + 4]{};
            UInt left = bits, right = sign;
            if(op == 4)
            {
                left = condition ? bits : (bits ^ UInt{1});
                right = condition ? (bits ^ UInt{1}) : bits;
            }
            std::memcpy(parameters, &left, sizeof(left));
            std::memcpy(parameters + sizeof(UInt), &right, sizeof(right));
            std::memcpy(parameters + 2 * sizeof(UInt), &condition, 4);
            UInt result{};
            runtime::entry_function_abi_buffers buffers{parameters, sizeof(parameters), reinterpret_cast<std::byte*>(&result), sizeof(result)};
            unsigned index = (sizeof(UInt) == 8 ? operations : 0) + op;
#if !defined(UWVM2_FP_BITS_FULL_ONLY)
            if(lazy) { runtime::lazy_compile_and_run_main_module(u8"bits-main", {index, buffers, false}); }
            else
#endif
                runtime::full_compile_and_run_main_module(u8"bits-main", {index, buffers});
            UInt expected = op == 1 ? bits & ~sign : op == 2 ? bits ^ sign : op == 3 ? (bits & ~sign) | sign : bits;
            if(op == 12 || op == 13)
            {
                if constexpr(sizeof(UInt) == 8) { expected = 0x7ff0000000000001ull; }
                else
                {
                    expected = 0x7f800001u;
                }
            }
            using Float = std::conditional_t<sizeof(UInt) == 4, value::wasm_f32, value::wasm_f64>;
            if(op >= 14) { expected = fp_rounding_oracle::expected<Float>(bits, op - 14); }
            bool const okay{op >= 14 ? fp_rounding_oracle::matches<Float>(bits, result, op - 14) : result == expected};
            if(!okay)
            {
                ++errors;
                std::fprintf(stderr,
                             "entry lazy=%d f%zu op=%u expected=%llx actual=%llx\n",
                             lazy,
                             sizeof(UInt) * 8,
                             op,
                             static_cast<unsigned long long>(expected),
                             static_cast<unsigned long long>(result));
            }
        }
    }
    return errors;
}

unsigned run_bits(mode::runtime_compiler_t backend, bool lazy)
{
    runtime::reset_runtime_state_host_api();
    auto wasm = build_bits_module();
    type::local_imported_t provider{bit_provider{}};
    auto prepared = strict::prepare_runtime_from_wasm(wasm, u8"bits-main", {}, {}, {provider});

    struct reset
    {
        ~reset() { runtime::reset_runtime_state_host_api(); }
    } reset_guard;

    if(prepared.mod == nullptr) { return 1; }
    mode::global_runtime_compiler = backend;
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
#if !defined(UWVM2_FP_BITS_FULL_ONLY)
    if(lazy) { mode::global_runtime_mode = mode::runtime_mode_t::lazy_compile; }
#endif
    mode::global_runtime_compile_threads_resolved = 16uz;
#if defined(UWVM_RUNTIME_LLVM_JIT)
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
#endif
    unsigned errors{};
    for(std::uint32_t bits: {0u, 0x80000000u, 1u, 0x7f800001u, 0xff800123u, 0x7fc00123u, 0x3f000000u, 0xbfc00000u}) { errors += check_bits(bits, lazy); }
    for(std::uint64_t bits: {0ull, 0x8000000000000000ull, 1ull, 0x7ff0000000000001ull, 0xfff0000000000123ull, 0x3fe0000000000000ull, 0xbff8000000000000ull}) { errors += check_bits(bits, lazy); }
    std::printf("Production FP bits backend=%u lazy=%d: %u failures\n", static_cast<unsigned>(backend), lazy, errors);
    return errors;
}

int main()
{
    unsigned errors{};
    for(bool lazy: {false, true})
    {
#if defined(UWVM2_FP_BITS_FULL_ONLY)
        if(lazy) { continue; }
#endif
        errors += run_bits(mode::runtime_compiler_t::uwvm_interpreter_only, lazy);
#if defined(UWVM_RUNTIME_LLVM_JIT)
        errors += run_bits(mode::runtime_compiler_t::llvm_jit_only, lazy);
#endif
    }
    return errors != 0;
}
