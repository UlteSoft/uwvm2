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

    [[nodiscard]] byte_vec build_memory_localget2_flush_then_load_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: (param i32 dummy, i32 addr) -> f32
        // Force local_get2 pending, then trigger the `local_get2 -> emit local.get(off1) + keep local.get(off2)` flush path in f32.load.
        {
            constexpr ::std::uint32_t k_align_f32 = 2u;  // align=4 bytes
            func_type ty{{k_val_i32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2: f32 tmp
            auto& c = fb.code;

            // Initialize memory[addr] = 3.5f.
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_const);
            f32(c, 3.5f);
            op(c, wasm_op::f32_store);
            u32(c, k_align_f32);
            u32(c, 0u);

            // local.get dummy; local.get addr; f32.load
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_load);
            u32(c, k_align_f32);
            u32(c, 0u);

            // Pop result into tmp, then drop dummy, then return tmp.
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i32 dummy, i32 addr) -> i32
        // Same as f0 but for `i32.load8_s` (tailcall-only localget-off fusion).
        {
            constexpr ::std::uint32_t k_align_8 = 0u;  // align=1 byte
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2: i32 tmp
            auto& c = fb.code;

            // Initialize memory[addr] = 0x80 (signed -128).
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 0x80);
            op(c, wasm_op::i32_store8);
            u32(c, k_align_8);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_load8_s);
            u32(c, k_align_8);
            u32(c, 0u);

            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);
        using Runner = interpreter_runner<Opt>;

        // f0: f32.load should return 3.5f
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x2(123, 0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 3.5f);
        }

        // f1: i32.load8_s should sign-extend 0x80 to -128
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(777, 0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == -128);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_memory_localget2_flush_then_load_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_localget2_flush_then_load_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_localget2_flush_then_load_fuse");
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

            auto const exp_f32_load_localget_off = optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);
            auto const exp_i32_load8_s_localget_off = optable::translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<opt>(curr, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_load_localget_off));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_load8_s_localget_off));
#endif

            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_localget2_flush_then_load_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
