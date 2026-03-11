#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_if_else_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: if-else (result i32) with different stacktop activity in then/else.
        // (param i32) (result i32) -> cond?6:7
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);
            // then: (1+2+3)=6 (push 3 to exercise spill/fill paths under small rings)
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::else_);
            // else: 7
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: if without else + then-branch exits outer block (tests stacktop state restore at `end`).
        // (param i32) (result i32) -> cond?111:222
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);  // outer block provides function result

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_block_empty);
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::br);
            u32(c, 1u);            // break out of outer block with 111
            op(c, wasm_op::end);   // end if

            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::end);   // end outer block
            op(c, wasm_op::end);   // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: if-else (result i32) where else-path returns early (else unreachable at if-end).
        // (param i32) (result i32) -> cond?123:456
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 123);
            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const);
            i32(c, 456);
            op(c, wasm_op::return_);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: if-else (result i32) where then-path returns early (then unreachable at if-end).
        // (param i32) (result i32) -> cond?11:22
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::return_);
            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const);
            i32(c, 22);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_if_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 6);

        // f1
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        // f2
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 456);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 123);

        // f3
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 22);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 11);

        return 0;
    }

    [[nodiscard]] int test_translate_if_else() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_if_else_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_if_else");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_if_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_if_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (exercise if/else stacktop merge logic).
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
            UWVM2TEST_REQUIRE(run_if_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_if_else();
}

