#include "../uwvm_int_translate_strict_common.h"

#include <cstdint>
#include <vector>

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using kind_t = optable::trivial_defined_call_kind;
    using wasm_byte = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
    using wasm_code_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t;

    [[nodiscard]] kind_t match_kind(::std::vector<wasm_byte> const& bytes) noexcept
    {
        wasm_code_t code{};
        code.body.expr_begin = bytes.data();
        code.body.code_end = bytes.data() + bytes.size();
        return compiler::details::match_trivial_call_inline_body(&code).kind;
    }

    [[nodiscard]] int test_trivial_matcher_negative() noexcept
    {
        auto op = [](wasm_op o) noexcept -> wasm_byte { return static_cast<wasm_byte>(u8(o)); };

        // nullptr => kind none
        {
            UWVM2TEST_REQUIRE(compiler::details::match_trivial_call_inline_body(nullptr).kind == kind_t::none);
        }

        auto expect = [&](::std::vector<wasm_byte> const& bytes, kind_t k) noexcept -> int
        {
            UWVM2TEST_REQUIRE(match_kind(bytes) == k);
            return 0;
        };

        auto expect_none = [&](::std::vector<wasm_byte> const& bytes) noexcept -> int
        {
            UWVM2TEST_REQUIRE(match_kind(bytes) == kind_t::none);
            return 0;
        };

        // empty => fail to read first op
        {
            ::std::vector<wasm_byte> b{};
            UWVM2TEST_REQUIRE(expect_none(b) == 0);
        }

        // nop_void: end (positive) + extra trailing bytes (negative)
        {
            ::std::vector<wasm_byte> ok{op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect(ok, kind_t::nop_void) == 0);

            auto bad = ok;
            bad.push_back(op(wasm_op::nop));
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);  // curr != end
        }

        // const_i32: i32.const IMM ; end
        {
            ::std::vector<wasm_byte> ok{op(wasm_op::i32_const), 0x00u, op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect(ok, kind_t::const_i32) == 0);

            // multi-byte i32 leb (128) => exercises leb loop
            ::std::vector<wasm_byte> ok2{op(wasm_op::i32_const), 0x80u, 0x01u, op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect(ok2, kind_t::const_i32) == 0);

            // missing leb
            ::std::vector<wasm_byte> trunc0{op(wasm_op::i32_const)};
            UWVM2TEST_REQUIRE(expect_none(trunc0) == 0);

            // unterminated leb
            ::std::vector<wasm_byte> trunc1{op(wasm_op::i32_const), 0x80u};
            UWVM2TEST_REQUIRE(expect_none(trunc1) == 0);

            // wrong tail op (expects end)
            ::std::vector<wasm_byte> bad_tail{op(wasm_op::i32_const), 0x00u, op(wasm_op::i32_add)};
            UWVM2TEST_REQUIRE(expect_none(bad_tail) == 0);
        }

        // param0_i32: local.get 0 ; end
        {
            ::std::vector<wasm_byte> ok{op(wasm_op::local_get), 0x00u, op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect(ok, kind_t::param0_i32) == 0);

            // missing end
            ::std::vector<wasm_byte> trunc{op(wasm_op::local_get), 0x00u};
            UWVM2TEST_REQUIRE(expect_none(trunc) == 0);

            // wrong index => not a trivial param0 matcher
            ::std::vector<wasm_byte> bad_idx{op(wasm_op::local_get), 0x01u, op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect_none(bad_idx) == 0);

            // multi-byte u32 leb (128) => exercises leb loop, but index mismatch => none
            ::std::vector<wasm_byte> bad_idx2{op(wasm_op::local_get), 0x80u, 0x01u, op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect_none(bad_idx2) == 0);
        }

        // add_const_i32: local.get 0 ; i32.const IMM ; i32.add ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x01u,
                op(wasm_op::i32_add),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::add_const_i32) == 0);

            auto bad_op = ok;
            bad_op[4] = op(wasm_op::i32_sub);
            UWVM2TEST_REQUIRE(expect_none(bad_op) == 0);

            auto bad_trunc = ok;
            bad_trunc.pop_back();
            UWVM2TEST_REQUIRE(expect_none(bad_trunc) == 0);
        }

        // xor_const_i32: local.get 0 ; i32.const IMM ; i32.xor ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x01u,
                op(wasm_op::i32_xor),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::xor_const_i32) == 0);

            auto bad = ok;
            // Avoid accidentally matching another trivial pattern (e.g. add_const_i32).
            bad[4] = op(wasm_op::i32_sub);
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // mul_const_i32: local.get 0 ; i32.const IMM ; i32.mul ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x02u,
                op(wasm_op::i32_mul),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::mul_const_i32) == 0);

            auto bad = ok;
            bad[3] = 0x80u;  // start an unterminated leb => should fail read_i32_leb
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // rotr_add_const_i32: local.get 0 ; i32.const ROTR ; i32.rotr ; i32.const ADD ; i32.add ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x05u,
                op(wasm_op::i32_rotr),
                op(wasm_op::i32_const),
                0x07u,
                op(wasm_op::i32_add),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::rotr_add_const_i32) == 0);

            auto bad = ok;
            bad[4] = op(wasm_op::i32_rotl);
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // xor_i32: local.get 0/1 ; local.get 1/0 ; i32.xor ; end
        {
            ::std::vector<wasm_byte> ok0{op(wasm_op::local_get), 0x00u, op(wasm_op::local_get), 0x01u, op(wasm_op::i32_xor), op(wasm_op::end)};
            ::std::vector<wasm_byte> ok1{op(wasm_op::local_get), 0x01u, op(wasm_op::local_get), 0x00u, op(wasm_op::i32_xor), op(wasm_op::end)};
            UWVM2TEST_REQUIRE(expect(ok0, kind_t::xor_i32) == 0);
            UWVM2TEST_REQUIRE(expect(ok1, kind_t::xor_i32) == 0);

            auto bad = ok0;
            // Avoid accidentally matching another trivial pattern (e.g. xor_const_i32).
            bad[2] = op(wasm_op::i64_const);
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // xor_add_const_i32: local.get 1 ; i32.const IMM ; i32.xor ; local.get 0 ; i32.add ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x01u,
                op(wasm_op::i32_const),
                0x2au,
                op(wasm_op::i32_xor),
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_add),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::xor_add_const_i32) == 0);

            auto bad = ok;
            bad[6] = 0x01u;  // expects local.get 0
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // sub_or_const_i32: local.get 0 ; local.get 1 ; i32.const IMM ; i32.or ; i32.sub ; end
        {
            ::std::vector<wasm_byte> ok{
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::local_get),
                0x01u,
                op(wasm_op::i32_const),
                0x11u,
                op(wasm_op::i32_or),
                op(wasm_op::i32_sub),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::sub_or_const_i32) == 0);

            auto bad = ok;
            bad[7] = op(wasm_op::i32_add);
            UWVM2TEST_REQUIRE(expect_none(bad) == 0);
        }

        // sum8_xor_const_i32: pattern E (positive + a few negative mutations)
        {
            ::std::vector<wasm_byte> ok{
                // local.get 0 ; local.get 1 ; i32.add ;
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::local_get),
                0x01u,
                op(wasm_op::i32_add),
                // local.get 2 ; local.get 3 ; i32.add ; i32.add ;
                op(wasm_op::local_get),
                0x02u,
                op(wasm_op::local_get),
                0x03u,
                op(wasm_op::i32_add),
                op(wasm_op::i32_add),
                // local.get 4 ; local.get 5 ; i32.add ;
                op(wasm_op::local_get),
                0x04u,
                op(wasm_op::local_get),
                0x05u,
                op(wasm_op::i32_add),
                // local.get 6 ; local.get 7 ; i32.add ; i32.add ; i32.add ;
                op(wasm_op::local_get),
                0x06u,
                op(wasm_op::local_get),
                0x07u,
                op(wasm_op::i32_add),
                op(wasm_op::i32_add),
                op(wasm_op::i32_add),
                // i32.const IMM ; i32.xor ; end
                op(wasm_op::i32_const),
                0x7fu,  // -1 (1-byte signed leb)
                op(wasm_op::i32_xor),
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::sum8_xor_const_i32) == 0);

            auto bad_idx = ok;
            // idx3 (0x03u) -> 0x09u
            bad_idx[8] = 0x09u;
            UWVM2TEST_REQUIRE(expect_none(bad_idx) == 0);

            auto bad_op = ok;
            // one i32.add -> i32.mul
            bad_op[10] = op(wasm_op::i32_mul);
            UWVM2TEST_REQUIRE(expect_none(bad_op) == 0);

            auto bad_trunc = ok;
            bad_trunc.resize(7);  // chop mid-stream => read_op failure
            UWVM2TEST_REQUIRE(expect_none(bad_trunc) == 0);
        }

        // xorshift32_i32: cover both match and fail paths inside the canonical matcher.
        {
            ::std::vector<wasm_byte> ok{
                // local.get 0
                op(wasm_op::local_get),
                0x00u,
                // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x0du,
                op(wasm_op::i32_shl),
                op(wasm_op::i32_xor),
                op(wasm_op::local_set),
                0x00u,
                // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x11u,
                op(wasm_op::i32_shr_u),
                op(wasm_op::i32_xor),
                op(wasm_op::local_set),
                0x00u,
                // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::i32_const),
                0x05u,
                op(wasm_op::i32_shl),
                op(wasm_op::i32_xor),
                op(wasm_op::local_set),
                0x00u,
                // local.get 0 ; end
                op(wasm_op::local_get),
                0x00u,
                op(wasm_op::end),
            };
            UWVM2TEST_REQUIRE(expect(ok, kind_t::xorshift32_i32) == 0);

            // Wrong first local index => fail early in expect_local0.
            {
                auto bad = ok;
                bad[1] = 0x01u;
                UWVM2TEST_REQUIRE(expect_none(bad) == 0);
            }

            // Wrong imm (expects 13) => exercises `imm != expected` path in expect_i32_const.
            {
                auto bad = ok;
                bad[5] = 0x0eu;
                UWVM2TEST_REQUIRE(expect_none(bad) == 0);
            }

            // Wrong opcode (expects i32.shl) => fail in expect_op.
            {
                auto bad = ok;
                bad[6] = op(wasm_op::i32_shr_u);
                UWVM2TEST_REQUIRE(expect_none(bad) == 0);
            }

            // Wrong local.set index => fail in read_u32_leb(idx) || idx != 0.
            {
                auto bad = ok;
                bad[9] = 0x01u;
                UWVM2TEST_REQUIRE(expect_none(bad) == 0);
            }

            // Truncated mid-body => exercises `curr == end` failure inside readers.
            {
                auto bad = ok;
                bad.resize(4);
                UWVM2TEST_REQUIRE(expect_none(bad) == 0);
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_trivial_matcher_negative();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
