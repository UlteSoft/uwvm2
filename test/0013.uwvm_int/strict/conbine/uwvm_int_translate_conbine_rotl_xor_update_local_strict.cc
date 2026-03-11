#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        if(r == 0u) { return x; }
        return (x << r) | (x >> ((32u - r) & 31u));
    }

    [[nodiscard]] byte_vec build_rotl_xor_update_local_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: y = y ^ rotl(x, 5) ; return y (local.set)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);  // y
            op(c, wasm_op::local_get);
            u32(c, 1u);  // x
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: y = y ^ rotl(x, 5) ; return y (local.tee)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);  // y
            op(c, wasm_op::local_get);
            u32(c, 1u);  // x
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
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

        using Runner = interpreter_runner<Opt>;

        for(auto const [y, x] : ::std::array{
                ::std::pair{0x01234567u, 0x89abcdefu},
                ::std::pair{0u, 0u},
                ::std::pair{0xffffffffu, 0u},
                ::std::pair{0u, 0xffffffffu},
                ::std::pair{0xdeadbeefu, 0x13579bdfu},
            })
        {
            auto const exp_u = y ^ rotl32(x, 5u);
            auto const exp = ::std::bit_cast<::std::int32_t>(exp_u);

            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i32x2(::std::bit_cast<::std::int32_t>(y), ::std::bit_cast<::std::int32_t>(x)),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == exp);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_i32x2(::std::bit_cast<::std::int32_t>(y), ::std::bit_cast<::std::int32_t>(x)),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == exp);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_rotl_xor_update_local() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_rotl_xor_update_local_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_rotl_xor_update_local");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: strict bytecode assertions (extra-heavy mega-op).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            auto const exp_set = optable::translate::get_uwvmint_i32_rotl_xor_local_set_fptr_from_tuple<opt>(curr, tuple);
            auto const exp_tee = optable::translate::get_uwvmint_i32_rotl_xor_local_tee_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_tee));
#endif

            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Byref mode: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_rotl_xor_update_local();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

