#include "../uwvm_int_translate_strict_common.h"

#include <string>
#include <string_view>

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] std::string read_text_file(char const* path)
    {
        ::std::FILE* fp{::std::fopen(path, "rb")};
        if(fp == nullptr) { return {}; }

        if(::std::fseek(fp, 0, SEEK_END) != 0)
        {
            ::std::fclose(fp);
            return {};
        }

        long const sz{::std::ftell(fp)};
        if(sz < 0)
        {
            ::std::fclose(fp);
            return {};
        }

        if(::std::fseek(fp, 0, SEEK_SET) != 0)
        {
            ::std::fclose(fp);
            return {};
        }

        std::string text(static_cast<::std::size_t>(sz), '\0');
        if(sz != 0)
        {
            auto const nread{::std::fread(text.data(), 1uz, static_cast<::std::size_t>(sz), fp)};
            text.resize(nread);
        }

        ::std::fclose(fp);
        return text;
    }

    [[nodiscard]] bool log_contains_kind(std::string_view log_text, std::string_view kind) noexcept
    {
        std::string needle;
        needle.reserve(kind.size() + 5uz);
        needle.append("kind=");
        needle.append(kind.data(), kind.size());
        return log_text.find(needle) != std::string_view::npos;
    }

    [[nodiscard]] byte_vec build_runtime_log_conbine_kinds_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: local_get
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local_get2 (extra-heavy pending window)
        // local.get 0 ; local.get 1 ; drop ; end  => return param0
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local_get_i32_localget (store-provider pending kind) + i32.store
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_store);
            u32(fb.code, 2u);  // align=4
            u32(fb.code, 0u);  // offset=0
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local_get_const_i32
        // local.get 0 ; i32.const 7 ; drop ; end  => return param0
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 7);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local_get_const_i64
        // local.get 0 ; i64.const 1 ; i64.add ; end
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 1);
            op(fb.code, wasm_op::i64_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local_get_eqz_i32
        // local.get 0 ; i32.eqz ; drop ; end
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_eqz);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local_get2_const_i32_mul -> i32_add_mul_imm_2localget
        // local.get 0 ; local.get 1 ; i32.const 3 ; i32.mul ; i32.add ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 3);
            op(fb.code, wasm_op::i32_mul);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: local_get2_const_i32_shl -> i32_add_shl_imm_2localget
        // local.get 0 ; local.get 1 ; i32.const 1 ; i32.shl ; i32.add ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::i32_shl);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: i32_add_2localget_local_set
        // local.get 0 ; local.get 1 ; i32.add ; local.set 0 ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: i32_add_2localget_local_tee
        // local.get 0 ; local.get 1 ; i32.add ; local.tee 0 ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: i32_add_imm_local_settee_same
        // local.get 0 ; i32.const 5 ; i32.add ; local.tee 0 ; end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 5);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64_add_2localget_local_set
        {
            func_type ty{{k_val_i64, k_val_i64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i64_add);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64_add_2localget_local_tee
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i64_add);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64_add_imm_local_settee_same
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 5);
            op(fb.code, wasm_op::i64_add);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64_mul_imm_local_settee_same
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 3);
            op(fb.code, wasm_op::i64_mul);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64_rotr_imm_local_settee_same
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 7);
            op(fb.code, wasm_op::i64_rotr);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        auto add_cmp_brif = [&](wasm_op cmp_op, ::std::int32_t imm)
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, imm);
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::end);  // end func
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f11..f15: local_get_const_i32_cmp_brif + brif_cmp kinds
        add_cmp_brif(wasm_op::i32_eq, 0);
        add_cmp_brif(wasm_op::i32_lt_s, -1);
        add_cmp_brif(wasm_op::i32_lt_u, 1);
        add_cmp_brif(wasm_op::i32_ge_s, 2);
        add_cmp_brif(wasm_op::i32_ge_u, 3);

        // f16: i32.lt_s bool-normalize peephole:
        // local.get 0 ; local.get 1 ; i32.lt_s ; i32.const 1 ; i32.and ; i32.eqz ; br_if 0
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_lt_s);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::end);  // end func
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f17: local_get_const_i32_add + local_get_const_i32_add_localget + i32.store local_plus_imm
        // local.get 0 ; i32.const 16 ; i32.add ; local.get 1 ; i32.store ; end
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 16);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_store);
            u32(c, 2u);  // align=4
            u32(c, 0u);  // offset=0
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f18: local_get_const_f32 + f32_add_imm_local_settee_same
        {
            func_type ty{{k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f19: local_get_const_f32 + f32_mul_imm_local_settee_same
        {
            func_type ty{{k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f20: local_get_const_f32 + f32_sub_imm_local_settee_same
        {
            func_type ty{{k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 3.0f);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f21: local_get_const_f64 + f64_add_imm_local_settee_same
        {
            func_type ty{{k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f22: local_get_const_f64 + f64_mul_imm_local_settee_same
        {
            func_type ty{{k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f23: local_get_const_f64 + f64_sub_imm_local_settee_same
        {
            func_type ty{{k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 3.0);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f24: f32_acc_add_floor_localget_wait_add -> f32_acc_add_floor_localget_set_acc
        // local.get acc ; local.get x ; f32.floor ; f32.add ; local.set acc ; end
        {
            func_type ty{{k_val_f32, k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_floor);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f25: f64_acc_add_abs_localget_wait_add -> f64_acc_add_abs_localget_set_acc
        // local.get acc ; local.get x ; f64.abs ; f64.add ; local.set acc ; end
        {
            func_type ty{{k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_abs);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f26: f32_acc_add_negabs_localget_wait_const/cpysign/add -> set_acc
        // local.get acc ; local.get x ; f32.const -1.0 ; f32.copysign ; f32.add ; local.set acc ; end
        {
            func_type ty{{k_val_f32, k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_const);
            f32(c, -1.0f);
            op(c, wasm_op::f32_copysign);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f27: f64_acc_add_negabs_localget_wait_const/cpysign/add -> set_acc
        // local.get acc ; local.get x ; f64.const -1.0 ; f64.copysign ; f64.add ; local.set acc ; end
        {
            func_type ty{{k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_const);
            f64(c, -1.0);
            op(c, wasm_op::f64_copysign);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // common i32 update_local pending kinds: sub/mul/and/or/xor/shl/shr_s/shr_u/rotl/rotr
        auto add_i32_update_local_set = [&](wasm_op binop, ::std::int32_t imm)
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_i32_update_local_set(wasm_op::i32_sub, 5);
        add_i32_update_local_set(wasm_op::i32_mul, 3);
        add_i32_update_local_set(wasm_op::i32_and, static_cast<::std::int32_t>(0x00ff00ffu));
        add_i32_update_local_set(wasm_op::i32_or, static_cast<::std::int32_t>(0x0f0f0f0fu));
        add_i32_update_local_set(wasm_op::i32_xor, static_cast<::std::int32_t>(0x55aa55aau));
        add_i32_update_local_set(wasm_op::i32_shl, 5);
        add_i32_update_local_set(wasm_op::i32_shr_s, 13);
        add_i32_update_local_set(wasm_op::i32_shr_u, 9);
        add_i32_update_local_set(wasm_op::i32_rotl, 11);
        add_i32_update_local_set(wasm_op::i32_rotr, 7);

        // common i64 update_local pending kinds: add/sub/mul/and/or/xor/shl/shr_s/shr_u/rotl/rotr
        auto add_i64_update_local_set = [&](wasm_op binop, ::std::int64_t imm)
        {
            func_type ty{{k_val_i64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_const);
            i64(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_i64_update_local_set(wasm_op::i64_add, 5);
        add_i64_update_local_set(wasm_op::i64_sub, 7);
        add_i64_update_local_set(wasm_op::i64_mul, 3);
        add_i64_update_local_set(wasm_op::i64_and, static_cast<::std::int64_t>(0x00ff00ff00ff00ffull));
        add_i64_update_local_set(wasm_op::i64_or, static_cast<::std::int64_t>(0x0f0f0f0f0f0f0f0full));
        add_i64_update_local_set(wasm_op::i64_xor, static_cast<::std::int64_t>(0x55aa55aa55aa55aaull));
        add_i64_update_local_set(wasm_op::i64_shl, 5);
        add_i64_update_local_set(wasm_op::i64_shr_s, 13);
        add_i64_update_local_set(wasm_op::i64_shr_u, 9);
        add_i64_update_local_set(wasm_op::i64_rotl, 11);
        add_i64_update_local_set(wasm_op::i64_rotr, 7);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_only_with_runtime_log(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        return 0;
    }

    [[nodiscard]] int test_translate_runtime_log_conbine_kinds()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_runtime_log_conbine_kinds_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_runtime_log_conbine_kinds");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr char kLogPath[]{"/tmp/uwvm_int_translate_runtime_log_conbine_kinds_strict.log"};
        (void)::std::remove(kLogPath);

        // Enable translator runtime-log and capture output for explicit kind-name assertions.
#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"uwvm_int_translate_runtime_log_conbine_kinds_strict.log", ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"/tmp/uwvm_int_translate_runtime_log_conbine_kinds_strict.log", ::fast_io::open_mode::out);
#endif
        ::uwvm2::uwvm::io::enable_runtime_log = true;

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        ::uwvm2::uwvm::io::enable_runtime_log = false;
#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"NUL", ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"/dev/null", ::fast_io::open_mode::out);
#endif

        auto const log_text{read_text_file(kLogPath)};
        UWVM2TEST_REQUIRE(!log_text.empty());

        for(char const* kind : {
                "local_get",
                "local_get2",
                "local_get_i32_localget",
                "local_get_const_i32",
                "local_get_const_i64",
                "local_get_eqz_i32",
#if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
                "local_get2_const_i32_mul",
                "local_get2_const_i32_shl",
#endif
                "i32_add_2localget_local_set",
                "i32_add_2localget_local_tee",
                "i32_add_imm_local_settee_same",
                "i32_sub_imm_local_settee_same",
                "i32_mul_imm_local_settee_same",
                "i32_and_imm_local_settee_same",
                "i32_or_imm_local_settee_same",
                "i32_xor_imm_local_settee_same",
                "i32_shl_imm_local_settee_same",
                "i32_shr_s_imm_local_settee_same",
                "i32_shr_u_imm_local_settee_same",
                "i32_rotl_imm_local_settee_same",
                "i32_rotr_imm_local_settee_same",
                "i64_add_2localget_local_set",
                "i64_add_2localget_local_tee",
                "i64_add_imm_local_settee_same",
                "i64_sub_imm_local_settee_same",
                "i64_mul_imm_local_settee_same",
                "i64_and_imm_local_settee_same",
                "i64_or_imm_local_settee_same",
                "i64_xor_imm_local_settee_same",
                "i64_shl_imm_local_settee_same",
                "i64_shr_s_imm_local_settee_same",
                "i64_shr_u_imm_local_settee_same",
                "i64_rotl_imm_local_settee_same",
                "i64_rotr_imm_local_settee_same",
                "local_get_const_i32_cmp_brif",
                "local_get_const_i32_add",
                "local_get_const_i32_add_localget",
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
                "local_get_const_f32",
                "local_get_const_f64",
                "f32_add_imm_local_settee_same",
                "f32_mul_imm_local_settee_same",
                "f32_sub_imm_local_settee_same",
                "f64_add_imm_local_settee_same",
                "f64_mul_imm_local_settee_same",
                "f64_sub_imm_local_settee_same",
                "f32_acc_add_floor_localget_wait_add",
                "f64_acc_add_abs_localget_wait_add",
                "f32_acc_add_negabs_localget_wait_const",
                "f64_acc_add_negabs_localget_wait_const",
#endif
            })
        {
            UWVM2TEST_REQUIRE(log_contains_kind(log_text, kind));
        }

        (void)::std::remove(kLogPath);

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_runtime_log_conbine_kinds();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
