#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_memory_scalar_variants_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: i32.store + i32.load
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(0x11223344u));
            op(c, wasm_op::i32_store);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32.store8 + load8_s/u => (-128 + 128) == 0
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_const);
            i32(c, 0x80);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_load8_s);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::i32_load8_u);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i32.store16 + load16_s/u => (-32767 + 32769) == 2
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 6);
            op(c, wasm_op::i32_const);
            i32(c, 0x8001);
            op(c, wasm_op::i32_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 6);
            op(c, wasm_op::i32_load16_s);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 6);
            op(c, wasm_op::i32_load16_u);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64.store + i64.load
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i64_const);
            i64(c, static_cast<::std::int64_t>(0x0123456789abcdefull));
            op(c, wasm_op::i64_store);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i64_load);
            u32(c, 3u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64.store8 + load8_s/u => (-128 + 128) == 0
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 24);
            op(c, wasm_op::i64_const);
            i64(c, 0x80);
            op(c, wasm_op::i64_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 24);
            op(c, wasm_op::i64_load8_s);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 24);
            op(c, wasm_op::i64_load8_u);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i64.store16 + load16_s/u => 2
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 26);
            op(c, wasm_op::i64_const);
            i64(c, 0x8001);
            op(c, wasm_op::i64_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 26);
            op(c, wasm_op::i64_load16_s);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 26);
            op(c, wasm_op::i64_load16_u);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: i64.store32 + load32_s/u => 2
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 28);
            op(c, wasm_op::i64_const);
            i64(c, static_cast<::std::int64_t>(0x80000001ull));
            op(c, wasm_op::i64_store32);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 28);
            op(c, wasm_op::i64_load32_s);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 28);
            op(c, wasm_op::i64_load32_u);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: non-zero memarg offset smoke (store8 offset=7, load8_u offset=8) => 127
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 0x7f);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 7u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load8_u);
            u32(c, 0u);
            u32(c, 8u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_scalar_variants(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr0.results)) == 0x11223344u);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 0);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr2.results) == 2);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr3.results)) == 0x0123456789abcdefull);

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                               rt.local_defined_function_vec_storage.index_unchecked(4),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr4.results) == 0);

        auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                               rt.local_defined_function_vec_storage.index_unchecked(5),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr5.results) == 2);

        auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6),
                               rt.local_defined_function_vec_storage.index_unchecked(6),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr6.results) == 2);

        auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7),
                               rt.local_defined_function_vec_storage.index_unchecked(7),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr7.results) == 0x7f);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar_variants() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar_variants_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar_variants");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            auto rc = run_memory_scalar_variants<opt>(rt);
            UWVM2TEST_REQUIRE(rc == 0);
        }

        // Tailcall + merged scalar rings (exercise stacktop-aware memory ops).
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

            auto rc = run_memory_scalar_variants<opt>(rt);
            UWVM2TEST_REQUIRE(rc == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_scalar_variants();
}

