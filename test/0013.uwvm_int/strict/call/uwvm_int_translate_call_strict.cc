#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

    [[nodiscard]] inline ::std::uint32_t load_u32(::std::byte const* p) noexcept
    {
        ::std::uint32_t v{};
        ::std::memcpy(::std::addressof(v), p, sizeof(v));
        return v;
    }

    inline void store_u32(::std::byte* p, ::std::uint32_t v) noexcept
    {
        ::std::memcpy(p, ::std::addressof(v), sizeof(v));
    }

    // Minimal local-call bridge for tests:
    // - compiler encodes local-defined calls as (module_id=SIZE_MAX, call_function=ptr_to_compiled_defined_call_info)
    // - we only implement the "trivial defined call" fast paths used by the compiler matcher
    static void call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        using kind_t = optable::trivial_defined_call_kind;
        using info_t = optable::compiled_defined_call_info;

        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        if(wasm_module_id != SIZE_MAX) [[unlikely]]
        {
            // This test harness only supports local-defined calls.
            ::fast_io::fast_terminate();
        }

        auto const* const info = reinterpret_cast<info_t const*>(call_function);
        if(info == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        if(info->trivial_kind == kind_t::none) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto* const top = *stack_top_ptr;
        auto const top_addr = reinterpret_cast<::std::uintptr_t>(top);
        if(top_addr < info->param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::byte* const base = reinterpret_cast<::std::byte*>(top_addr - info->param_bytes);

        ::std::uint_least32_t const imm_u32{::std::bit_cast<::std::uint_least32_t>(info->trivial_imm)};
        ::std::uint_least32_t const imm2_u32{::std::bit_cast<::std::uint_least32_t>(info->trivial_imm2)};

        auto finish_void = [&]() noexcept
        {
            *stack_top_ptr = base;
        };

        auto finish_i32 = [&](::std::uint32_t v) noexcept
        {
            store_u32(base, v);
            *stack_top_ptr = base + 4;
        };

        switch(info->trivial_kind)
        {
            case kind_t::nop_void:
            {
                finish_void();
                break;
            }
            case kind_t::const_i32:
            {
                finish_i32(static_cast<::std::uint32_t>(imm_u32));
                break;
            }
            case kind_t::param0_i32:
            {
                // Return the first parameter (ignore the rest).
                finish_i32(load_u32(base));
                break;
            }
            case kind_t::add_const_i32:
            {
                auto const a = load_u32(base);
                finish_i32(static_cast<::std::uint32_t>(a + imm_u32));
                break;
            }
            case kind_t::xor_const_i32:
            {
                auto const a = load_u32(base);
                finish_i32(static_cast<::std::uint32_t>(a ^ imm_u32));
                break;
            }
            case kind_t::mul_const_i32:
            {
                auto const a = load_u32(base);
                finish_i32(static_cast<::std::uint32_t>(a * imm_u32));
                break;
            }
            case kind_t::mul_add_const_i32:
            {
                auto const a = load_u32(base);
                finish_i32(static_cast<::std::uint32_t>((a * imm_u32) + imm2_u32));
                break;
            }
            case kind_t::rotr_add_const_i32:
            {
                auto const a = load_u32(base);
                auto const rot = static_cast<int>(imm2_u32 & 31u);
                auto const r = ::std::rotr(static_cast<::std::uint32_t>(a), rot);
                finish_i32(static_cast<::std::uint32_t>(r + imm_u32));
                break;
            }
            case kind_t::xorshift32_i32:
            {
                ::std::uint32_t x = load_u32(base);
                x ^= (x << 13);
                x ^= (x >> 17);
                x ^= (x << 5);
                finish_i32(x);
                break;
            }
            case kind_t::xor_i32:
            {
                auto const a = load_u32(base);
                auto const b = load_u32(base + 4);
                finish_i32(static_cast<::std::uint32_t>(a ^ b));
                break;
            }
            case kind_t::xor_add_const_i32:
            {
                auto const a = load_u32(base);
                auto const b = load_u32(base + 4);
                finish_i32(static_cast<::std::uint32_t>(a + (b ^ imm_u32)));
                break;
            }
            case kind_t::sub_or_const_i32:
            {
                auto const a = load_u32(base);
                auto const b = load_u32(base + 4);
                finish_i32(static_cast<::std::uint32_t>(a - (b | imm_u32)));
                break;
            }
            case kind_t::sum8_xor_const_i32:
            {
                ::std::uint32_t sum{};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    sum += load_u32(base + (i * 4));
                }
                finish_i32(static_cast<::std::uint32_t>(sum ^ imm_u32));
                break;
            }
            [[unlikely]] default:
            {
                ::fast_io::fast_terminate();
            }
        }
    }

    [[nodiscard]] byte_vec build_call_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // 0: const_i32 => 123
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 123);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 1: add_const_i32 => x+7
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 2: mul_add_const_i32 => x*3+5
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 3: xor_i32 => a^b
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 4: xor_add_const_i32 => a + (b ^ 0x55)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 0x55);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 5: sub_or_const_i32 => a - (b | 0x80)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 0x80);
            op(c, wasm_op::i32_or);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 6: param0_i32 (3 params) => param0
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 7: xor_const_i32 => x ^ 0xff00ff00
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(0xff00ff00u));
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 8: mul_const_i32 => x * 9
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 9);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 9: rotr_add_const_i32 => rotr(x, 5) + 7
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotr);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 10: xorshift32_i32 (canonical form)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // local.get 0
            op(c, wasm_op::local_get);
            u32(c, 0u);

            // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 13);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 17);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // local.get 0 ; end
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 11: nop_void
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 12: sum8_xor_const_i32 => (a0+...+a7) ^ 0x1234
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::local_get);
            u32(c, 4u);
            op(c, wasm_op::local_get);
            u32(c, 5u);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::local_get);
            u32(c, 6u);
            op(c, wasm_op::local_get);
            u32(c, 7u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::i32_const);
            i32(c, 0x1234);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 13: caller_call_add => add_const(5) == 12
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::call);
            u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 14: caller_call_add_drop => drop(add_const(5)); return 77
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::call);
            u32(c, 1u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 77);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 15: caller_call_add_local_set => local0 = add_const(6); return local0
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 6);
            op(c, wasm_op::call);
            u32(c, 1u);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 16: caller_call_add_local_tee => return (local0 = add_const(1))
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::call);
            u32(c, 1u);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 17: caller_call_xor_add => xor_add_const(3, 4) == 84
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_const);
            i32(c, 4);
            op(c, wasm_op::call);
            u32(c, 4u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 18: caller_call_param0_3p => param0_i32(1,2,3) == 1
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::call);
            u32(c, 6u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 19: caller_call_sum8 => sum8_xor_const(1..8) == 4624
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            for(int v = 1; v <= 8; ++v)
            {
                op(c, wasm_op::i32_const);
                i32(c, v);
            }
            op(c, wasm_op::call);
            u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 20: caller_call_nop_void => 1+2 (call nop_void in between)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::call);
            u32(c, 11u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool contains_any_stacktop_call_fptr(ByteStorage const& bc, Fptr f3, Fptr f4, Fptr f5, Fptr f6) noexcept
    {
        return bytecode_contains_fptr(bc, f3) || bytecode_contains_fptr(bc, f4) || bytecode_contains_fptr(bc, f5) || bytecode_contains_fptr(bc, f6);
    }

    [[nodiscard]] int test_translate_call() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +call_bridge;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_call_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: tailcall, no stacktop caching. Verify call+drop/local_set/local_tee fusion opfuncs.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            // Trivial matcher sanity.
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(0).trivial_kind == optable::trivial_defined_call_kind::const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(0).trivial_imm == static_cast<wasm_i32>(123));
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(1).trivial_kind == optable::trivial_defined_call_kind::add_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(1).trivial_imm == static_cast<wasm_i32>(7));
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(2).trivial_kind == optable::trivial_defined_call_kind::mul_add_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(2).trivial_imm == static_cast<wasm_i32>(3));
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(2).trivial_imm2 == static_cast<wasm_i32>(5));
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(3).trivial_kind == optable::trivial_defined_call_kind::xor_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(4).trivial_kind == optable::trivial_defined_call_kind::xor_add_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(5).trivial_kind == optable::trivial_defined_call_kind::sub_or_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(6).trivial_kind == optable::trivial_defined_call_kind::param0_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(7).trivial_kind == optable::trivial_defined_call_kind::xor_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(8).trivial_kind == optable::trivial_defined_call_kind::mul_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(9).trivial_kind == optable::trivial_defined_call_kind::rotr_add_const_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(10).trivial_kind == optable::trivial_defined_call_kind::xorshift32_i32);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(11).trivial_kind == optable::trivial_defined_call_kind::nop_void);
            UWVM2TEST_REQUIRE(cm.local_defined_call_info.index_unchecked(12).trivial_kind == optable::trivial_defined_call_kind::sum8_xor_const_i32);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_call_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
            constexpr auto exp_call_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
            constexpr auto exp_call_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<opt, wasm_i32>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_call_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(15).op.operands, exp_call_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp_call_ltee));
#endif

            using Runner = interpreter_runner<opt>;

            auto rr13 = Runner::run(cm.local_funcs.index_unchecked(13),
                                    rt.local_defined_function_vec_storage.index_unchecked(13),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr13.results) == 12);

            auto rr14 = Runner::run(cm.local_funcs.index_unchecked(14),
                                    rt.local_defined_function_vec_storage.index_unchecked(14),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr14.results) == 77);

            auto rr15 = Runner::run(cm.local_funcs.index_unchecked(15),
                                    rt.local_defined_function_vec_storage.index_unchecked(15),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr15.results) == 13);

            auto rr16 = Runner::run(cm.local_funcs.index_unchecked(16),
                                    rt.local_defined_function_vec_storage.index_unchecked(16),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr16.results) == 8);

            auto rr17 = Runner::run(cm.local_funcs.index_unchecked(17),
                                    rt.local_defined_function_vec_storage.index_unchecked(17),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr17.results) == 84);

            auto rr18 = Runner::run(cm.local_funcs.index_unchecked(18),
                                    rt.local_defined_function_vec_storage.index_unchecked(18),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr18.results) == 1);

            auto rr19 = Runner::run(cm.local_funcs.index_unchecked(19),
                                    rt.local_defined_function_vec_storage.index_unchecked(19),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr19.results) == 4624);

            auto rr20 = Runner::run(cm.local_funcs.index_unchecked(20),
                                    rt.local_defined_function_vec_storage.index_unchecked(20),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr20.results) == 3);
        }

        // Mode C: tailcall + i32 stacktop cache. Verify stacktop call fast paths are actually emitted.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                // Fully merged scalar4 range (required by wasm1.h for mixed-typed operand stack correctness).
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr optable::uwvm_interpreter_stacktop_currpos_t c3{
                .i32_stack_top_curr_pos = 3uz,
                .i64_stack_top_curr_pos = 3uz,
                .f32_stack_top_curr_pos = 3uz,
                .f64_stack_top_curr_pos = 3uz,
                .v128_stack_top_curr_pos = SIZE_MAX,
            };
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c4{.i32_stack_top_curr_pos = 4uz,
                                                                      .i64_stack_top_curr_pos = 4uz,
                                                                      .f32_stack_top_curr_pos = 4uz,
                                                                      .f64_stack_top_curr_pos = 4uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c5{.i32_stack_top_curr_pos = 5uz,
                                                                      .i64_stack_top_curr_pos = 5uz,
                                                                      .f32_stack_top_curr_pos = 5uz,
                                                                      .f64_stack_top_curr_pos = 5uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c6{.i32_stack_top_curr_pos = 6uz,
                                                                      .i64_stack_top_curr_pos = 6uz,
                                                                      .f32_stack_top_curr_pos = 6uz,
                                                                      .f64_stack_top_curr_pos = 6uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};

            // caller_call_add: call_stacktop_i32 ParamCount=1 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_call_fptr(cm.local_funcs.index_unchecked(13).op.operands, f3, f4, f5, f6));
            }

            // caller_call_add_drop: call_stacktop_i32_drop ParamCount=1
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_call_fptr(cm.local_funcs.index_unchecked(14).op.operands, f3, f4, f5, f6));
            }

            // caller_call_add_local_set: call_stacktop_i32_local_set ParamCount=1
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_call_fptr(cm.local_funcs.index_unchecked(15).op.operands, f3, f4, f5, f6));
            }

            // caller_call_param0_3p: call_stacktop_i32 ParamCount=3 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_call_fptr(cm.local_funcs.index_unchecked(18).op.operands, f3, f4, f5, f6));
            }
#endif

            using Runner = interpreter_runner<opt>;

            auto rr13 = Runner::run(cm.local_funcs.index_unchecked(13),
                                    rt.local_defined_function_vec_storage.index_unchecked(13),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr13.results) == 12);

            auto rr14 = Runner::run(cm.local_funcs.index_unchecked(14),
                                    rt.local_defined_function_vec_storage.index_unchecked(14),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr14.results) == 77);

            auto rr15 = Runner::run(cm.local_funcs.index_unchecked(15),
                                    rt.local_defined_function_vec_storage.index_unchecked(15),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr15.results) == 13);

            auto rr18 = Runner::run(cm.local_funcs.index_unchecked(18),
                                    rt.local_defined_function_vec_storage.index_unchecked(18),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr18.results) == 1);

            auto rr19 = Runner::run(cm.local_funcs.index_unchecked(19),
                                    rt.local_defined_function_vec_storage.index_unchecked(19),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr19.results) == 4624);

            auto rr20 = Runner::run(cm.local_funcs.index_unchecked(20),
                                    rt.local_defined_function_vec_storage.index_unchecked(20),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr20.results) == 3);
        }

        // Mode B: byref semantics (no stacktop caching).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_call_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
            constexpr auto exp_call_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
            constexpr auto exp_call_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<opt, wasm_i32>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_call_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(15).op.operands, exp_call_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp_call_ltee));
#endif

            using Runner = interpreter_runner<opt>;

            auto rr13 = Runner::run(cm.local_funcs.index_unchecked(13),
                                    rt.local_defined_function_vec_storage.index_unchecked(13),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr13.results) == 12);

            auto rr14 = Runner::run(cm.local_funcs.index_unchecked(14),
                                    rt.local_defined_function_vec_storage.index_unchecked(14),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr14.results) == 77);

            auto rr15 = Runner::run(cm.local_funcs.index_unchecked(15),
                                    rt.local_defined_function_vec_storage.index_unchecked(15),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr15.results) == 13);

            auto rr16 = Runner::run(cm.local_funcs.index_unchecked(16),
                                    rt.local_defined_function_vec_storage.index_unchecked(16),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr16.results) == 8);

            auto rr20 = Runner::run(cm.local_funcs.index_unchecked(20),
                                    rt.local_defined_function_vec_storage.index_unchecked(20),
                                    pack_no_params(),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr20.results) == 3);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call();
}
