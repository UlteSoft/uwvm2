#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr auto k_test_tail_scalar4_merged_opt{make_tailcall_scalar4_merged_opt<2uz>()};

    template <optable::uwvm_interpreter_translate_option_t Opt, typename Fn>
    [[nodiscard]] bool search_curr_i32(Fn&& fn) noexcept
    {
        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            auto curr = make_initial_stacktop_currpos<Opt>();
            curr.i32_stack_top_curr_pos = i32_pos;
            if(fn(curr)) { return true; }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename Fn>
    [[nodiscard]] bool search_curr_i32_f32(Fn&& fn) noexcept
    {
        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            for(::std::size_t f32_pos{Opt.f32_stack_top_begin_pos}; f32_pos != Opt.f32_stack_top_end_pos; ++f32_pos)
            {
                auto curr = make_initial_stacktop_currpos<Opt>();
                curr.i32_stack_top_curr_pos = i32_pos;
                curr.f32_stack_top_curr_pos = f32_pos;
                if(fn(curr)) { return true; }
            }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename Fn>
    [[nodiscard]] bool search_curr_i32_f64(Fn&& fn) noexcept
    {
        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            for(::std::size_t f64_pos{Opt.f64_stack_top_begin_pos}; f64_pos != Opt.f64_stack_top_end_pos; ++f64_pos)
            {
                auto curr = make_initial_stacktop_currpos<Opt>();
                curr.i32_stack_top_curr_pos = i32_pos;
                curr.f64_stack_top_curr_pos = f64_pos;
                if(fn(curr)) { return true; }
            }
        }
        return false;
    }

    [[nodiscard]] byte_vec pack_i32(::std::int32_t v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_f32(::std::int32_t a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_f64(::std::int32_t a, double b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_memory_scalar4_merged_local_plus_imm_nonzero_offset_module()
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
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 136);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x13572468u));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 244);
            op(c, wasm_op::f32_const); f32(c, 6.75f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 8u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 368);
            op(c, wasm_op::f64_const); f64(c, 12.5);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
#endif

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 28);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 4u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 28);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 4u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        {
            func_type ty{{k_val_i32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 36);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 12u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 36);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 16u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
#endif

        return mb.build();
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm,
                                     ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const& bc_i32_load = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc_i32_store = cm.local_funcs.index_unchecked(
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
                                                                      3
#else
                                                                      1
#endif
                                                                  ).op.operands;

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_load, optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_load, optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_load, optable::translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_store_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_i32_store, optable::translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const& bc_f32_load = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc_f64_load = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc_f32_store = cm.local_funcs.index_unchecked(4).op.operands;
        auto const& bc_f64_store = cm.local_funcs.index_unchecked(5).op.operands;

        UWVM2TEST_REQUIRE(search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_load, optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_load, optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_load, optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_load, optable::translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_load, optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_load, optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_store, optable::translate::get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_store, optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_store, optable::translate::get_uwvmint_f32_store_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_store, optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f32_store, optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_store, optable::translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_store, optable::translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_store, optable::translate::get_uwvmint_f64_store_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_store, optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_f64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc_f64_store, optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
#endif

        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);
#else
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);
#endif
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                            pack_i32(100),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x13572468u);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(220),
                                              nullptr,
                                              nullptr)
                                       .results) == 6.75f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(336),
                                              nullptr,
                                              nullptr)
                                       .results) == 12.5);
#endif

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
                                                          3
#else
                                                          1
#endif
                                                      ),
                                              rt.local_defined_function_vec_storage.index_unchecked(
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
                                                  3
#else
                                                  1
#endif
                                              ),
                                              pack_i32x2(400, static_cast<::std::int32_t>(0x6a5b4c3du)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0x6a5b4c3du));

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32_f32(500, -3.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == -3.25f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32_f64(620, -48.125),
                                              nullptr,
                                              nullptr)
                                       .results) == -48.125);
#endif

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt,
                                            ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
#endif
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar4_merged_local_plus_imm_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_local_plus_imm_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_local_plus_imm_nonzero_offset");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_scalar4_merged_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_scalar4_merged_opt>(rt, mem) == 0);
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_scalar4_merged_local_plus_imm_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
