#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_conbine_f64_from_i32_localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param i32) -> f64 : local.get0 ; f64.convert_i32_s
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i32) -> f64 : local.get0 ; f64.convert_i32_u
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);

        using Runner = interpreter_runner<Opt>;

        auto run_s = [&](::std::int32_t v) noexcept -> double
        {
            return load_f64(Runner::run(cm.local_funcs.index_unchecked(0),
                                        rt.local_defined_function_vec_storage.index_unchecked(0),
                                        pack_i32(v),
                                        nullptr,
                                        nullptr)
                                .results);
        };

        auto run_u = [&](::std::int32_t v) noexcept -> double
        {
            return load_f64(Runner::run(cm.local_funcs.index_unchecked(1),
                                        rt.local_defined_function_vec_storage.index_unchecked(1),
                                        pack_i32(v),
                                        nullptr,
                                        nullptr)
                                .results);
        };

        // signed
        for(auto const v : ::std::array<::std::int32_t, 7>{
                0,
                1,
                -1,
                ::std::numeric_limits<::std::int32_t>::min(),
                ::std::numeric_limits<::std::int32_t>::max(),
                123456789,
                -987654321,
            })
        {
            UWVM2TEST_REQUIRE(run_s(v) == static_cast<double>(v));
        }

        // unsigned (interpret bits as u32)
        for(auto const v : ::std::array<::std::int32_t, 6>{
                0,
                1,
                -1,
                static_cast<::std::int32_t>(0x80000000u),
                static_cast<::std::int32_t>(0x7fffffffu),
                static_cast<::std::int32_t>(0x40000000u),
            })
        {
            ::std::uint32_t const u = static_cast<::std::uint32_t>(v);
            UWVM2TEST_REQUIRE(run_u(v) == static_cast<double>(u));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_f64_from_i32_localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_f64_from_i32_localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_f64_from_i32_localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall: strict bytecode assertions for `local.get(i32) + f64.convert_i32_{s,u}` fusion.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            auto const exp_s = optable::translate::get_uwvmint_f64_from_i32_s_fptr_from_tuple<opt>(curr, tuple);
            auto const exp_u = optable::translate::get_uwvmint_f64_from_i32_u_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_u));
#endif

            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        // Byref: semantics smoke (byref intentionally uses a minimal conbine state machine; fusion may be flushed).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_f64_from_i32_localget();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

