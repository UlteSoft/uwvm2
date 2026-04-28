#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using kind_t = optable::trivial_defined_call_kind;
    using info_t = optable::compiled_defined_call_info;

    inline ::std::size_t g_expected_wasm_id{};
    inline ::std::size_t g_direct_bridge_calls{};
    inline ::std::size_t g_import_bridge_calls{};

    [[nodiscard]] inline ::std::uint32_t load_u32(::std::byte const* p) noexcept
    {
        ::std::uint32_t v{};
        ::std::memcpy(::std::addressof(v), p, sizeof(v));
        return v;
    }

    inline void store_u32(::std::byte* p, ::std::uint32_t v) noexcept
    {
        ::std::memcpy(p, ::std::addressof(v), sizeof(v));
    }

    [[nodiscard]] byte_vec pack_i32(::std::int32_t x)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(x), sizeof(x));
        return out;
    }

    static void mixed_call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(wasm_module_id == SIZE_MAX)
        {
            ++g_direct_bridge_calls;

            auto const* const info = reinterpret_cast<info_t const*>(call_function);
            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(info->trivial_kind != kind_t::add_const_i32) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(info->param_bytes != 4uz || info->result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto* const top = *stack_top_ptr;
            ::std::byte* const base = top - info->param_bytes;
            auto const a = load_u32(base);
            auto const imm = static_cast<::std::uint32_t>(::std::bit_cast<::std::uint_least32_t>(info->trivial_imm));
            store_u32(base, static_cast<::std::uint32_t>(a + imm));
            *stack_top_ptr = base + 4;
            return;
        }

        ++g_import_bridge_calls;
        if(wasm_module_id != g_expected_wasm_id) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(call_function != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const top = *stack_top_ptr;
        ::std::byte* const base = top - 4;
        auto const a = load_u32(base);
        store_u32(base, static_cast<::std::uint32_t>(a + 7u));
        *stack_top_ptr = base + 4;
    }

    [[nodiscard]] byte_vec build_cycle_relay_module()
    {
        module_builder mb{};

        func_type add_ty{{k_val_i32}, {k_val_i32}};
        mb.types.push_back(::std::move(add_ty));

        mb.add_import_func("main_cycle", "hot", 0u);
        mb.add_export_func(0u, "hot_alias");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_cycle_main_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type add_ty{{k_val_i32}, {k_val_i32}};
        mb.types.push_back(add_ty);
        mb.add_import_func("relay_cycle", "hot_alias", 0u);

        // local f0 / wasm funcidx 1: hot(x) -> x + 7
        {
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(add_ty, ::std::move(fb));
            mb.add_export_func(1u, "hot");
        }

        // local f1: direct imported alias call
        {
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(add_ty, ::std::move(fb));
        }

        // local f2: call + drop
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // local f3: call + local.set/get
        {
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::local_set);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(add_ty, ::std::move(fb));
        }

        // local f4: call + local.tee
        {
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::local_tee);
            u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(add_ty, ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt, ::std::size_t wasm_id) noexcept
    {
        g_expected_wasm_id = wasm_id;
        g_direct_bridge_calls = 0uz;
        g_import_bridge_calls = 0uz;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{.curr_wasm_id = wasm_id};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_defined_call_info.size() == 5uz);
        UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(0).trivial_kind == kind_t::add_const_i32);

        using Runner = interpreter_runner<Opt>;

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(11),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 18);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32(5),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.results.empty());
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i32(19),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 26);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_i32(32),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 39);
        }

        UWVM2TEST_REQUIRE(g_direct_bridge_calls == 4uz);
        UWVM2TEST_REQUIRE(g_import_bridge_calls == 0uz);
        return 0;
    }

    [[nodiscard]] int test_translate_import_cycle_direct_call() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +mixed_call_bridge;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const relay_wasm = build_cycle_relay_module();
        auto const main_wasm = build_cycle_main_module();

        auto prep = prepare_runtime_from_wasm(main_wasm,
                                              u8"main_cycle",
                                              {
                                                  {.wasm_bytes = &relay_wasm, .module_name = u8"relay_cycle"},
                                              });
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(rt.imported_function_vec_storage.size() == 1uz);
        UWVM2TEST_REQUIRE(rt.local_defined_function_vec_storage.size() == 5uz);

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 7uz) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 7uz) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 7uz) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 7uz) == 0);
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
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 7uz) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_import_cycle_direct_call();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
