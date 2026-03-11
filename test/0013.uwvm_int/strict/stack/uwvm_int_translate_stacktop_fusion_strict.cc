#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_stacktop_fusion_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: const-const-const => triggers spill+const fusion when stacktop caching ring is size 2.
        // (result i32) -> 60
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 10);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_const); i32(c, 30);
            // Combine can otherwise fold the last const into the following i32.add (imm-binop),
            // which prevents the 3rd push and thus never forces a spill.
            op(c, wasm_op::nop);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: const-const-local.get => triggers spill+local.get fusion when stacktop caching ring is size 2.
        // (param i32) (result i32) -> x+3
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::local_get); u32(c, 0u);
            // Prevent local.get being absorbed into the following add (imm/localget binop),
            // which would avoid emitting the spill+local.get fused op.
            op(c, wasm_op::nop);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
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

    [[nodiscard]] int test_translate_stacktop_fusion() noexcept
    {
        // Install optable hooks (unexpected traps/calls should terminate the test process).
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_fusion_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_fusion");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke (stacktop fusion itself requires stacktop caching ABI).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt2{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err2{};
            optable::compile_option cop2{};
            auto cm2 = compiler::compile_all_from_uwvm_single_func<opt2>(rt, cop2, err2);
            UWVM2TEST_REQUIRE(err2.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner2 = interpreter_runner<opt2>;
            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(0),
                                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                                   pack_no_params(),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 60);
            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(1),
                                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                                   pack_i32(9),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 12);
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

        using Runner = interpreter_runner<opt>;

        // Semantics smoke first.
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 60);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_i32(9),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 12);
        }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        // Verify the translator actually emitted the fused spill+push opfuncs by scanning the bytecode.
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr3{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = 5uz,
            .f64_stack_top_curr_pos = 5uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr4{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = 4uz,
            .f32_stack_top_curr_pos = 6uz,
            .f64_stack_top_curr_pos = 6uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };

        constexpr auto spill_const_3 =
            optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_i32, wasm_i32>(curr3, tuple);
        constexpr auto spill_const_4 =
            optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_i32, wasm_i32>(curr4, tuple);

        constexpr auto spill_localget_3 =
            optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_i32, wasm_i32>(curr3, tuple);
        constexpr auto spill_localget_4 =
            optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_i32, wasm_i32>(curr4, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;

        UWVM2TEST_REQUIRE(contains_fptr(bc0, spill_const_3) || contains_fptr(bc0, spill_const_4));
        UWVM2TEST_REQUIRE(contains_fptr(bc1, spill_localget_3) || contains_fptr(bc1, spill_localget_4));
#endif

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_fusion();
}
