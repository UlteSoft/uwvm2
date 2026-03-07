#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

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

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    inline ::std::size_t g_expected_wasm_id{};

    // Minimal imported-call bridge:
    // - compiler encodes imported calls as (module_id=compile_option.curr_wasm_id, call_function=funcidx)
    // - in this test, the only imported function is funcidx=0 with signature (i32,i32)->i32, implemented as `a+b`.
    static void imported_call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(wasm_module_id != g_expected_wasm_id) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(call_function != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

        // stack layout: [a(i32), b(i32)] (top points past b)
        ::std::byte* const top = *stack_top_ptr;
        ::std::byte* const base = top - 8;
        ::std::uint32_t const a = load_u32(base);
        ::std::uint32_t const b = load_u32(base + 4);
        ::std::uint32_t const r = a + b;
        store_u32(base, r);
        *stack_top_ptr = base + 4;
    }

    [[nodiscard]] byte_vec build_modC()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 2u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // global0: (mut i32) = 7, exported as "g"
        {
            global_entry g{};
            g.valtype = k_val_i32;
            g.mut = true;
            op(g.init_expr, wasm_op::i32_const);
            i32(g.init_expr, 7);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
            mb.add_export_global(0u, "g");
        }

        // f0: add(i32,i32)->i32, exported as "add"
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);

            auto const local_idx = mb.add_func(::std::move(ty), ::std::move(fb));
            mb.add_export_func(local_idx, "add");
        }

        mb.add_export_memory("mem");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_mod_import_chain(char const* self_name,
                                                  char const* upstream_module,
                                                  char const* upstream_mem,
                                                  char const* upstream_global,
                                                  char const* upstream_func)
    {
        (void)self_name;
        module_builder mb{};

        // type0: (i32,i32)->i32 for imported "add"
        func_type add_ty{{k_val_i32, k_val_i32}, {k_val_i32}};
        auto const ty0 = static_cast<::std::uint32_t>(mb.types.size());
        mb.types.push_back(::std::move(add_ty));

        mb.add_import_memory(upstream_module, upstream_mem, 1u, 2u, true);
        mb.add_import_global(upstream_module, upstream_global, k_val_i32, true);
        mb.add_import_func(upstream_module, upstream_func, ty0);

        // Re-export all imports with the same names.
        mb.add_export_memory("mem");
        mb.add_export_global(0u, "g");
        mb.add_export_func(0u, "add");

        return mb.build();
    }

    [[nodiscard]] byte_vec build_main_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // type0: imported add (i32,i32)->i32
        {
            func_type add_ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            mb.types.push_back(::std::move(add_ty));
        }
        mb.add_import_memory("modA", "mem", 1u, 2u, true);
        mb.add_import_global("modA", "g", k_val_i32, true);
        mb.add_import_func("modA", "add", 0u);

        // f0: imported memory/global + imported call smoke.
        // return (mem[0]=42; g=7; g=(mem[0]+g); g + add(a,b))
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // reset imported global: g=7
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::global_set);
            u32(c, 0u);

            // store 42 at mem[0]
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 42);
            op(c, wasm_op::i32_store);
            u32(c, 2u);  // align=4
            u32(c, 0u);  // offset=0

            // load mem[0]
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);

            // + global.get 0
            op(c, wasm_op::global_get);
            u32(c, 0u);
            op(c, wasm_op::i32_add);  // 42+7=49

            // global.set 0 (g=49)
            op(c, wasm_op::global_set);
            u32(c, 0u);

            // global.get 0 (49)
            op(c, wasm_op::global_get);
            u32(c, 0u);

            // add(a,b)
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::call);
            u32(c, 0u);  // imported function index 0

            // 49 + (a+b)
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: direct imported call (i32,i32)->i32
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: call + drop (fuse_call_drop)
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: call + local.set/get (fuse_call_local_set)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::call);
            u32(c, 0u);
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt, ::std::size_t wasm_id) noexcept
    {
        g_expected_wasm_id = wasm_id;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{.curr_wasm_id = wasm_id};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: 49 + (a+b)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x2(10, 20),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 79);
        }

        // f1: add(a,b)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(123, 456),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 579);
        }

        // f2: call+drop => no return, but must not trap.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32x2(-1, 2),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.results.empty());
        }

        // f3: call+local.set/get => add(a,b)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i32x2(0x11111111, 0x22222222),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == static_cast<::std::int32_t>(0x33333333u));
        }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && \
    defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            // Important: stacktop currpos must be within the configured range.
            // Use `begin_pos` here; for this test's call sites (two i32 local.get pushes then call),
            // the call_stacktop opfunc is selected at `currpos==begin_pos`.
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{
                .i32_stack_top_curr_pos = Opt.i32_stack_top_begin_pos,
                .i64_stack_top_curr_pos = Opt.i64_stack_top_begin_pos,
                .f32_stack_top_curr_pos = Opt.f32_stack_top_begin_pos,
                .f64_stack_top_curr_pos = Opt.f64_stack_top_begin_pos,
                .v128_stack_top_curr_pos = Opt.v128_stack_top_begin_pos,
            };
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const exp_call2_i32 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<Opt, 2uz, wasm_i32>(curr, tuple);
            auto const exp_call2_drop = optable::translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<Opt, 2uz>(curr, tuple);
            auto const exp_call2_local_set = optable::translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<Opt, 2uz>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_call2_i32));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_call2_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_call2_local_set));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_import_chain_mem_global_call() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +imported_call_bridge;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        // C defines, B imports from C, A imports from B, main imports from A.
        auto const modC = build_modC();
        auto const modB = build_mod_import_chain("modB", "modC", "mem", "g", "add");
        auto const modA = build_mod_import_chain("modA", "modB", "mem", "g", "add");
        auto const main_wasm = build_main_module();

        auto prep = prepare_runtime_from_wasm(main_wasm,
                                              u8"main",
                                              {
                                                  {.wasm_bytes = &modA, .module_name = u8"modA"},
                                                  {.wasm_bytes = &modB, .module_name = u8"modB"},
                                                  {.wasm_bytes = &modC, .module_name = u8"modC"},
                                              });

        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(rt.imported_memory_vec_storage.size() == 1uz);
        UWVM2TEST_REQUIRE(rt.imported_global_vec_storage.size() == 1uz);
        UWVM2TEST_REQUIRE(rt.imported_function_vec_storage.size() == 1uz);

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 42uz) == 0);
        }

        // tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 42uz) == 0);
        }

        // tailcall + stacktop caching
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
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, 42uz) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_import_chain_mem_global_call();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
