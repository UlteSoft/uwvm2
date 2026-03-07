#include "../uwvm_int_translate_strict_common.h"

#include <cstdint>
#include <limits>

#if defined(__unix__) || defined(__APPLE__)
# include <sys/wait.h>
# include <unistd.h>
#endif

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

#if defined(__unix__) || defined(__APPLE__)

    [[noreturn]] void exit_11() noexcept { _exit(11); }  // integer overflow
    [[noreturn]] void exit_12() noexcept { _exit(12); }  // invalid conversion to integer
    [[noreturn]] void exit_90() noexcept { _exit(90); }  // unexpected trap kind
    [[noreturn]] void exit_98() noexcept { _exit(98); }  // unexpected return (no trap)

    template <typename Fn>
    [[nodiscard]] int run_in_child_expect_exit(int expected_code, Fn&& fn)
    {
        pid_t const pid = ::fork();
        if(pid == 0)
        {
            fn();
            exit_98();
        }
        if(pid < 0) { return fail(__LINE__, "fork"); }

        int status{};
        if(::waitpid(pid, &status, 0) < 0) { return fail(__LINE__, "waitpid"); }
        if(!WIFEXITED(status)) { return fail(__LINE__, "child did not exit normally"); }
        int const ec = WEXITSTATUS(status);
        if(ec != expected_code)
        {
            ::std::fprintf(stderr, "uwvm2test: expected exit=%d, got=%d\n", expected_code, ec);
            return fail(__LINE__, "unexpected child exit code");
        }
        return 0;
    }

#endif

    [[nodiscard]] byte_vec build_conbine_trunc_trap_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (f32)->i32: local.get 0; i32.trunc_f32_s
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_trunc_f32_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (f32)->i32: local.get 0; i32.trunc_f32_u
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_trunc_f32_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (f64)->i32: local.get 0; i32.trunc_f64_s
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_trunc_f64_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (f64)->i32: local.get 0; i32.trunc_f64_u
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_trunc_f64_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

#if defined(__unix__) || defined(__APPLE__)

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_trunc_trap_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
            auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
            auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
            auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

            constexpr auto exp0 = optable::translate::get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp1 = optable::translate::get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp2 = optable::translate::get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp3 = optable::translate::get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp0));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp1));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp2));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp3));
        }
#endif

        // normal cases (no trap)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f32(3.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32(5.75f),
                                              nullptr,
                                              nullptr)
                                       .results) == 5);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f64(-7.9),
                                              nullptr,
                                              nullptr)
                                       .results) == -7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64(42.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 42);

        // trap matrix:
        // - NaN => invalid conversion (exit 12)
        // - out-of-range => integer overflow (exit 11)
        float const nanf = ::std::numeric_limits<float>::quiet_NaN();
        double const nand = ::std::numeric_limits<double>::quiet_NaN();

        auto run_trap = [&](::std::size_t fidx, byte_vec const& params, int expected_exit) noexcept -> int
        {
            return run_in_child_expect_exit(expected_exit,
                                            [&]
                                            {
                                                // Sanity against unexpected overrides.
                                                if(optable::trap_invalid_conversion_to_integer_func != exit_12) { _exit(92); }
                                                if(optable::trap_integer_overflow_func != exit_11) { _exit(91); }

                                                (void)Runner::run(cm.local_funcs.index_unchecked(fidx),
                                                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                                                  params,
                                                                  nullptr,
                                                                  nullptr);
                                                exit_98();
                                            });
        };

        // NaN cases
        UWVM2TEST_REQUIRE(run_trap(0uz, pack_f32(nanf), 12) == 0);
        UWVM2TEST_REQUIRE(run_trap(1uz, pack_f32(nanf), 12) == 0);
        UWVM2TEST_REQUIRE(run_trap(2uz, pack_f64(nand), 12) == 0);
        UWVM2TEST_REQUIRE(run_trap(3uz, pack_f64(nand), 12) == 0);

        // overflow cases
        UWVM2TEST_REQUIRE(run_trap(0uz, pack_f32(1.0e20f), 11) == 0);
        UWVM2TEST_REQUIRE(run_trap(1uz, pack_f32(-1.0f), 11) == 0);
        UWVM2TEST_REQUIRE(run_trap(2uz, pack_f64(1.0e40), 11) == 0);
        UWVM2TEST_REQUIRE(run_trap(3uz, pack_f64(-1.0), 11) == 0);

        return 0;
    }

#endif

    [[nodiscard]] int test_translate_conbine_trunc_trap() noexcept
    {
#if !defined(__unix__) && !defined(__APPLE__)
        return 0;  // skip on non-POSIX platforms
#else
        optable::unreachable_func = exit_90;
        optable::trap_integer_divide_by_zero_func = exit_90;
        optable::trap_invalid_conversion_to_integer_func = exit_12;
        optable::trap_integer_overflow_func = exit_11;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_trunc_trap_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_trunc_trap");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_conbine_trunc_trap_suite<opt>(rt) == 0);
        }

        // tailcall (the heavy combine fused opfuncs are tailcall-specialized)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_conbine_trunc_trap_suite<opt>(rt) == 0);
        }

        return 0;
#endif
    }
}  // namespace

int main()
{
    return test_translate_conbine_trunc_trap();
}
