#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using kind_t = optable::trivial_defined_call_kind;

    [[nodiscard]] byte_vec build_trivial_kinds_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // 0: nop_void -> end
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 1: const_i32 -> i32.const IMM ; end
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1234);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 2: param0_i32 -> local.get 0 ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 3: add_const_i32 -> local.get 0 ; i32.const IMM ; i32.add ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 4: xor_const_i32 -> local.get 0 ; i32.const IMM ; i32.xor ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x55);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 5: mul_const_i32 -> local.get 0 ; i32.const IMM ; i32.mul ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 3);
            op(fb.code, wasm_op::i32_mul);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 6: mul_add_const_i32 -> local.get 0 ; i32.const MUL ; i32.mul ; i32.const ADD ; i32.add ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 11);  // mul
            op(fb.code, wasm_op::i32_mul);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 13);  // add
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 7: rotr_add_const_i32 -> local.get 0 ; i32.const ROTR ; i32.rotr ; i32.const ADD ; i32.add ; end
        // Note: the matcher stores `ADD` in `imm` and `ROTR` in `imm2`.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 5);  // rotr amount
            op(fb.code, wasm_op::i32_rotr);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 17);  // add const
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 8: xor_i32 -> local.get 0 ; local.get 1 ; i32.xor ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 9: xor_add_const_i32 -> local.get 1 ; i32.const IMM ; i32.xor ; local.get 0 ; i32.add ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x6c);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 10: sub_or_const_i32 -> local.get 0 ; local.get 1 ; i32.const IMM ; i32.or ; i32.sub ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x0f0f);
            op(fb.code, wasm_op::i32_or);
            op(fb.code, wasm_op::i32_sub);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 11: sum8_xor_const_i32 (param i32x8 -> i32)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            // local.get 0 ; local.get 1 ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            // local.get 2 ; local.get 3 ; i32.add ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 3u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            // local.get 4 ; local.get 5 ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 4u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 5u);
            op(fb.code, wasm_op::i32_add);
            // local.get 6 ; local.get 7 ; i32.add ; i32.add ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 6u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 7u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            // i32.const IMM ; i32.xor ; end
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x13579bdf);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 12: xorshift32_i32 (param i32 -> i32) - exact canonical form.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            // local.get 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 13);
            op(fb.code, wasm_op::i32_shl);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 17);
            op(fb.code, wasm_op::i32_shr_u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 5);
            op(fb.code, wasm_op::i32_shl);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; end
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // Negative cases: code matches a trivial-kind pattern, but signature does not.
        // We use locals (no params) so the body stays valid, but the type-checking in translate.h must reject it.

        // 13: param0_i32 shape but (no params) -> (i32), using a local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 14: add_const_i32 shape but (no params) -> (i32), using a local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 15: xor_i32 shape but (no params) -> (i32), using local0/local1.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({2u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 16: sum8_xor_const_i32 shape but (no params) -> (i32), using local0..local7.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({8u, k_val_i32});
            // local.get 0 ; local.get 1 ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            // local.get 2 ; local.get 3 ; i32.add ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 3u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            // local.get 4 ; local.get 5 ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 4u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 5u);
            op(fb.code, wasm_op::i32_add);
            // local.get 6 ; local.get 7 ; i32.add ; i32.add ; i32.add ;
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 6u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 7u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::i32_add);
            // i32.const IMM ; i32.xor ; end
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x13579bdf);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 17: xorshift32_i32 shape but (no params) -> (i32), using local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            // local.get 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 13);
            op(fb.code, wasm_op::i32_shl);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 17);
            op(fb.code, wasm_op::i32_shr_u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 5);
            op(fb.code, wasm_op::i32_shl);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            // local.get 0 ; end
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 18: xor_i32 pattern D -> local.get 0/1 ; local.get 1/0 ; i32.xor ; local.get 2 ; i32.xor ; end
        // Covers the "extra local default-zero" trivial matcher path in translate.h.
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local index 2 (zero-initialized)
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // More negative cases: match a kind by byte pattern, but fail the signature preconditions used by call_info.

        // 19: xor_const_i32 shape but (no params) -> (i32), using local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x1234);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 20: mul_const_i32 shape but (no params) -> (i32), using local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::i32_mul);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 21: mul_add_const_i32 shape but (no params) -> (i32), using local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 11);  // mul
            op(fb.code, wasm_op::i32_mul);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 13);  // add
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 22: rotr_add_const_i32 shape but (no params) -> (i32), using local0.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 5);  // rotr amount
            op(fb.code, wasm_op::i32_rotr);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 17);  // add const
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 23: xor_add_const_i32 (pattern C') shape but (no params) -> (i32), using local0/local1.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({2u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x6c);
            op(fb.code, wasm_op::i32_xor);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 24: sub_or_const_i32 shape but (no params) -> (i32), using local0/local1.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({2u, k_val_i32});
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0x0f0f);
            op(fb.code, wasm_op::i32_or);
            op(fb.code, wasm_op::i32_sub);
            op(fb.code, wasm_op::end);
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

        UWVM2TEST_REQUIRE(cm.local_defined_call_info.size() == 25uz);

        auto match_kind = [&](::std::size_t idx) noexcept -> kind_t
        {
            return compiler::details::match_trivial_call_inline_body(rt.local_defined_function_vec_storage.index_unchecked(idx).wasm_code_ptr).kind;
        };

        auto expect = [&](::std::size_t idx, kind_t k, ::std::int32_t imm, ::std::int32_t imm2) noexcept -> int
        {
            auto const& info = cm.local_defined_call_info.index_unchecked(idx);
            UWVM2TEST_REQUIRE(info.trivial_kind == k);
            UWVM2TEST_REQUIRE(static_cast<::std::int32_t>(info.trivial_imm) == imm);
            UWVM2TEST_REQUIRE(static_cast<::std::int32_t>(info.trivial_imm2) == imm2);
            return 0;
        };

        UWVM2TEST_REQUIRE(expect(0uz, kind_t::nop_void, 0, 0) == 0);
        UWVM2TEST_REQUIRE(expect(1uz, kind_t::const_i32, 1234, 0) == 0);
        UWVM2TEST_REQUIRE(expect(2uz, kind_t::param0_i32, 0, 0) == 0);
        UWVM2TEST_REQUIRE(expect(3uz, kind_t::add_const_i32, 7, 0) == 0);
        UWVM2TEST_REQUIRE(expect(4uz, kind_t::xor_const_i32, 0x55, 0) == 0);
        UWVM2TEST_REQUIRE(expect(5uz, kind_t::mul_const_i32, 3, 0) == 0);
        UWVM2TEST_REQUIRE(expect(6uz, kind_t::mul_add_const_i32, 11, 13) == 0);
        UWVM2TEST_REQUIRE(expect(7uz, kind_t::rotr_add_const_i32, 17, 5) == 0);
        UWVM2TEST_REQUIRE(expect(8uz, kind_t::xor_i32, 0, 0) == 0);
        UWVM2TEST_REQUIRE(expect(9uz, kind_t::xor_add_const_i32, 0x6c, 0) == 0);
        UWVM2TEST_REQUIRE(expect(10uz, kind_t::sub_or_const_i32, 0x0f0f, 0) == 0);
        UWVM2TEST_REQUIRE(expect(11uz, kind_t::sum8_xor_const_i32, 0x13579bdf, 0) == 0);
        UWVM2TEST_REQUIRE(expect(12uz, kind_t::xorshift32_i32, 0, 0) == 0);
        UWVM2TEST_REQUIRE(expect(18uz, kind_t::xor_i32, 0, 0) == 0);

        auto expect_none = [&](::std::size_t idx) noexcept -> int
        {
            auto const& info = cm.local_defined_call_info.index_unchecked(idx);
            UWVM2TEST_REQUIRE(info.trivial_kind == kind_t::none);
            return 0;
        };

        // Negative cases: signature mismatch => trivial kind must not be set.
        UWVM2TEST_REQUIRE(expect_none(13uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(14uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(15uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(16uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(17uz) == 0);

        // Ensure the matcher still recognizes these patterns (byte-shape match), but call_info rejects them.
        UWVM2TEST_REQUIRE(match_kind(13uz) == kind_t::param0_i32);
        UWVM2TEST_REQUIRE(match_kind(14uz) == kind_t::add_const_i32);
        UWVM2TEST_REQUIRE(match_kind(15uz) == kind_t::xor_i32);
        UWVM2TEST_REQUIRE(match_kind(16uz) == kind_t::sum8_xor_const_i32);
        UWVM2TEST_REQUIRE(match_kind(17uz) == kind_t::xorshift32_i32);

        UWVM2TEST_REQUIRE(expect_none(19uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(20uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(21uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(22uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(23uz) == 0);
        UWVM2TEST_REQUIRE(expect_none(24uz) == 0);

        UWVM2TEST_REQUIRE(match_kind(19uz) == kind_t::xor_const_i32);
        UWVM2TEST_REQUIRE(match_kind(20uz) == kind_t::mul_const_i32);
        UWVM2TEST_REQUIRE(match_kind(21uz) == kind_t::mul_add_const_i32);
        UWVM2TEST_REQUIRE(match_kind(22uz) == kind_t::rotr_add_const_i32);
        UWVM2TEST_REQUIRE(match_kind(23uz) == kind_t::xor_add_const_i32);
        UWVM2TEST_REQUIRE(match_kind(24uz) == kind_t::sub_or_const_i32);

        // Cover overlong LEB error paths in the trivial matcher (non-wasm-valid byte streams).
        {
            using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
            using wasm_code_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t;

            constexpr wasm_byte bad_u32_leb[] = {
                static_cast<wasm_byte>(u8(wasm_op::local_get)),
                0x80u,
                0x80u,
                0x80u,
                0x80u,
                0x80u,
                static_cast<wasm_byte>(u8(wasm_op::end)),
            };

            wasm_code_t code{};
            code.body.expr_begin = bad_u32_leb;
            code.body.code_end = bad_u32_leb + (sizeof(bad_u32_leb) / sizeof(bad_u32_leb[0]));
            UWVM2TEST_REQUIRE(compiler::details::match_trivial_call_inline_body(&code).kind == kind_t::none);
        }

        {
            using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
            using wasm_code_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t;

            constexpr wasm_byte bad_i32_leb[] = {
                static_cast<wasm_byte>(u8(wasm_op::i32_const)),
                0x80u,
                0x80u,
                0x80u,
                0x80u,
                0x80u,
                static_cast<wasm_byte>(u8(wasm_op::end)),
            };

            wasm_code_t code{};
            code.body.expr_begin = bad_i32_leb;
            code.body.code_end = bad_i32_leb + (sizeof(bad_i32_leb) / sizeof(bad_i32_leb[0]));
            UWVM2TEST_REQUIRE(compiler::details::match_trivial_call_inline_body(&code).kind == kind_t::none);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_trivial_call_kinds() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_trivial_kinds_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_trivial_call_kinds");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_trivial_call_kinds();
}
