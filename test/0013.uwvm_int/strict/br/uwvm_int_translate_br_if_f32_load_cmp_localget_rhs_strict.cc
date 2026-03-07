#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

    [[nodiscard]] byte_vec build_br_if_f32_load_cmp_localget_rhs_module()
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

        auto memarg = [&](byte_vec& c, ::std::uint32_t align_pow2, ::std::uint32_t offset) -> void
        {
            u32(c, align_pow2);
            u32(c, offset);
        };

        auto add_case_local_plus_imm = [&](wasm_op cmp_op, ::std::int32_t taken, ::std::int32_t fall) -> void
        {
            // (param f32 rhs) (local i32 base) -> i32
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: base
            auto& c = fb.code;

            // base = 64
            op(c, wasm_op::i32_const);
            i32(c, 64);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            // Store 1.25 at mem[base + 16]
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_const);
            f32(c, 1.25f);
            op(c, wasm_op::f32_store);
            memarg(c, 2u, 0u);

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken);

            // if (f32.load(base+16) <cmp> rhs) return taken else fall
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load);
            memarg(c, 2u, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);  // rhs
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_case_localget_off = [&](wasm_op cmp_op, ::std::uint32_t offset, ::std::int32_t taken, ::std::int32_t fall) -> void
        {
            // (param f32 rhs) (local i32 base) -> i32
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: base
            auto& c = fb.code;

            // base = 128
            op(c, wasm_op::i32_const);
            i32(c, 128);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            // Store 1.25 at mem[base + offset] using store offset immediate
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_const);
            f32(c, 1.25f);
            op(c, wasm_op::f32_store);
            memarg(c, 2u, offset);

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken);

            // if (f32.load(base, offset) <cmp> rhs) return taken else fall
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_load);
            memarg(c, 2u, offset);

            op(c, wasm_op::local_get);
            u32(c, 0u);  // rhs
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0..f3: load_local_plus_imm + cmp(localget rhs) + br_if (expects mega-fuse variants in heavy combine).
        add_case_local_plus_imm(wasm_op::f32_lt, 111, 112);
        add_case_local_plus_imm(wasm_op::f32_gt, 121, 122);
        add_case_local_plus_imm(wasm_op::f32_le, 131, 132);
        add_case_local_plus_imm(wasm_op::f32_ge, 141, 142);

        // f4..f6: load_localget_off + cmp(localget rhs) + br_if (missing ge/gt/le variants in strict suite).
        add_case_localget_off(wasm_op::f32_gt, 24u, 211, 212);
        add_case_localget_off(wasm_op::f32_le, 28u, 221, 222);
        add_case_localget_off(wasm_op::f32_ge, 32u, 231, 232);

        return mb.build();
    }

    [[nodiscard]] byte_vec pack_f32(float v)
    {
        return ::uwvm2test::uwvm_int_strict::pack_f32(v);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 7uz);

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        [[maybe_unused]] native_memory_t const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        using Runner = interpreter_runner<Opt>;

        auto run1 = [&](::std::size_t fidx, float rhs) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(rhs),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        // f0: lt : 1.25 < 2.0 => taken
        UWVM2TEST_REQUIRE(run1(0, 2.0f) == 111);
        UWVM2TEST_REQUIRE(run1(0, 1.0f) == 112);

        // f1: gt : 1.25 > 1.0 => taken
        UWVM2TEST_REQUIRE(run1(1, 1.0f) == 121);
        UWVM2TEST_REQUIRE(run1(1, 2.0f) == 122);

        // f2: le : 1.25 <= 1.25 => taken
        UWVM2TEST_REQUIRE(run1(2, 1.25f) == 131);
        UWVM2TEST_REQUIRE(run1(2, 1.0f) == 132);

        // f3: ge : 1.25 >= 1.25 => taken
        UWVM2TEST_REQUIRE(run1(3, 1.25f) == 141);
        UWVM2TEST_REQUIRE(run1(3, 2.0f) == 142);

        // f4: gt : 1.25 > 1.0 => taken
        UWVM2TEST_REQUIRE(run1(4, 1.0f) == 211);
        UWVM2TEST_REQUIRE(run1(4, 2.0f) == 212);

        // f5: le : 1.25 <= 1.25 => taken
        UWVM2TEST_REQUIRE(run1(5, 1.25f) == 221);
        UWVM2TEST_REQUIRE(run1(5, 1.0f) == 222);

        // f6: ge : 1.25 >= 1.25 => taken
        UWVM2TEST_REQUIRE(run1(6, 1.25f) == 231);
        UWVM2TEST_REQUIRE(run1(6, 2.0f) == 232);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const exp_lp_lt = optable::translate::get_uwvmint_br_if_f32_load_local_plus_imm_lt_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);
            auto const exp_lp_gt = optable::translate::get_uwvmint_br_if_f32_load_local_plus_imm_gt_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);
            auto const exp_lp_le = optable::translate::get_uwvmint_br_if_f32_load_local_plus_imm_le_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);
            auto const exp_lp_ge = optable::translate::get_uwvmint_br_if_f32_load_local_plus_imm_ge_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);

            auto const exp_off_gt = optable::translate::get_uwvmint_br_if_f32_load_localget_off_gt_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);
            auto const exp_off_le = optable::translate::get_uwvmint_br_if_f32_load_localget_off_le_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);
            auto const exp_off_ge = optable::translate::get_uwvmint_br_if_f32_load_localget_off_ge_localget_rhs_fptr_from_tuple<Opt>(curr, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_lp_lt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_lp_gt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_lp_le));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_lp_ge));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_off_gt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_off_le));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_off_ge));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_f32_load_cmp_localget_rhs() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_f32_load_cmp_localget_rhs_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_f32_load_cmp_localget_rhs");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall mode (no stacktop caching): ensures memory/compare/br_if mega-fuse paths are reachable.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall + split stacktop caching: i32 and f32 disjoint to stress pop/push modeling around br_if.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 9uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_br_if_f32_load_cmp_localget_rhs();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
