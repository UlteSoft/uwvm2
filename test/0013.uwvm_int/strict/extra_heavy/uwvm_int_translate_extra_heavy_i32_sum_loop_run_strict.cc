#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_i32_sum_loop_run_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: matches the extra-heavy i32 sum-loop mega-fusion pattern (when enabled):
        // - memory[sp+0] = i (i32), memory[sp+4] = sum (i32)
        // - while (i < end) { sum += i; i += step; }
        // - return sum
        {
            constexpr ::std::uint32_t k_sp_local = 0u;
            constexpr ::std::uint32_t k_align_i32 = 2u;  // align=4
            constexpr ::std::uint32_t k_off_i = 0u;
            constexpr ::std::uint32_t k_off_sum = 4u;
            constexpr ::std::int32_t k_end = 10;
            constexpr ::std::int32_t k_step = 1;

            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0: sp (i32)
            auto& c = fb.code;

            // sp = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, k_sp_local);

            // init i = 0
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_store);
            u32(c, k_align_i32);
            u32(c, k_off_i);

            // init sum = 0
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_store);
            u32(c, k_align_i32);
            u32(c, k_off_sum);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // Header:
            // local.get sp; i32.load off_i; i32.const end; i32.lt_s; i32.const 1; i32.and; i32.eqz; br_if 1
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_load);
            u32(c, k_align_i32);
            u32(c, k_off_i);
            op(c, wasm_op::i32_const);
            i32(c, k_end);
            op(c, wasm_op::i32_lt_s);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 1u);

            // sum update:
            // local.get sp; local.get sp; i32.load off_sum; local.get sp; i32.load off_i; i32.add; i32.store off_sum
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_load);
            u32(c, k_align_i32);
            u32(c, k_off_sum);
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_load);
            u32(c, k_align_i32);
            u32(c, k_off_i);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_store);
            u32(c, k_align_i32);
            u32(c, k_off_sum);

            // i increment:
            // local.get sp; local.get sp; i32.load off_i; i32.const step; i32.add; i32.store off_i
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_load);
            u32(c, k_align_i32);
            u32(c, k_off_i);
            op(c, wasm_op::i32_const);
            i32(c, k_step);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_store);
            u32(c, k_align_i32);
            u32(c, k_off_i);

            // back-edge
            op(c, wasm_op::br);
            u32(c, 0u);

            // end loop + block
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            // return sum
            op(c, wasm_op::local_get);
            u32(c, k_sp_local);
            op(c, wasm_op::i32_load);
            u32(c, k_align_i32);
            u32(c, k_off_sum);

            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_i32_sum_loop_run_suite(runtime_module_t const& rt) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 45);
        return 0;
    }

    [[nodiscard]] int test_translate_i32_sum_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i32_sum_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_sum_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: bytecode should contain the mega-op when extra-heavy combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_loop_run =
                optable::translate::get_uwvmint_i32_sum_loop_run_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_loop_run));
#endif

            using Runner = interpreter_runner<opt>;
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 45);
        }

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_i32_sum_loop_run_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: semantics.
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
            UWVM2TEST_REQUIRE(run_i32_sum_loop_run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_i32_sum_loop_run();
}

