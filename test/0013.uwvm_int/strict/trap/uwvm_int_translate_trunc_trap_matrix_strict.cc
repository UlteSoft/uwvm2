#include "../uwvm_int_translate_strict_common.h"

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

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_trunc_trap_matrix_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

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

        // f0..f7 => invalid conversion
        for(::std::size_t i{}; i != 8uz; ++i)
        {
            int const ec = run_one(i, {12});
            if(ec != 0) { return ec; }
        }

        // f8..f15 => overflow (must hit trap_integer_overflow_func).
        for(::std::size_t i{8uz}; i != 16uz; ++i)
        {
            int const ec = run_one(i, {11});
            if(ec != 0) { return ec; }
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_trunc_trap_matrix(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 16uz);

        // Hooks must remain intact after compilation.
        UWVM2TEST_REQUIRE(optable::trap_invalid_conversion_to_integer_func == exit_12);
        UWVM2TEST_REQUIRE(optable::trap_integer_overflow_func == exit_11);

        // Ensure f8 actually contains the expected `i32.trunc_f32_s` opfunc (helps detect unexpected fusions/layout changes).
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
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
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_trunc_trap_matrix_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_trunc_trap_matrix");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            int const ec = compile_and_run_trunc_trap_matrix<opt>(rt);
            if(ec != 0) { return ec; }
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
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
