#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_quick_branchy_i32_loop_run_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: matches the extra-heavy quick_branchy_i32 loop mega-fusion pattern (when enabled).
        // Returns: acc ^ s ^ cnt  (cnt should be 0 at exit)
        {
            constexpr ::std::uint32_t k_s = 0u;
            constexpr ::std::uint32_t k_t = 1u;
            constexpr ::std::uint32_t k_acc = 2u;
            constexpr ::std::uint32_t k_cnt = 3u;

            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0 s
            fb.locals.push_back({1u, k_val_i32});  // local1 t
            fb.locals.push_back({1u, k_val_i32});  // local2 acc
            fb.locals.push_back({1u, k_val_i32});  // local3 cnt
            auto& c = fb.code;

            // init s/acc/cnt
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::local_set);
            u32(c, k_s);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, k_t);

            op(c, wasm_op::i32_const);
            i32(c, 0x12345678);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::i32_const);
            i32(c, 97);
            op(c, wasm_op::local_set);
            u32(c, k_cnt);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // Canonical loop body (must match wasm1.h pattern exactly).
            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_const);
            i32(c, 1664525);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::local_tee);
            u32(c, k_t);
            op(c, wasm_op::i32_const);
            i32(c, 1013904223);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, k_s);
            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, k_t);
            op(c, wasm_op::i32_const);
            // 3668339992u as i32
            i32(c, static_cast<::std::int32_t>(static_cast<::std::uint32_t>(3668339992u)));
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::select);

            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::select);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_set);
            u32(c, k_acc);

            op(c, wasm_op::local_get);
            u32(c, k_cnt);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, k_cnt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // Return: acc ^ s ^ cnt
            op(c, wasm_op::local_get);
            u32(c, k_acc);
            op(c, wasm_op::local_get);
            u32(c, k_s);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, k_cnt);
            op(c, wasm_op::i32_xor);

            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] ::std::uint32_t rotl32(::std::uint32_t x, unsigned r) noexcept
    {
        return ::std::rotl(x, static_cast<int>(r));
    }

    [[nodiscard]] ::std::int32_t expected_f0() noexcept
    {
        ::std::uint32_t cnt = 97u;
        ::std::uint32_t acc = 0x12345678u;
        ::std::uint32_t s = 1u;

        constexpr ::std::uint32_t a = 1664525u;
        constexpr ::std::uint32_t b = 1013904223u;
        constexpr ::std::uint32_t c = 3668339992u;

        while(cnt != 0u)
        {
            ::std::uint32_t const t = static_cast<::std::uint32_t>(s * a);
            ::std::uint32_t const s2 = static_cast<::std::uint32_t>(t + b);

            ::std::uint32_t const v1 = static_cast<::std::uint32_t>(s2 + acc);
            ::std::uint32_t const v2 = static_cast<::std::uint32_t>((t + c) ^ acc);
            ::std::uint32_t const sel1 = (s2 & 1u) ? v1 : v2;

            ::std::uint32_t const shr = static_cast<::std::uint32_t>(s2 >> 3u);
            ::std::uint32_t const neg_shl = static_cast<::std::uint32_t>(0u - static_cast<::std::uint32_t>(s2 << 1u));
            ::std::uint32_t const sel2 = (s2 & 4u) ? shr : neg_shl;

            ::std::uint32_t const sum = static_cast<::std::uint32_t>(sel1 + sel2);
            acc = rotl32(sum, 5u);
            s = s2;

            cnt = static_cast<::std::uint32_t>(cnt - 1u);
        }

        ::std::uint32_t const result = (acc ^ s) ^ cnt;
        return static_cast<::std::int32_t>(result);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_quick_branchy_i32_loop_run_suite(runtime_module_t const& rt, ::std::int32_t expected) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == expected);

        return 0;
    }

    [[nodiscard]] int test_translate_quick_branchy_i32_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        ::std::int32_t const expected = expected_f0();

        auto wasm = build_quick_branchy_i32_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_quick_branchy_i32_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: bytecode should contain the mega-op when extra-heavy combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_loop_run =
                optable::translate::get_uwvmint_quick_branchy_i32_loop_run_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_loop_run));
#endif

            using Runner = interpreter_runner<opt>;
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == expected);
        }

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_quick_branchy_i32_loop_run_suite<opt>(rt, expected) == 0);
        }

        // Tailcall + stacktop caching: semantics.
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
            UWVM2TEST_REQUIRE(run_quick_branchy_i32_loop_run_suite<opt>(rt, expected) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_quick_branchy_i32_loop_run();
}

