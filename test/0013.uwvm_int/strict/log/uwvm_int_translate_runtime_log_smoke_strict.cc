#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_runtime_log_smoke_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // global0: (mut i32) = 1
        {
            global_entry g{};
            g.valtype = k_val_i32;
            g.mut = true;
            op(g.init_expr, wasm_op::i32_const);
            i32(g.init_expr, 1);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        // f0: () -> (i32)
        // - memory store/load
        // - local.set / local.tee
        // - global.get / global.set
        // - if (result i32)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0: i32
            auto& c = fb.code;

            // i32.store 42 at mem[0]
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_const);
            i32(c, 42);
            op(c, wasm_op::i32_store);
            u32(c, 2u);  // align=4
            u32(c, 0u);  // offset=0

            // local0 = i32.load mem[0]
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_load);
            u32(c, 2u);  // align=4
            u32(c, 0u);  // offset=0
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // local0 = (global0 + local0); global0 = local0;
            op(c, wasm_op::global_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::global_set);
            u32(c, 0u);

            // Reset global0 to keep this function idempotent across repeated runs in the same process.
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::global_set);
            u32(c, 0u);

            // if (local0 > 40) then local0 else 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 40);
            op(c, wasm_op::i32_gt_s);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);  // (result i32)
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::end);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_with_runtime_log(runtime_module_t const& rt, runtime_local_func_t const& rt_fn)
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};

        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(!cm.local_funcs.empty());

        auto rr = interpreter_runner<Opt>::run(cm.local_funcs.index_unchecked(0), rt_fn, pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr.results.size() == 4uz);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 43);
        return 0;
    }

    [[nodiscard]] int test_translate_runtime_log_smoke()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_runtime_log_smoke_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_runtime_log_smoke");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_function_vec_storage.empty());
        runtime_local_func_t const& rt_fn = rt.local_defined_function_vec_storage.index_unchecked(0);

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
            UWVM2TEST_REQUIRE((compile_and_run_with_runtime_log<opt>(rt, rt_fn)) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((compile_and_run_with_runtime_log<opt>(rt, rt_fn)) == 0);
        }

        // tailcall + stacktop caching (scalar4 merged, minimal size => force spill/fill paths)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 4uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 4uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 4uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 4uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((compile_and_run_with_runtime_log<opt>(rt, rt_fn)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_runtime_log_smoke();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
