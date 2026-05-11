#include "../uwvm_int_translate_strict_common.h"

#include <cstdint>

#if defined(__unix__) || defined(__APPLE__)
# include <sys/wait.h>
# include <unistd.h>
#endif

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

#if defined(__unix__) || defined(__APPLE__)

    [[noreturn]] void exit_10() noexcept { _exit(10); }
    [[noreturn]] void exit_11() noexcept { _exit(11); }
    [[noreturn]] void exit_12() noexcept { _exit(12); }
    [[noreturn]] void exit_13() noexcept { _exit(13); }
    [[noreturn]] void exit_90() noexcept { _exit(90); }   // unexpected trap kind
    [[noreturn]] void exit_97() noexcept { _exit(97); }   // validation/compile failure
    [[noreturn]] void exit_98() noexcept { _exit(98); }   // no trap (unexpected return)

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[noreturn]] void compile_and_run(byte_vec const& wasm_bytes)
    {
        auto const pr = prepare_runtime_from_wasm(wasm_bytes, u8"uwvm2test.trap");
        auto const& rt = *pr.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        if(err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { exit_97(); }

        using Runner = interpreter_runner<Opt>;
        (void)Runner::run(cm.local_funcs.index_unchecked(0),
                          rt.local_defined_function_vec_storage.index_unchecked(0),
                          pack_no_params(),
                          nullptr,
                          nullptr);

        // Should not reach: all these test bodies must trap.
        exit_98();
    }

    [[nodiscard]] byte_vec build_div0_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i32_const); i32(c, 1);
        op(c, wasm_op::i32_const); i32(c, 0);
        op(c, wasm_op::i32_div_s);
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_overflow_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i32_const); i32(c, ::std::numeric_limits<::std::int32_t>::min());
        op(c, wasm_op::i32_const); i32(c, -1);
        op(c, wasm_op::i32_div_s);
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_conv_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::f32_const);
        append_f32_ieee(c, ::std::numeric_limits<float>::quiet_NaN());
        op(c, wasm_op::i32_trunc_f32_s);
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_unreachable_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::unreachable);
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

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
        if(ec != expected_code) { return fail(__LINE__, "unexpected child exit code"); }
        return 0;
    }

    void set_all_traps_to(optable::unreachable_func_t fn) noexcept
    {
        optable::unreachable_func = fn;
        optable::trap_invalid_conversion_to_integer_func = fn;
        optable::trap_integer_divide_by_zero_func = fn;
        optable::trap_integer_overflow_func = fn;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[noreturn]] void child_div0()
    {
        set_all_traps_to(exit_90);
        optable::trap_integer_divide_by_zero_func = exit_10;
        auto const wasm = build_div0_module();
        compile_and_run<Opt>(wasm);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[noreturn]] void child_overflow()
    {
        set_all_traps_to(exit_90);
        optable::trap_integer_overflow_func = exit_11;
        auto const wasm = build_overflow_module();
        compile_and_run<Opt>(wasm);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[noreturn]] void child_invalid_conv()
    {
        set_all_traps_to(exit_90);
        optable::trap_invalid_conversion_to_integer_func = exit_12;
        auto const wasm = build_invalid_conv_module();
        compile_and_run<Opt>(wasm);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[noreturn]] void child_unreachable()
    {
        set_all_traps_to(exit_90);
        optable::unreachable_func = exit_13;
        auto const wasm = build_unreachable_module();
        compile_and_run<Opt>(wasm);
    }

#endif
}  // namespace

int main()
{
#if !defined(__unix__) && !defined(__APPLE__)
    return 0;  // skip on non-POSIX platforms
#else
    auto run_case = [&](auto&& child_fn, int expected_exit, char const* expected_message) -> int
    {
        (void)expected_exit;
        (void)expected_message;
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        return run_in_child_expect_trap_message(expected_message, ::std::forward<decltype(child_fn)>(child_fn));
#else
        return run_in_child_expect_exit(expected_exit, ::std::forward<decltype(child_fn)>(child_fn));
#endif
    };

    int ec{};

    if(abi_mode_enabled("byref"))
    {
        ec = run_case([] { child_div0<k_test_byref_opt>(); }, 10, "integer divide by zero");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_overflow<k_test_byref_opt>(); }, 11, "integer overflow");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_invalid_conv<k_test_byref_opt>(); }, 12, "invalid conversion to integer");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_unreachable<k_test_byref_opt>(); }, 13, "catch unreachable");
        if(ec != 0) { return ec; }
    }

    if(abi_mode_enabled("tail-min"))
    {
        ec = run_case([] { child_div0<k_test_tail_min_opt>(); }, 10, "integer divide by zero");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_overflow<k_test_tail_min_opt>(); }, 11, "integer overflow");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_invalid_conv<k_test_tail_min_opt>(); }, 12, "invalid conversion to integer");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_unreachable<k_test_tail_min_opt>(); }, 13, "catch unreachable");
        if(ec != 0) { return ec; }
    }

    if(abi_mode_enabled("tail-sysv"))
    {
        ec = run_case([] { child_div0<k_test_tail_sysv_opt>(); }, 10, "integer divide by zero");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_overflow<k_test_tail_sysv_opt>(); }, 11, "integer overflow");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_invalid_conv<k_test_tail_sysv_opt>(); }, 12, "invalid conversion to integer");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_unreachable<k_test_tail_sysv_opt>(); }, 13, "catch unreachable");
        if(ec != 0) { return ec; }
    }

    if(abi_mode_enabled("tail-aapcs64"))
    {
        ec = run_case([] { child_div0<k_test_tail_aapcs64_opt>(); }, 10, "integer divide by zero");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_overflow<k_test_tail_aapcs64_opt>(); }, 11, "integer overflow");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_invalid_conv<k_test_tail_aapcs64_opt>(); }, 12, "invalid conversion to integer");
        if(ec != 0) { return ec; }
        ec = run_case([] { child_unreachable<k_test_tail_aapcs64_opt>(); }, 13, "catch unreachable");
        if(ec != 0) { return ec; }
    }

    return 0;
#endif
}
