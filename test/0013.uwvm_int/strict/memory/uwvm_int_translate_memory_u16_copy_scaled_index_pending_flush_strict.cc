#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32_i32(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_u16_copy_scaled_index_pending_flush_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: trigger `u16_copy_scaled_index_*` pending and force flush via `i32.add`.
        // (dst: i32, idx: i32) -> i32 : dst + load16_u(idx<<sh, offset=src_off)
        //
        // Pending window (combine=extra + tailcall only):
        //   local.get dst; local.get idx; i32.const sh; i32.shl; i32.load16_u offset=src_off
        // Then `i32.add` forces `flush_conbine_pending()` for `u16_copy_scaled_index_after_load`.
        {
            constexpr ::std::int32_t k_sh = 1;
            constexpr ::std::uint32_t k_src_off = 10u;
            constexpr ::std::uint32_t k_align_16 = 1u;  // align=2 bytes
            constexpr ::std::int32_t k_val_u16 = 0x1234;

            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // Store k_val_u16 at address = (idx<<k_sh) + k_src_off.
            op(c, wasm_op::local_get);
            u32(c, 1u);  // idx
            op(c, wasm_op::i32_const);
            i32(c, k_sh);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(k_src_off));
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_const);
            i32(c, k_val_u16);
            op(c, wasm_op::i32_store16);
            u32(c, k_align_16);
            u32(c, 0u);

            // dst + load16_u(idx<<k_sh, offset=k_src_off)
            op(c, wasm_op::local_get);
            u32(c, 0u);  // dst
            op(c, wasm_op::local_get);
            u32(c, 1u);  // idx
            op(c, wasm_op::i32_const);
            i32(c, k_sh);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u);
            u32(c, k_align_16);
            u32(c, k_src_off);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_u16_copy_scaled_index_pending_flush_suite(runtime_module_t const& rt, ::std::int32_t dst, ::std::int32_t idx) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        ::std::int32_t const got =
            load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                 rt.local_defined_function_vec_storage.index_unchecked(0),
                                 pack_i32_i32(dst, idx),
                                 nullptr,
                                 nullptr)
                         .results);

        constexpr ::std::int32_t k_val_u16 = 0x1234;
        ::std::uint32_t const expected_u = static_cast<::std::uint32_t>(static_cast<::std::uint32_t>(dst) + static_cast<::std::uint32_t>(k_val_u16));
        UWVM2TEST_REQUIRE(got == static_cast<::std::int32_t>(expected_u));

        return 0;
    }

    [[nodiscard]] int test_translate_memory_u16_copy_scaled_index_pending_flush() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_u16_copy_scaled_index_pending_flush_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_u16_copy_scaled_index_pending_flush");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr ::std::int32_t dst = 100;
        constexpr ::std::int32_t idx = 3;

        // Tailcall (no stacktop caching): triggers `u16_copy_scaled_index_*` pending.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_u16_copy_scaled_index_pending_flush_suite<opt>(rt, dst, idx) == 0);
        }

        // Byref mode: semantics smoke (no u16_copy pending kinds, but must still be correct).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_u16_copy_scaled_index_pending_flush_suite<opt>(rt, dst, idx) == 0);
        }

        // Tailcall + stacktop caching: also triggers the pending path (and exercises stacktop_enabled branches nearby).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_u16_copy_scaled_index_pending_flush_suite<opt>(rt, dst, idx) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_u16_copy_scaled_index_pending_flush();
}

