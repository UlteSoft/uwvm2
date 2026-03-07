#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_extra_heavy_i32_bitwise_2localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        auto add_binop = [&](wasm_op binop) -> void
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, binop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0..f2: local.get2 + i32.{and,or,xor} (extra-heavy fusion candidates).
        add_binop(wasm_op::i32_and);
        add_binop(wasm_op::i32_or);
        add_binop(wasm_op::i32_xor);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_extra_heavy_i32_bitwise_2localget_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 3uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_and = optable::translate::get_uwvmint_i32_and_2localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_or = optable::translate::get_uwvmint_i32_or_2localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_xor = optable::translate::get_uwvmint_i32_xor_2localget_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_and));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_or));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_xor));
        }
#endif

        // semantics
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i32x2(0x0f0f0f0f, 0x33333333),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 0x03030303);
        }
        {
            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_i32x2(0x0f0f0f0f, 0x33333333),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 0x3f3f3f3f);
        }
        {
            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_i32x2(0x0f0f0f0f, 0x33333333),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == 0x3c3c3c3c);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_extra_heavy_i32_bitwise_2localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_extra_heavy_i32_bitwise_2localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_extra_heavy_i32_bitwise_2localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_extra_heavy_i32_bitwise_2localget_suite<opt>(rt) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_extra_heavy_i32_bitwise_2localget_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_extra_heavy_i32_bitwise_2localget();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
