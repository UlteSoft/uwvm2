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

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f32x2(float a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2(double a, double b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_delay_local_types_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32 regress (local.tee + br_if): delay_local must NOT fire when combine may rewrite local.tee.
        // (param i32) (result i32) (local i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: i32
            auto& c = fb.code;

            // local1 = -1 (so i32.add decrements)
            op(c, wasm_op::i32_const); i32(c, -1);
            op(c, wasm_op::local_set); u32(c, 1u);

            // block
            op(c, wasm_op::block); append_u8(c, k_block_empty);
            // loop
            op(c, wasm_op::loop); append_u8(c, k_block_empty);

            // local.get 0 ; nop ; local.get 1 ; i32.add ; local.tee 0 ; br_if 0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::br_if); u32(c, 0u);

            // end loop, end block
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            // return local0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 delay_local positive-case (param i32 i32) (result i32) (local i32)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: i32
            auto& c = fb.code;

            // local.get 0 ; nop ; local.get 1 ; i32.add ; local.tee 2 ; end
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64 delay_local positive-case (param i64 i64) (result i64) (local i64)
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local1: i64
            auto& c = fb.code;

            // local.get 0 ; nop ; local.get 1 ; i64.add ; local.tee 2 ; end
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f32 delay_local positive-case (param f32 f32) (result f32) (local f32)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local1: f32
            auto& c = fb.code;

            // local.get 0 ; nop ; local.get 1 ; f32.add ; local.tee 2 ; end
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f64 delay_local positive-case (param f64 f64) (result f64) (local f64)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local1: f64
            auto& c = fb.code;

            // local.get 0 ; nop ; local.get 1 ; f64.add ; local.tee 2 ; end
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_delay_local_types() noexcept
    {
        // Install optable hooks (unexpected traps/calls should terminate the test process).
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_types_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_types");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        runtime_module_t const& rt = *prep.mod;

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<opt>;
        [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        [[maybe_unused]] constexpr auto tuple =
            compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto exp_br_if_local_tee =
            optable::translate::get_uwvmint_br_if_local_tee_nz_fptr_from_tuple<opt>(curr, tuple);
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
        constexpr auto exp_delay_i32_add_tee =
            optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                opt,
                optable::numeric_details::int_binop::add>(curr, tuple);
        constexpr auto exp_delay_i64_add =
            optable::translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<
                opt,
                optable::numeric_details::int_binop::add>(curr, tuple);
        constexpr auto exp_delay_f32_add =
            optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                opt,
                optable::numeric_details::float_binop::add>(curr, tuple);
        constexpr auto exp_delay_f64_add =
            optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                opt,
                optable::numeric_details::float_binop::add>(curr, tuple);

        // Sanity: bytecode should actually contain the expected mega-op pointer in the simple positive cases.
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_delay_i32_add_tee));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_delay_i64_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_delay_f32_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_delay_f64_add));

        // Regress: ensure delay_local *_local_tee is not used when local.tee is followed by br_if (local.tee+br_if may fuse).
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_delay_i32_add_tee));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        // Regress: local.tee + br_if should fuse.
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_br_if_local_tee));
#endif

        // f0: i32 loop => 0
        for(int v : {1, 2, 3, 100})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
        }

        // f1: i32 delay_local positive => a+b and must hit delay_local mega-op when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(3, 4),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7);
        }

        // f2: i64 delay_local positive => a+b and must hit delay_local mega-op when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i64x2(0x123456789ll, 1001ll),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr.results) == 0x123456b72ll);
        }

        // f3: f32 delay_local positive => a+b and must hit delay_local mega-op when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_f32x2(1.5f, 2.25f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 3.75f);
        }

        // f4: f64 delay_local positive => a+b and must hit delay_local mega-op when enabled.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_f64x2(6.5, 8.25),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 14.75);
        }

        // Byref mode: semantics smoke (delay_local/combine mega-ops are not expected to persist across opcodes).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt2{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err2{};
            optable::compile_option cop2{};
            auto cm2 = compiler::compile_all_from_uwvm_single_func<opt2>(rt, cop2, err2);
            UWVM2TEST_REQUIRE(err2.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner2 = interpreter_runner<opt2>;

            for(int v : {1, 2, 3, 100})
            {
                auto rr = Runner2::run(cm2.local_funcs.index_unchecked(0),
                                       rt.local_defined_function_vec_storage.index_unchecked(0),
                                       pack_i32(v),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
            }

            UWVM2TEST_REQUIRE(load_i32(Runner2::run(cm2.local_funcs.index_unchecked(1),
                                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                                   pack_i32x2(3, 4),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 7);

            UWVM2TEST_REQUIRE(load_i64(Runner2::run(cm2.local_funcs.index_unchecked(2),
                                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                                   pack_i64x2(0x123456789ll, 1001ll),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 0x123456b72ll);

            UWVM2TEST_REQUIRE(load_f32(Runner2::run(cm2.local_funcs.index_unchecked(3),
                                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                                   pack_f32x2(1.5f, 2.25f),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 3.75f);

            UWVM2TEST_REQUIRE(load_f64(Runner2::run(cm2.local_funcs.index_unchecked(4),
                                                   rt.local_defined_function_vec_storage.index_unchecked(4),
                                                   pack_f64x2(6.5, 8.25),
                                                   nullptr,
                                                   nullptr)
                                            .results) == 14.75);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_delay_local_types();
}
