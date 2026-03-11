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

    [[nodiscard]] byte_vec build_memory_narrow_localget_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: local.get addr ; i32.load8_s  (tests i32_load8_s_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 0x80);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load8_s);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get addr ; i32.load8_u  (tests i32_load8_u_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 0x80);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load8_u);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get addr ; i32.load16_s  (tests i32_load16_s_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_const);
            i32(c, 0x8001);
            op(c, wasm_op::i32_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load16_s);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get addr ; i32.load16_u  (tests i32_load16_u_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_const);
            i32(c, 0x8001);
            op(c, wasm_op::i32_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load16_u);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get addr ; local.get v ; i32.store8  (tests i32_store8_localget_off when extra-heavy)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load8_u);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get addr ; i32.const imm ; i32.store8  (tests i32_store8_imm_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 0xcd);
            op(c, wasm_op::i32_store8);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load8_u);
            u32(c, 0u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local.get addr ; local.get v ; i32.store16  (tests i32_store16_localget_off when extra-heavy)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load16_u);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: local.get addr ; i32.const imm ; i32.store16  (tests i32_store16_imm_localget_off)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 0xcafe);
            op(c, wasm_op::i32_store16);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load16_u);
            u32(c, 1u);
            u32(c, 0u);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_narrow_localget_fuse_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

        // f0: load8_s => -128
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == -128);

        // f1: load8_u => 128
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 128);

        // f2: load16_s => -32767
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(2),
                                              nullptr,
                                              nullptr)
                                       .results) == -32767);

        // f3: load16_u => 32769
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(2),
                                              nullptr,
                                              nullptr)
                                       .results) == 32769);

        // f4: store8 localget_off => 0xab
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32x2(0, 0xab),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xab);

        // f5: store8 imm localget_off => 0xcd
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xcd);

        // f6: store16 localget_off => 0xbeef
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i32x2(0, 0xbeef),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xbeef);

        // f7: store16 imm localget_off => 0xcafe
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xcafe);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_narrow_localget_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_narrow_localget_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_narrow_localget_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        [[maybe_unused]] auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto curr{make_initial_stacktop_currpos<opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            auto const exp_load8_s = optable::translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_load8_u = optable::translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_load16_s = optable::translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_load16_u = optable::translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);

            auto const exp_store8_imm = optable::translate::get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_store16_imm = optable::translate::get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_load8_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_load8_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_load16_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_load16_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_store8_imm));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_store16_imm));

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            auto const exp_store8 = optable::translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_store16 = optable::translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_store8));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_store16));
# endif
#endif

            UWVM2TEST_REQUIRE(run_memory_narrow_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_narrow_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_narrow_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_narrow_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_memory_narrow_localget_fuse_suite<opt>(cm, rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_narrow_localget_fuse();
}
