#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_f64x1(double x)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(x), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_delay_local_f64_mul_localget_rhs_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: store const to memory; load it as lhs; then delay-local fuse rhs local.get into f64.mul.
        // (param f64 rhs) -> f64
        //   i32.const 0; f64.const 2.5; f64.store
        //   i32.const 0; f64.load
        //   local.get0; f64.mul
        {
            constexpr ::std::uint32_t k_align_f64 = 3u;  // align=8 bytes
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::f64_const);
            f64(c, 2.5);
            op(c, wasm_op::f64_store);
            u32(c, k_align_f64);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::f64_load);
            u32(c, k_align_f64);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);
        using Runner = interpreter_runner<Opt>;

        for(double const rhs : {0.0, 1.0, 3.0, -2.0, 10.0})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_f64x1(rhs),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == (2.5 * rhs));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_delay_local_f64_mul_localget_rhs() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_f64_mul_localget_rhs_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_f64_mul_localget_rhs");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall: strict bytecode assertions for delay-local rhs-localget fusion.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            auto const exp = optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                opt,
                optable::numeric_details::float_binop::mul>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp));
#endif

            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        // Byref: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE((run_suite<opt>(cm, rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_delay_local_f64_mul_localget_rhs();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

