#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_big_leb_indices_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: big local index (>127) LEB for local.get/set/tee.
        // (param i32) (result i32) (local i32 x 210)
        // local200 = param + 7; local201 = local200 xor 0x1234; return local201
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({210u, k_val_i32});
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 200u);

            op(c, wasm_op::local_get); u32(c, 200u);
            op(c, wasm_op::i32_const); i32(c, 0x1234);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee); u32(c, 201u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 201u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: big memarg offset (>127) LEB for i32.store/i32.load (offset=300).
        // (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // i32.store align=0 offset=300
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store); u32(c, 0u); u32(c, 300u);

            // i32.load align=0 offset=300
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load); u32(c, 0u); u32(c, 300u);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: big br_table vec length (>127) LEB.
        // Map selector -> {11,22,33,44,55} by cycling depths 0..4 across 130 entries.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);  // outer (result i32)

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b1 (depth 4 from inside b5)
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b2
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b3
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b4
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b5

            op(c, wasm_op::local_get);
            u32(c, 0u);

            op(c, wasm_op::br_table);
            u32(c, 130u);  // vec length (2-byte LEB)
            for(::std::uint32_t i = 0u; i < 130u; ++i)
            {
                u32(c, i % 5u);  // 0..4
            }
            u32(c, 4u);  // default => b1

            // end b5 (depth=0)
            op(c, wasm_op::end);
            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::br); u32(c, 4u);  // break to outer

            // end b4 (depth=1)
            op(c, wasm_op::end);
            op(c, wasm_op::i32_const); i32(c, 22);
            op(c, wasm_op::br); u32(c, 3u);

            // end b3 (depth=2)
            op(c, wasm_op::end);
            op(c, wasm_op::i32_const); i32(c, 33);
            op(c, wasm_op::br); u32(c, 2u);

            // end b2 (depth=3)
            op(c, wasm_op::end);
            op(c, wasm_op::i32_const); i32(c, 44);
            op(c, wasm_op::br); u32(c, 1u);

            // end b1 (depth=4 / default)
            op(c, wasm_op::end);
            op(c, wasm_op::i32_const); i32(c, 55);

            op(c, wasm_op::end);  // end outer
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_big_leb_indices_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: big local idx
        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            ::std::uint32_t const expected = (x + 7u) ^ 0x1234u;
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expected);
        }

        // f1: big memarg offset
        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x89abcdefu})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == x);
        }

        // f2: big br_table vec length
        auto run_sel = [&](::std::int32_t sel) noexcept -> ::std::int32_t
        {
            return load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                        rt.local_defined_function_vec_storage.index_unchecked(2),
                                        pack_i32(sel),
                                        nullptr,
                                        nullptr)
                                .results);
        };
        UWVM2TEST_REQUIRE(run_sel(0) == 11);
        UWVM2TEST_REQUIRE(run_sel(1) == 22);
        UWVM2TEST_REQUIRE(run_sel(2) == 33);
        UWVM2TEST_REQUIRE(run_sel(3) == 44);
        UWVM2TEST_REQUIRE(run_sel(4) == 55);
        UWVM2TEST_REQUIRE(run_sel(5) == 11);
        UWVM2TEST_REQUIRE(run_sel(129) == 55);
        UWVM2TEST_REQUIRE(run_sel(130) == 55);

        return 0;
    }

    [[nodiscard]] int test_translate_big_leb_indices() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_big_leb_indices_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_big_leb_indices");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref (tail-call off)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_big_leb_indices_suite<opt>(rt) == 0);
        }

        // tailcall (no cache)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_big_leb_indices_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching (merged scalar4)
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
            UWVM2TEST_REQUIRE(run_big_leb_indices_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_big_leb_indices();
}
