#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_reinterpret_roundtrip_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i32 -> f32 -> i32 (bit-identity)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_reinterpret_i32);
            op(c, wasm_op::i32_reinterpret_f32);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 -> f64 -> i64 (bit-identity)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_reinterpret_i64);
            op(c, wasm_op::i64_reinterpret_f64);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool contains_any_fptr2(ByteStorage const& bc, Fptr f3, Fptr f4, Fptr f5, Fptr f6) noexcept
    {
        return bytecode_contains_fptr(bc, f3) || bytecode_contains_fptr(bc, f4) || bytecode_contains_fptr(bc, f5) || bytecode_contains_fptr(bc, f6);
    }

    [[nodiscard]] int test_translate_stacktop_reinterpret_scalar4_merged() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_reinterpret_roundtrip_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_reinterpret_scalar4_merged");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            for(::std::uint32_t bits : {0u, 0x3f800000u, 0x7fc00000u, 0xffffffffu})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(static_cast<::std::int32_t>(bits)),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == bits);
            }

            for(::std::uint64_t bits : {0ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xffffffffffffffffull})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                      rt.local_defined_function_vec_storage.index_unchecked(1),
                                      pack_i64(static_cast<::std::int64_t>(bits)),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == bits);
            }
        }

        // Mode B: tailcall + stacktop caching (scalar4 fully-merged). Cover merged-range reinterpret typing branches.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            for(::std::uint32_t bits : {0u, 0x3f800000u, 0x7fc00000u, 0xffffffffu})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(static_cast<::std::int32_t>(bits)),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == bits);
            }

            for(::std::uint64_t bits : {0ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xffffffffffffffffull})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                      rt.local_defined_function_vec_storage.index_unchecked(1),
                                      pack_i64(static_cast<::std::int64_t>(bits)),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == bits);
            }

            // Ensure opfuncs are present (guards against accidental translation skipping).
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c3{.i32_stack_top_curr_pos = 3uz,
                                                                      .i64_stack_top_curr_pos = 3uz,
                                                                      .f32_stack_top_curr_pos = 3uz,
                                                                      .f64_stack_top_curr_pos = 3uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c4{.i32_stack_top_curr_pos = 4uz,
                                                                      .i64_stack_top_curr_pos = 4uz,
                                                                      .f32_stack_top_curr_pos = 4uz,
                                                                      .f64_stack_top_curr_pos = 4uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c5{.i32_stack_top_curr_pos = 5uz,
                                                                      .i64_stack_top_curr_pos = 5uz,
                                                                      .f32_stack_top_curr_pos = 5uz,
                                                                      .f64_stack_top_curr_pos = 5uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c6{.i32_stack_top_curr_pos = 6uz,
                                                                      .i64_stack_top_curr_pos = 6uz,
                                                                      .f32_stack_top_curr_pos = 6uz,
                                                                      .f64_stack_top_curr_pos = 6uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};

            // Accept currpos-shifted variants (currpos depends on stack usage around the opcode).
            UWVM2TEST_REQUIRE(contains_any_fptr2(cm.local_funcs.index_unchecked(0).op.operands,
                                                optable::translate::get_uwvmint_f32_reinterpret_i32_fptr_from_tuple<opt>(c3, tuple),
                                                optable::translate::get_uwvmint_f32_reinterpret_i32_fptr_from_tuple<opt>(c4, tuple),
                                                optable::translate::get_uwvmint_f32_reinterpret_i32_fptr_from_tuple<opt>(c5, tuple),
                                                optable::translate::get_uwvmint_f32_reinterpret_i32_fptr_from_tuple<opt>(c6, tuple)));
            UWVM2TEST_REQUIRE(contains_any_fptr2(cm.local_funcs.index_unchecked(0).op.operands,
                                                optable::translate::get_uwvmint_i32_reinterpret_f32_fptr_from_tuple<opt>(c3, tuple),
                                                optable::translate::get_uwvmint_i32_reinterpret_f32_fptr_from_tuple<opt>(c4, tuple),
                                                optable::translate::get_uwvmint_i32_reinterpret_f32_fptr_from_tuple<opt>(c5, tuple),
                                                optable::translate::get_uwvmint_i32_reinterpret_f32_fptr_from_tuple<opt>(c6, tuple)));
            UWVM2TEST_REQUIRE(contains_any_fptr2(cm.local_funcs.index_unchecked(1).op.operands,
                                                optable::translate::get_uwvmint_f64_reinterpret_i64_fptr_from_tuple<opt>(c3, tuple),
                                                optable::translate::get_uwvmint_f64_reinterpret_i64_fptr_from_tuple<opt>(c4, tuple),
                                                optable::translate::get_uwvmint_f64_reinterpret_i64_fptr_from_tuple<opt>(c5, tuple),
                                                optable::translate::get_uwvmint_f64_reinterpret_i64_fptr_from_tuple<opt>(c6, tuple)));
            UWVM2TEST_REQUIRE(contains_any_fptr2(cm.local_funcs.index_unchecked(1).op.operands,
                                                optable::translate::get_uwvmint_i64_reinterpret_f64_fptr_from_tuple<opt>(c3, tuple),
                                                optable::translate::get_uwvmint_i64_reinterpret_f64_fptr_from_tuple<opt>(c4, tuple),
                                                optable::translate::get_uwvmint_i64_reinterpret_f64_fptr_from_tuple<opt>(c5, tuple),
                                                optable::translate::get_uwvmint_i64_reinterpret_f64_fptr_from_tuple<opt>(c6, tuple)));
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_reinterpret_scalar4_merged();
}
