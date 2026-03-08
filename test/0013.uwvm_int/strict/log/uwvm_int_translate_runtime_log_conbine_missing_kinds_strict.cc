#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_runtime_log_conbine_missing_kinds_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: const_f32_localget + f32_div_from_imm_localtee_wait
        // (src: f32) -> f32 : (3.0 / src) stored by local.tee
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f32_const);
            f32(c, 3.0f);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: const_f64_localget + f64_div_from_imm_localtee_wait
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f64_const);
            f64(c, 3.0);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: float_mul_2localget -> float_mul_2localget_local3 -> float_2mul_wait_second_mul -> float_2mul_after_second_mul
        // (a,b,c,d: f32) -> f32 : a*b + c*d
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: select_localget3 -> select_after_select
        // (a,b: i64, cond: i32) -> i64 : select(a,b,cond)
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: mac_localget3 -> mac_after_mul -> mac_after_add
        // (acc,x,y: i32) -> i32 : acc = acc + x*y ; return acc
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f32_add_2localget_local_set
        // (a,b: f32) -> f32 : tmp = a+b ; local.set tmp ; return tmp
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f32_add_2localget_local_tee
        // (a,b: f32) -> f32 : return (local.tee tmp = a+b)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_tee);
            u32(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f64_add_2localget_local_set
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local2
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: f64_add_2localget_local_tee
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local2
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_tee);
            u32(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: i32_rem_u_2localget_wait_eqz -> i32_rem_u_eqz_2localget_wait_brif
        // (a,b: i32) -> i32 : block { (a%b)==0 ? br_if : fallthrough } ; return 0
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_rem_u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: for_i32_inc_* loop skeleton
        // (i: i32) -> i32
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 10);
            op(c, wasm_op::i32_lt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: for_ptr_inc_* loop skeleton
        // (p: i32, pend: i32) -> i32
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: canonical test8 loop skeleton (for_i32_inc_f64_lt_u_eqz_*)
        // (n: f64, i: i32) -> i32
        {
            func_type ty{{k_val_f64, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 1u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: xorshift_* chain
        // (x: i32) -> i32 : (x ^ (x>>5)) ^ (x<<1)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f14: rot_xor_add_* chain
        // (x,y: i32) -> i32 : (rotl(x, 9) ^ y) - 123
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 9);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_const);
            i32(c, 123);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f15: rotl_xor_local_set_* chain (local.tee)
        // (x,y: i32) -> i32 : (local.tee x = y ^ rotl(x, 5))
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f16: u16_copy_scaled_index_* pending chain
        // (dst: i32, idx: i32) -> i32 : dst + load16_u((idx<<1)+off)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);  // dst
            op(c, wasm_op::local_get);
            u32(c, 1u);  // idx
            op(c, wasm_op::i32_const);
            i32(c, 1);  // sh
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u);
            u32(c, 1u);  // align=2
            u32(c, 4u);  // offset
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f17-f20: acc += f32.unop(local.get x) chains (ceil/trunc/nearest/abs)
        // These drive the heavy conbine state machine through the missing `f32_acc_add_*` kinds under runtime-log.
        {
            auto add_f32_acc_unop = [&](wasm_op unop) {
                func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
                func_body fb{};
                auto& c = fb.code;

                op(c, wasm_op::local_get);
                u32(c, 0u);  // acc
                op(c, wasm_op::local_get);
                u32(c, 1u);  // x
                op(c, unop);
                op(c, wasm_op::f32_add);
                op(c, wasm_op::local_set);
                u32(c, 0u);
                op(c, wasm_op::local_get);
                u32(c, 0u);
                op(c, wasm_op::end);

                (void)mb.add_func(::std::move(ty), ::std::move(fb));
            };

            add_f32_acc_unop(wasm_op::f32_ceil);
            add_f32_acc_unop(wasm_op::f32_trunc);
            add_f32_acc_unop(wasm_op::f32_nearest);
            add_f32_acc_unop(wasm_op::f32_abs);
        }

        // f21-f25: acc += f64.unop(local.get x) chains (floor/ceil/trunc/nearest/abs)
        // These drive the heavy conbine state machine through the missing `f64_acc_add_*` kinds under runtime-log.
        {
            auto add_f64_acc_unop = [&](wasm_op unop) {
                func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
                func_body fb{};
                auto& c = fb.code;

                op(c, wasm_op::local_get);
                u32(c, 0u);  // acc
                op(c, wasm_op::local_get);
                u32(c, 1u);  // x
                op(c, unop);
                op(c, wasm_op::f64_add);
                op(c, wasm_op::local_set);
                u32(c, 0u);
                op(c, wasm_op::local_get);
                u32(c, 0u);
                op(c, wasm_op::end);

                (void)mb.add_func(::std::move(ty), ::std::move(fb));
            };

            add_f64_acc_unop(wasm_op::f64_floor);
            add_f64_acc_unop(wasm_op::f64_ceil);
            add_f64_acc_unop(wasm_op::f64_trunc);
            add_f64_acc_unop(wasm_op::f64_nearest);
            add_f64_acc_unop(wasm_op::f64_abs);
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_only_with_runtime_log(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        return 0;
    }

    [[nodiscard]] int test_translate_runtime_log_conbine_missing_kinds()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_runtime_log_conbine_missing_kinds_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_runtime_log_conbine_missing_kinds");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Enable translator runtime-log and discard output to avoid spam.
#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"NUL", ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"/dev/null", ::fast_io::open_mode::out);
#endif
        ::uwvm2::uwvm::io::enable_runtime_log = true;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        // tailcall (required by some extra-heavy combine paths)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_runtime_log_conbine_missing_kinds();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
