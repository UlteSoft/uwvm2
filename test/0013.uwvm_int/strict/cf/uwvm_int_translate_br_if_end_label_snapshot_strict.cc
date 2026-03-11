#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_if_end_label_snapshot_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32 cond, i32 x) -> (result i32)
        // Similar to the unconditional-`br` snapshot test, but uses `br_if 0` to reach the end label.
        // The fallthrough becomes unreachable via `unreachable`, so the end handler must restore the
        // stacktop model from the recorded end-label snapshot.
        //
        // return (x + 100) + x == 2*x + 100 (only for cond != 0)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // carried = x + 100
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 100);
            op(c, wasm_op::i32_add);

            // block (result i32)
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            // extra value to be dropped on the taken edge
            op(c, wasm_op::i32_const);
            i32(c, 0x1111);

            // block result candidate
            op(c, wasm_op::local_get);
            u32(c, 1u);

            // condition
            op(c, wasm_op::local_get);
            u32(c, 0u);

            // if (cond) break block with result
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // Fallthrough is unreachable (cond==0 path traps at runtime, but must translate).
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i32_const);
            i32(c, 0);

            // end block
            op(c, wasm_op::end);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_br_if_end_label_snapshot_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        for(::std::int32_t x : {0, 1, 7, -5, ::std::numeric_limits<::std::int32_t>::min()})
        {
            byte_vec params{};
            auto a = pack_i32(1);  // cond = 1 (take the branch; avoid executing `unreachable`)
            auto b = pack_i32(x);
            params.reserve(a.size() + b.size());
            append_bytes(params, a);
            append_bytes(params, b);

            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  params,
                                  nullptr,
                                  nullptr);

            auto const xu = static_cast<::std::uint32_t>(x);
            auto const expect = static_cast<::std::uint32_t>((xu * 2u) + 100u);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expect);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_end_label_snapshot() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_end_label_snapshot_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_end_label_snapshot");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_br_if_end_label_snapshot_suite<opt>(rt) == 0);
        }

        // tailcall (no stacktop caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_br_if_end_label_snapshot_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching (int/float merged rings)
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
            UWVM2TEST_REQUIRE(run_br_if_end_label_snapshot_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_if_end_label_snapshot();
}

