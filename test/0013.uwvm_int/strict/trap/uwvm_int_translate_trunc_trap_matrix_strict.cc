#include "../uwvm_int_translate_strict_common.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>

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
    [[noreturn]] void exit_98() noexcept { _exit(98); }  // no trap (unexpected return)

    template <typename Fn>
    [[nodiscard]] int run_in_child_expect_exit_oneof(::std::initializer_list<int> expected_codes, Fn&& fn)
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
        for(int const ok : expected_codes)
        {
            if(ec == ok) { return 0; }
        }

        ::std::fprintf(stderr, "uwvm2test: expected exit in {");
        bool first = true;
        for(int const ok : expected_codes)
        {
            ::std::fprintf(stderr, "%s%d", first ? "" : ",", ok);
            first = false;
        }
        ::std::fprintf(stderr, "}, got=%d\n", ec);
        return fail(__LINE__, "unexpected child exit code");
    }

    template <typename Fn>
    [[nodiscard]] int run_in_child_expect_exit(int expected_code, Fn&& fn)
    {
        return run_in_child_expect_exit_oneof({expected_code}, fn);
    }

    [[nodiscard]] byte_vec build_trunc_trap_matrix_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_i32_from_f32 = [&](wasm_op trunc_op, float x) -> void
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, x);
            op(fb.code, trunc_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_from_f64 = [&](wasm_op trunc_op, double x) -> void
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, x);
            op(fb.code, trunc_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i64_from_f32 = [&](wasm_op trunc_op, float x) -> void
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, x);
            op(fb.code, trunc_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i64_from_f64 = [&](wasm_op trunc_op, double x) -> void
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, x);
            op(fb.code, trunc_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        float const nanf = ::std::numeric_limits<float>::quiet_NaN();
        double const nand = ::std::numeric_limits<double>::quiet_NaN();

        // f0..f7: invalid conversion (NaN) - should hit trap_invalid_conversion_to_integer_func.
        add_i32_from_f32(wasm_op::i32_trunc_f32_s, nanf);
        add_i32_from_f32(wasm_op::i32_trunc_f32_u, nanf);
        add_i32_from_f64(wasm_op::i32_trunc_f64_s, nand);
        add_i32_from_f64(wasm_op::i32_trunc_f64_u, nand);

        add_i64_from_f32(wasm_op::i64_trunc_f32_s, nanf);
        add_i64_from_f32(wasm_op::i64_trunc_f32_u, nanf);
        add_i64_from_f64(wasm_op::i64_trunc_f64_s, nand);
        add_i64_from_f64(wasm_op::i64_trunc_f64_u, nand);

        // f8..f15: integer overflow - should hit trap_integer_overflow_func.
        // Use values well outside the destination range.
        add_i32_from_f32(wasm_op::i32_trunc_f32_s, 1.0e20f);
        add_i32_from_f32(wasm_op::i32_trunc_f32_u, -1.0f);
        add_i32_from_f64(wasm_op::i32_trunc_f64_s, 1.0e40);
        add_i32_from_f64(wasm_op::i32_trunc_f64_u, -1.0);

        add_i64_from_f32(wasm_op::i64_trunc_f32_s, 1.0e20f);
        add_i64_from_f32(wasm_op::i64_trunc_f32_u, -1.0f);
        add_i64_from_f64(wasm_op::i64_trunc_f64_s, 1.0e40);
        add_i64_from_f64(wasm_op::i64_trunc_f64_u, -1.0);

        auto add_param_trunc = [&](::std::uint8_t input_type, ::std::uint8_t output_type, wasm_op trunc_op) -> void
        {
            func_type ty{{input_type}, {output_type}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            append_u32_leb(fb.code, 0u);
            op(fb.code, wasm_op::nop);  // Keep this matrix on the non-fused fast/byref conversion paths.
            op(fb.code, trunc_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f16..f23: parameterized forms used for exact lower/upper boundary coverage.
        add_param_trunc(k_val_f32, k_val_i32, wasm_op::i32_trunc_f32_s);
        add_param_trunc(k_val_f32, k_val_i32, wasm_op::i32_trunc_f32_u);
        add_param_trunc(k_val_f64, k_val_i32, wasm_op::i32_trunc_f64_s);
        add_param_trunc(k_val_f64, k_val_i32, wasm_op::i32_trunc_f64_u);
        add_param_trunc(k_val_f32, k_val_i64, wasm_op::i64_trunc_f32_s);
        add_param_trunc(k_val_f32, k_val_i64, wasm_op::i64_trunc_f32_u);
        add_param_trunc(k_val_f64, k_val_i64, wasm_op::i64_trunc_f64_s);
        add_param_trunc(k_val_f64, k_val_i64, wasm_op::i64_trunc_f64_u);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_trunc_trap_matrix_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        auto run_one = [&](::std::size_t fidx, char const* expected_message) noexcept -> int
        {
            int const ec = run_in_child_expect_trap_message(expected_message,
                                                            [&]
                                                            {
                                                                (void)Runner::run(cm.local_funcs.index_unchecked(fidx),
                                                                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                                                                  pack_no_params(),
                                                                                  nullptr,
                                                                                  nullptr);
                                                                exit_98();
                                                            });
            if(ec != 0)
            {
                ::std::fprintf(stderr, "uwvm2test: trunc trap case failed: fidx=%zu\n", fidx);
            }
            return ec;
        };
#else
        auto run_one = [&](::std::size_t fidx, ::std::initializer_list<int> expected_exits) noexcept -> int
        {
            int const ec = run_in_child_expect_exit_oneof(expected_exits,
                                                    [&]
                                                    {
                                                        // Child must see the same trap hooks as parent (sanity against unexpected overrides).
                                                        if(optable::trap_invalid_conversion_to_integer_func != exit_12) { _exit(92); }
                                                        if(optable::trap_integer_overflow_func != exit_11) { _exit(91); }

                                                        (void)Runner::run(cm.local_funcs.index_unchecked(fidx),
                                                                          rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                                                          pack_no_params(),
                                                                          nullptr,
                                                                          nullptr);
                                                        exit_98();
                                                    });
            if(ec != 0)
            {
                ::std::fprintf(stderr, "uwvm2test: trunc trap case failed: fidx=%zu\n", fidx);
            }
            return ec;
        };
#endif

        // f0..f7 => invalid conversion
        for(::std::size_t i{}; i != 8uz; ++i)
        {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            int const ec = run_one(i, "invalid conversion to integer");
#else
            int const ec = run_one(i, {12});
#endif
            if(ec != 0) { return ec; }
        }

        // f8..f15 => overflow (must hit trap_integer_overflow_func).
        for(::std::size_t i{8uz}; i != 16uz; ++i)
        {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            int const ec = run_one(i, "integer overflow");
#else
            int const ec = run_one(i, {11});
#endif
            if(ec != 0) { return ec; }
        }

#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        auto run_param_trap = [&](::std::size_t fidx, byte_vec const& params, char const* expected_message) noexcept -> int
        {
            return run_in_child_expect_trap_message(expected_message,
                                                    [&]
                                                    {
                                                        (void)Runner::run(cm.local_funcs.index_unchecked(fidx),
                                                                          rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                                                          params,
                                                                          nullptr,
                                                                          nullptr);
                                                        exit_98();
                                                    });
        };
#else
        auto run_param_trap = [&](::std::size_t fidx, byte_vec const& params, int expected_exit) noexcept -> int
        {
            return run_in_child_expect_exit(expected_exit,
                                            [&]
                                            {
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
#endif

        auto require_param_trap = [&](::std::size_t fidx, byte_vec const& params, int expected_exit) noexcept -> int
        {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            return run_param_trap(fidx, params, expected_exit == 12 ? "invalid conversion to integer" : "integer overflow");
#else
            return run_param_trap(fidx, params, expected_exit);
#endif
        };

        auto run_i32_f32 = [&](::std::size_t fidx, float value) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(value),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };
        auto run_i32_f64 = [&](::std::size_t fidx, double value) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64(value),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };
        auto run_i64_f32 = [&](::std::size_t fidx, float value) noexcept -> ::std::int64_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(value),
                                  nullptr,
                                  nullptr);
            return load_i64(rr.results);
        };
        auto run_i64_f64 = [&](::std::size_t fidx, double value) noexcept -> ::std::int64_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64(value),
                                  nullptr,
                                  nullptr);
            return load_i64(rr.results);
        };

        // Unsigned truncation is valid for -1 < x < 0 and produces zero.
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f32(17uz, -0.5f)) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f32(17uz, ::std::nextafter(-1.0f, 0.0f))) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f64(19uz, -0.5)) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f64(19uz, ::std::nextafter(-1.0, 0.0))) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f32(21uz, -0.5f)) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f32(21uz, ::std::nextafter(-1.0f, 0.0f))) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f64(23uz, -0.5)) == 0u);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f64(23uz, ::std::nextafter(-1.0, 0.0))) == 0u);

        // The greatest representable source below each exclusive upper bound remains valid.
        float const i32_f32_upper_ok{::std::nextafter(2147483648.0f, 0.0f)};
        double const i32_f64_upper_ok{::std::nextafter(2147483648.0, 0.0)};
        float const u32_f32_upper_ok{::std::nextafter(4294967296.0f, 0.0f)};
        double const u32_f64_upper_ok{::std::nextafter(4294967296.0, 0.0)};
        float const i64_f32_upper_ok{::std::nextafter(9223372036854775808.0f, 0.0f)};
        double const i64_f64_upper_ok{::std::nextafter(9223372036854775808.0, 0.0)};
        float const u64_f32_upper_ok{::std::nextafter(18446744073709551616.0f, 0.0f)};
        double const u64_f64_upper_ok{::std::nextafter(18446744073709551616.0, 0.0)};
        UWVM2TEST_REQUIRE(run_i32_f32(16uz, i32_f32_upper_ok) == static_cast<::std::int32_t>(i32_f32_upper_ok));
        UWVM2TEST_REQUIRE(run_i32_f64(18uz, i32_f64_upper_ok) == static_cast<::std::int32_t>(i32_f64_upper_ok));
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f32(17uz, u32_f32_upper_ok)) == static_cast<::std::uint32_t>(u32_f32_upper_ok));
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(run_i32_f64(19uz, u32_f64_upper_ok)) == static_cast<::std::uint32_t>(u32_f64_upper_ok));
        UWVM2TEST_REQUIRE(run_i64_f32(20uz, i64_f32_upper_ok) == static_cast<::std::int64_t>(i64_f32_upper_ok));
        UWVM2TEST_REQUIRE(run_i64_f64(22uz, i64_f64_upper_ok) == static_cast<::std::int64_t>(i64_f64_upper_ok));
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f32(21uz, u64_f32_upper_ok)) == static_cast<::std::uint64_t>(u64_f32_upper_ok));
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64_f64(23uz, u64_f64_upper_ok)) == static_cast<::std::uint64_t>(u64_f64_upper_ok));

        // Signed lower sentinels are exclusive. Their next representable value toward +infinity is valid;
        // for f64->i32 that includes fractional values below INT32_MIN which truncate back to INT32_MIN.
        float const i32_f32_lower_bad{-2147483904.0f};
        double const i32_f64_lower_bad{-2147483649.0};
        float const i64_f32_lower_bad{-9223373136366403584.0f};
        double const i64_f64_lower_bad{-9223372036854777856.0};
        UWVM2TEST_REQUIRE(run_i32_f32(16uz, ::std::nextafter(i32_f32_lower_bad, 0.0f)) == ::std::numeric_limits<::std::int32_t>::min());
        UWVM2TEST_REQUIRE(run_i32_f64(18uz, -2147483648.5) == ::std::numeric_limits<::std::int32_t>::min());
        UWVM2TEST_REQUIRE(run_i32_f64(18uz, ::std::nextafter(i32_f64_lower_bad, 0.0)) == ::std::numeric_limits<::std::int32_t>::min());
        UWVM2TEST_REQUIRE(run_i64_f32(20uz, ::std::nextafter(i64_f32_lower_bad, 0.0f)) == ::std::numeric_limits<::std::int64_t>::min());
        UWVM2TEST_REQUIRE(run_i64_f64(22uz, ::std::nextafter(i64_f64_lower_bad, 0.0)) == ::std::numeric_limits<::std::int64_t>::min());

        float const nanf{::std::numeric_limits<float>::quiet_NaN()};
        double const nand{::std::numeric_limits<double>::quiet_NaN()};
        float const pinff{::std::numeric_limits<float>::infinity()};
        double const pinfd{::std::numeric_limits<double>::infinity()};

        // NaN is invalid conversion; both infinities are integer overflow for all eight opcodes.
        for(::std::size_t fidx : {16uz, 17uz, 20uz, 21uz})
        {
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f32(nanf), 12) == 0);
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f32(pinff), 11) == 0);
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f32(-pinff), 11) == 0);
        }
        for(::std::size_t fidx : {18uz, 19uz, 22uz, 23uz})
        {
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f64(nand), 12) == 0);
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f64(pinfd), 11) == 0);
            UWVM2TEST_REQUIRE(require_param_trap(fidx, pack_f64(-pinfd), 11) == 0);
        }

        // Exact lower and upper overflow boundaries.
        UWVM2TEST_REQUIRE(require_param_trap(16uz, pack_f32(i32_f32_lower_bad), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(18uz, pack_f64(i32_f64_lower_bad), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(20uz, pack_f32(i64_f32_lower_bad), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(22uz, pack_f64(i64_f64_lower_bad), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(16uz, pack_f32(2147483648.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(18uz, pack_f64(2147483648.0), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(20uz, pack_f32(9223372036854775808.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(22uz, pack_f64(9223372036854775808.0), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(17uz, pack_f32(-1.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(19uz, pack_f64(-1.0), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(21uz, pack_f32(-1.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(23uz, pack_f64(-1.0), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(17uz, pack_f32(4294967296.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(19uz, pack_f64(4294967296.0), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(21uz, pack_f32(18446744073709551616.0f), 11) == 0);
        UWVM2TEST_REQUIRE(require_param_trap(23uz, pack_f64(18446744073709551616.0), 11) == 0);

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_trunc_trap_matrix(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 24uz);

#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        // Hooks must remain intact after compilation.
        UWVM2TEST_REQUIRE(optable::trap_invalid_conversion_to_integer_func == exit_12);
        UWVM2TEST_REQUIRE(optable::trap_integer_overflow_func == exit_11);
#endif

        // Keep bytecode shape checks to non-cached layouts. Cached ABIs can legitimately select different stacktop-specialized variants after `f32.const`.
        if constexpr(Opt.i32_stack_top_begin_pos == SIZE_MAX && Opt.i64_stack_top_begin_pos == SIZE_MAX && Opt.f32_stack_top_begin_pos == SIZE_MAX &&
                     Opt.f64_stack_top_begin_pos == SIZE_MAX && Opt.v128_stack_top_begin_pos == SIZE_MAX)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
            auto const exp_trunc =
                optable::translate::get_uwvmint_i32_trunc_f32_s_fptr_from_tuple<Opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8uz).op.operands, exp_trunc));

            // f8 is `f32.const 1.0e20` then `i32.trunc_f32_s`. Verify the immediate bits survive translation.
            auto const exp_f32_const = optable::translate::get_uwvmint_f32_const_fptr_from_tuple<Opt>(curr, tuple);
            auto const& bc = cm.local_funcs.index_unchecked(8uz).op.operands;

            auto find_fptr_off = [&](auto fptr) noexcept -> ::std::size_t
            {
                if(fptr == nullptr) { return SIZE_MAX; }
                ::std::array<::std::byte, sizeof(fptr)> needle{};
                ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(fptr));
                if(bc.size() < needle.size()) { return SIZE_MAX; }
                for(::std::size_t i{}; i + needle.size() <= bc.size(); ++i)
                {
                    if(::std::memcmp(bc.data() + i, needle.data(), needle.size()) == 0) { return i; }
                }
                return SIZE_MAX;
            };

            ::std::size_t const off = find_fptr_off(exp_f32_const);
            UWVM2TEST_REQUIRE(off != SIZE_MAX);
            UWVM2TEST_REQUIRE(off + sizeof(exp_f32_const) + 4uz <= bc.size());
            ::std::uint32_t imm_bits{};
            ::std::memcpy(::std::addressof(imm_bits), bc.data() + off + sizeof(exp_f32_const), sizeof(imm_bits));
            UWVM2TEST_REQUIRE(imm_bits == 0x60ad78ecu);

            UWVM2TEST_REQUIRE(off + sizeof(exp_f32_const) + 4uz + sizeof(exp_trunc) <= bc.size());
            ::std::array<::std::byte, sizeof(exp_trunc)> trunc_bytes{};
            ::std::memcpy(trunc_bytes.data(), ::std::addressof(exp_trunc), sizeof(exp_trunc));
            UWVM2TEST_REQUIRE(::std::memcmp(bc.data() + off + sizeof(exp_f32_const) + 4uz, trunc_bytes.data(), trunc_bytes.size()) == 0);
        }

        return run_trunc_trap_matrix_suite<Opt>(cm, rt);
    }

#endif

    [[nodiscard]] int test_translate_trunc_trap_matrix() noexcept
    {
#if !defined(__unix__) && !defined(__APPLE__)
        return 0;  // skip on non-POSIX platforms
#else
        optable::unreachable_func = exit_90;
        optable::trap_integer_divide_by_zero_func = exit_90;
        optable::trap_invalid_conversion_to_integer_func = exit_12;
        optable::trap_integer_overflow_func = exit_11;

        // No calls expected in this test.
        optable::call_func = ::uwvm2test::uwvm_int_strict::strict_terminate_call;
        optable::call_indirect_func = ::uwvm2test::uwvm_int_strict::strict_terminate_call_indirect;

        auto wasm = build_trunc_trap_matrix_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_trunc_trap_matrix");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            int const ec = compile_and_run_trunc_trap_matrix<opt>(rt);
            if(ec != 0) { return ec; }
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            int const ec = compile_and_run_trunc_trap_matrix<opt>(rt);
            if(ec != 0) { return ec; }
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            int const ec = compile_and_run_trunc_trap_matrix<opt>(rt);
            if(ec != 0) { return ec; }
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            int const ec = compile_and_run_trunc_trap_matrix<opt>(rt);
            if(ec != 0) { return ec; }
        }

        return 0;
#endif
    }
}  // namespace

int main()
{
    return test_translate_trunc_trap_matrix();
}
