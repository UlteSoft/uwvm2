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

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        if(r == 0u) { return x; }
        return (x << r) | (x >> ((32u - r) & 31u));
    }

    [[nodiscard]] constexpr ::std::uint64_t rotl64(::std::uint64_t x, ::std::uint64_t r) noexcept
    {
        r &= 63u;
        if(r == 0u) { return x; }
        return (x << r) | (x >> ((64u - r) & 63u));
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos < Opt.i32_stack_top_end_pos; ++pos)
            {
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i64_stack_top_begin_pos}; pos < Opt.i64_stack_top_end_pos; ++pos)
            {
                curr.i64_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    [[nodiscard]] byte_vec build_conbine_rot_xor_add_after_xor_constc_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: direct heavy fusion on `rot_xor_add_after_xor_constc` -> `i32_rot_xor_add`.
        {
            func_type ty{
                {k_val_i32, k_val_i32},
                {k_val_i32}
            };
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: direct heavy fusion on `rot_xor_add_i64_after_xor_constc` -> `i64_rot_xor_add`.
        {
            func_type ty{
                {k_val_i64, k_val_i64},
                {k_val_i64}
            };
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_const);
            i64(c, 5);
            op(c, wasm_op::i64_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::i64_const);
            i64(c, 11);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: pending flush fallback from `rot_xor_add_after_xor_constc` when next op is not `i32.add`.
        {
            func_type ty{
                {k_val_i32, k_val_i32},
                {k_val_i32}
            };
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 9);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_const);
            i32(c, 123);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: pending flush fallback from `rot_xor_add_i64_after_xor_constc` when next op is not `i64.add`.
        {
            func_type ty{
                {k_val_i64, k_val_i64},
                {k_val_i64}
            };
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_const);
            i64(c, 9);
            op(c, wasm_op::i64_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::i64_const);
            i64(c, 123);
            op(c, wasm_op::i64_sub);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const make_i32_rot_xor_add = [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_rot_xor_add_fptr_from_tuple<Opt>(curr_variant, tuple); };
            auto const make_i64_rot_xor_add = [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_rot_xor_add_fptr_from_tuple<Opt>(curr_variant, tuple); };

            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(cm.local_funcs.index_unchecked(0).op.operands, make_i32_rot_xor_add));
            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(cm.local_funcs.index_unchecked(1).op.operands, make_i64_rot_xor_add));
            UWVM2TEST_REQUIRE(!bytecode_contains_i32_variant<Opt>(cm.local_funcs.index_unchecked(2).op.operands, make_i32_rot_xor_add));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(cm.local_funcs.index_unchecked(3).op.operands, make_i64_rot_xor_add));
        }
#endif

        using Runner = interpreter_runner<Opt>;

        auto run_i32_2 = [&](::std::size_t fn_index, ::std::uint32_t x, ::std::uint32_t y) noexcept -> ::std::uint32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fn_index),
                                  rt.local_defined_function_vec_storage.index_unchecked(fn_index),
                                  pack_i32x2(static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)),
                                  nullptr,
                                  nullptr);
            return static_cast<::std::uint32_t>(load_i32(rr.results));
        };

        auto run_i64_2 = [&](::std::size_t fn_index, ::std::uint64_t x, ::std::uint64_t y) noexcept -> ::std::uint64_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fn_index),
                                  rt.local_defined_function_vec_storage.index_unchecked(fn_index),
                                  pack_i64x2(::std::bit_cast<::std::int64_t>(x), ::std::bit_cast<::std::int64_t>(y)),
                                  nullptr,
                                  nullptr);
            return static_cast<::std::uint64_t>(load_i64(rr.results));
        };

        for(auto const [x, y]: ::std::array{
                ::std::pair{0x01234567u, 0x89abcdefu},
                ::std::pair{0u,          0u         },
                ::std::pair{0xffffffffu, 0u         },
                ::std::pair{0u,          0xffffffffu},
                ::std::pair{0xdeadbeefu, 0x13579bdfu},
        })
        {
            UWVM2TEST_REQUIRE(run_i32_2(0uz, x, y) == static_cast<::std::uint32_t>((rotl32(x, 5u) ^ y) + 11u));
            UWVM2TEST_REQUIRE(run_i32_2(2uz, x, y) == static_cast<::std::uint32_t>((rotl32(x, 9u) ^ y) - 123u));
        }

        for(auto const [x, y]: ::std::array{
                ::std::pair{0x0123456789abcdefull, 0xfedcba9876543210ull},
                ::std::pair{0ull,                  0ull                 },
                ::std::pair{0xffffffffffffffffull, 0ull                 },
                ::std::pair{0ull,                  0xffffffffffffffffull},
                ::std::pair{0xdeadbeefcafebabeull, 0x13579bdf2468ace0ull},
        })
        {
            UWVM2TEST_REQUIRE(run_i64_2(1uz, x, y) == static_cast<::std::uint64_t>((rotl64(x, 5u) ^ y) + 11ull));
            UWVM2TEST_REQUIRE(run_i64_2(3uz, x, y) == static_cast<::std::uint64_t>((rotl64(x, 9u) ^ y) - 123ull));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_rot_xor_add_after_xor_constc() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_rot_xor_add_after_xor_constc_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_rot_xor_add_after_xor_constc");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_rot_xor_add_after_xor_constc();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
