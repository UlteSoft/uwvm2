#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_stacktop_spill_then_binop_localget2_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: (param i64 i64) -> none
        // Fill stacktop with 3 i64 pushes (ring size 2 in the tailcall stacktop-ABI option),
        // then do local.get+local.get+i64.add which should (with extra-heavy combine) fuse to
        // a localget2-binop and additionally patch the emitted spill opfunc to spill+binop.
        {
            func_type ty{{k_val_i64, k_val_i64}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i64_const);
            i64(c, 2);
            op(c, wasm_op::i64_const);
            i64(c, 3);

            // Flush pending-const state before the local.get2 sequence.
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_add);

            // Drop: 3 consts + (localget2 add result)
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param f64 f64) -> none
        {
            func_type ty{{k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_const);
            f64(c, 3.0);

            op(c, wasm_op::nop);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_add);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool contains_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
    {
        if(fptr == nullptr) { return false; }
        ::std::array<::std::byte, sizeof(Fptr)> needle{};
        ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(Fptr));
        if(bytes.size() < needle.size()) { return false; }
        auto const* const data = bytes.data();
        for(::std::size_t i{}; i + needle.size() <= bytes.size(); ++i)
        {
            if(::std::memcmp(data + i, needle.data(), needle.size()) == 0) { return true; }
        }
        return false;
    }

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2(double a, double b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] int test_translate_stacktop_spill_then_binop_localget2() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill_then_binop_localget2_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill_then_binop_localget2");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt2{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err2{};
            optable::compile_option cop2{};
            auto cm2 = compiler::compile_all_from_uwvm_single_func<opt2>(rt, cop2, err2);
            UWVM2TEST_REQUIRE(err2.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner2 = interpreter_runner<opt2>;
            (void)Runner2::run(cm2.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_i64x2(10, 20),
                               nullptr,
                               nullptr);
            (void)Runner2::run(cm2.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_f64x2(1.25, 2.5),
                               nullptr,
                               nullptr);
        }

        // Tailcall + stacktop caching (small rings, scalar-only). Same layout as translate_strict Mode C.
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

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr_a{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = 5uz,
            .f64_stack_top_curr_pos = 5uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr_b{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = 4uz,
            .f32_stack_top_curr_pos = 6uz,
            .f64_stack_top_curr_pos = 6uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };

        constexpr auto spill_then_i64_add_a =
            optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_i64>(curr_a, tuple);
        constexpr auto spill_then_i64_add_b =
            optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_i64>(curr_b, tuple);

        constexpr auto spill_then_f64_add_a =
            optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_f64>(curr_a, tuple);
        constexpr auto spill_then_f64_add_b =
            optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_f64>(curr_b, tuple);

        auto const& bc_i64 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc_f64 = cm.local_funcs.index_unchecked(1).op.operands;

        UWVM2TEST_REQUIRE(contains_fptr(bc_i64, spill_then_i64_add_a) || contains_fptr(bc_i64, spill_then_i64_add_b));
        UWVM2TEST_REQUIRE(contains_fptr(bc_f64, spill_then_f64_add_a) || contains_fptr(bc_f64, spill_then_f64_add_b));
#endif

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill_then_binop_localget2();
}

