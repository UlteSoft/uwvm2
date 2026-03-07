#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_if_pop_from_memory_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i64 a, i64 b) -> (result i32)
        // block (result i32)
        //   i32.const taken
        //   local.get 0; local.get 1; i64.ne
        //   br_if 0
        //   drop
        //   i32.const fall
        // end
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        using Runner = interpreter_runner<Opt>;

        auto pack_i64x2 = [](::std::int64_t a, ::std::int64_t b) -> byte_vec
        {
            byte_vec out(16);
            ::std::memcpy(out.data(), ::std::addressof(a), 8);
            ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
            return out;
        };

        // a == b => (a != b) is false => fallthrough
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64x2(7, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        // a != b => branch taken
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64x2(7, 8),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_pop_from_memory() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_pop_from_memory_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_pop_from_memory");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall mode (no stacktop caching): semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching with a tiny fully-merged scalar ring (1 slot).
        // This forces `i64.ne` (net stack effect -1) to leave the i32 condition in operand stack memory
        // when it is immediately followed by `br_if`, so `br_if` must pop from memory.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                // Use a tiny fully-merged scalar ring (1 slot). This stresses the stack-top model and can force
                // the i32 condition to remain in operand stack memory at the br_if site.
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

            // Bytecode assertion: br_if must pop condition from operand stack memory when the cache is empty at the site.
            {
                ::uwvm2::validation::error::code_validation_error_impl err{};
                optable::compile_option cop{};
                auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
                UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

                constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
                constexpr auto tuple =
                    compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
                auto const exp = optable::translate::get_uwvmint_br_if_pop_from_memory_fptr_from_tuple<opt>(curr, tuple);
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp));
            }

            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_br_if_pop_from_memory();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
