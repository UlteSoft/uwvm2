#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_convert_cross_ring_spill_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: () -> (i64)
        // i64.const 1 ; i32.const -2 ; i64.extend_i32_s ; i64.add  => -1
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, -2);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: () -> (i64)
        // i64.const 1 ; i32.const -1 ; i64.extend_i32_u ; i64.add  => 2^32
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, -1);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: () -> (i32)
        // i32.const 5 ; i64.const (2^32 + 7) ; i32.wrap_i64 ; i32.add  => 12
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i64_const);
            i64(c, 4294967303ll);  // 2^32 + 7
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_convert_cross_ring_spill_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr0.results) == -1);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr1.results)) == 0x100000000ull);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr2.results) == 12);

        return 0;
    }

    [[nodiscard]] int test_translate_convert_cross_ring_spill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_convert_cross_ring_spill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_convert_cross_ring_spill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // No caching (byref + tailcall).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_convert_cross_ring_spill_suite<opt>(rt) == 0);
        }
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_convert_cross_ring_spill_suite<opt>(rt) == 0);
        }

        // Disjoint rings + ring size 1: conversions must spill to free an insertion slot.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 4uz,  // 1 slot
                .i64_stack_top_begin_pos = 4uz,
                .i64_stack_top_end_pos = 5uz,  // 1 slot
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 6uz,  // 1 slot (required by wasm1 stacktop invariant)
                .f64_stack_top_begin_pos = 6uz,
                .f64_stack_top_end_pos = 7uz,  // 1 slot (required by wasm1 stacktop invariant)
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_convert_cross_ring_spill_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_convert_cross_ring_spill();
}
