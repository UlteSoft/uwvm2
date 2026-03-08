#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_memory_more_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;
        mb.export_memory = true;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i64 store/load roundtrip (param i64) (result i64)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 store32 + load32_s/u (param i64) (result i32)
        // returns (i32)(load32_s + load32_u)
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i64_load32_s); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_wrap_i64);

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_wrap_i64);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 store/load + trunc (param f32) (result i32)
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64 store/load + trunc (param f64) (result i32)
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        if(mb.export_memory) { mb.add_export_memory("mem"); }
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_more_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        for(::std::uint64_t x : {0ull, 1ull, 0x0123456789abcdefull, 0xffffffffffffffffull})
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i64(static_cast<::std::int64_t>(x)),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr0.results)) == x);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_i64(static_cast<::std::int64_t>(x)),
                                   nullptr,
                                   nullptr);
            ::std::uint32_t low = static_cast<::std::uint32_t>(x);
            ::std::int32_t s = static_cast<::std::int32_t>(low);
            ::std::uint32_t u = low;
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr1.results)) == (static_cast<::std::uint32_t>(s) + u));
        }

        for(float f : {0.0f, 1.0f, -1.0f, 3.25f, -3.25f})
        {
            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_f32(f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == static_cast<::std::int32_t>(f));
        }

        for(double d : {0.0, 1.0, -1.0, 6.5, -6.5})
        {
            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_f64(d),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr3.results) == static_cast<::std::int32_t>(d));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_memory_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_memory_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_memory_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_memory_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_memory_more_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
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
            UWVM2TEST_REQUIRE(run_memory_more_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_more();
}

