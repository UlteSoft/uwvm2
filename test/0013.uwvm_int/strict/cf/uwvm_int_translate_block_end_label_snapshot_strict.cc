#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_block_end_label_snapshot_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32) -> (result i32)
        // Carry one value across a block. Inside the block we `br 0` to the end label (with a block result),
        // and keep the fallthrough path unreachable. This exercises the stacktop end-label snapshot/restore logic
        // when stacktop caching is enabled.
        //
        // return (x + 1) + 7 == x + 8
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // carried = x + 1
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);

            // block (result i32)
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            // Push an extra value that must be dropped by the branch.
            op(c, wasm_op::i32_const);
            i32(c, 0x1234);

            // Push the block result and branch to end label.
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::br);
            u32(c, 0u);

            // Unreachable fallthrough: still must validate/translate.
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i32_const);
            i32(c, 9);

            // end block
            op(c, wasm_op::end);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_block_end_label_snapshot_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        for(::std::int32_t x : {0, 1, 34, -1, ::std::numeric_limits<::std::int32_t>::min()})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(x),
                                  nullptr,
                                  nullptr);
            auto const expect_u32 = static_cast<::std::uint32_t>(static_cast<::std::uint32_t>(x) + 8u);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expect_u32);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_block_end_label_snapshot() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_block_end_label_snapshot_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_block_end_label_snapshot");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_block_end_label_snapshot_suite<opt>(rt) == 0);
        }

        // tailcall (no stacktop caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_block_end_label_snapshot_suite<opt>(rt) == 0);
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
            UWVM2TEST_REQUIRE(run_block_end_label_snapshot_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_block_end_label_snapshot();
}

