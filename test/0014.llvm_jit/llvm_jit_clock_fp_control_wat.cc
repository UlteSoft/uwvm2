#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include "../0008.imported/wasi/wasip1/func/fp_control_probe.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace fp_control = ::uwvm2test::wasip1_fp_control;

    [[nodiscard]] strict::byte_vec build_clock_fp_module()
    {
        strict::module_builder module{};
        auto op = [&](strict::byte_vec& code, strict::wasm_op opcode) { strict::append_u8(code, strict::u8(opcode)); };
        auto u32 = [&](strict::byte_vec& code, ::std::uint32_t value) { strict::append_u32_leb(code, value); };

        // WASI Preview 1 clock_time_get has (clockid:i32, precision:i64, time_ptr:i32) -> errno:i32.
        // UWVM's memory64 extension changes only the guest pointer operand to i64. Both functions are explicitly
        // audited as preserving Wasm-relevant FP control and therefore use the generated import fast path.
        module.types.push_back(strict::func_type{{strict::k_val_i32, strict::k_val_i64, strict::k_val_i32}, {strict::k_val_i32}});
        module.add_import_func("wasi_snapshot_preview1", "clock_time_get", 0u);
#if defined(UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64)
        module.types.push_back(strict::func_type{{strict::k_val_i32, strict::k_val_i64, strict::k_val_i64}, {strict::k_val_i32}});
        module.add_import_func("wasi_snapshot_preview1", "clock_time_get_wasm64", 1u);
#endif

        module.has_memory = true;
        module.memory_min = 1u;

        auto add_binary_after_clock = [&](bool wasm64_pointer, strict::wasm_op binary_opcode)
        {
            strict::func_type type{{strict::k_val_f32, strict::k_val_f32}, {strict::k_val_f32}};
            strict::func_body body{};
            auto& code{body.code};

            op(code, strict::wasm_op::i32_const);
            strict::append_i32_leb(code, 1);  // __WASI_CLOCKID_MONOTONIC
            op(code, strict::wasm_op::i64_const);
            strict::append_i64_leb(code, 0);
            if(wasm64_pointer)
            {
                op(code, strict::wasm_op::i64_const);
                strict::append_i64_leb(code, 4096);
            }
            else
            {
                op(code, strict::wasm_op::i32_const);
                strict::append_i32_leb(code, 4096);
            }
            op(code, strict::wasm_op::call);
            u32(code, wasm64_pointer ? 1u : 0u);
            op(code, strict::wasm_op::drop);

            // This operation must execute immediately after the preserving callback in the same generated frame.
            // It detects leaked rounding and FTZ/DAZ controls rather than merely testing the host function directly.
            op(code, strict::wasm_op::local_get);
            u32(code, 0u);
            op(code, strict::wasm_op::local_get);
            u32(code, 1u);
            op(code, binary_opcode);
            op(code, strict::wasm_op::end);
            static_cast<void>(module.add_func(::std::move(type), ::std::move(body)));
        };

        add_binary_after_clock(false, strict::wasm_op::f32_add);
        add_binary_after_clock(false, strict::wasm_op::f32_mul);
#if defined(UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64)
        add_binary_after_clock(true, strict::wasm_op::f32_add);
        add_binary_after_clock(true, strict::wasm_op::f32_mul);
#endif
        return module.build();
    }

    template <typename Value>
    void append_abi_value(strict::byte_vec& bytes, Value value)
    {
        auto const old_size{bytes.size()};
        bytes.resize(old_size + sizeof(Value));
        ::std::memcpy(bytes.data() + old_size, ::std::addressof(value), sizeof(Value));
    }

    [[nodiscard]] ::std::uint32_t run_binary(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module,
                                             ::std::uint_least32_t function_index,
                                             float left,
                                             float right) noexcept
    {
        strict::byte_vec parameters{};
        parameters.reserve(sizeof(float) * 2u);
        append_abi_value(parameters, left);
        append_abi_value(parameters, right);

        float result{};
        ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(
            module, function_index, ::std::addressof(result), sizeof(result), parameters.data(), parameters.size());
        return ::std::bit_cast<::std::uint32_t>(result);
    }

    [[nodiscard]] int check_pointer_variant(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module,
                                            ::std::uint_least32_t add_index,
                                            ::std::uint_least32_t multiply_index,
                                            fp_control::snapshot const& hostile_control) noexcept
    {
        // 0.75 ulp rounds upward under nearest-even, but FE_DOWNWARD leaves 1.0 unchanged. This distinguishes the
        // canonical Wasm mode from the hostile embedding mode; an exact half-ulp would produce 1.0 in both modes.
        auto const three_quarter_ulp{::std::bit_cast<float>(::std::uint32_t{0x33c00000u})};
        if(run_binary(module, add_index, 1.0f, three_quarter_ulp) != 0x3f800001u) { return 1; }
        if(!fp_control::unchanged(hostile_control)) { return 2; }

        auto const minimum_subnormal{::std::bit_cast<float>(::std::uint32_t{1u})};
        if(run_binary(module, add_index, minimum_subnormal, 0.0f) != 1u) { return 3; }
        if(!fp_control::unchanged(hostile_control)) { return 4; }

        auto const minimum_normal{::std::numeric_limits<float>::min()};
        if(run_binary(module, multiply_index, minimum_normal, 0.5f) != 0x00400000u) { return 5; }
        if(!fp_control::unchanged(hostile_control)) { return 6; }
        return 0;
    }
}

int main()
{
    fp_control::initial_state_restore restore_initial{};
    fp_control::snapshot hostile_control{};
    if(!restore_initial.valid || !fp_control::prepare_hostile(hostile_control)) { return 1; }

    auto wasm{build_clock_fp_module()};
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_jit_clock_fp_control")};
    if(prepared.mod == nullptr) { return 2; }

    // Imported functions precede local functions in the shared function index space.
#if defined(UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64)
    constexpr ::std::uint_least32_t first_local_function_index{2u};
#else
    constexpr ::std::uint_least32_t first_local_function_index{1u};
#endif
    if(auto const wasm32_result{
           check_pointer_variant(prepared.mod, first_local_function_index, first_local_function_index + 1u, hostile_control)};
       wasm32_result != 0)
    {
        return 10 + wasm32_result;
    }
#if defined(UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64)
    if(auto const wasm64_result{
           check_pointer_variant(prepared.mod, first_local_function_index + 2u, first_local_function_index + 3u, hostile_control)};
       wasm64_result != 0)
    {
        return 20 + wasm64_result;
    }
#endif
    return 0;
}
