#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using wasm_externref_t = ::uwvm2::object::global::wasm_externref_t;
    using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

    [[nodiscard]] byte_vec build_wasm1p1_externref_table_module()
    {
        byte_vec out{};
        append_u8(out, 0x00u);
        append_u8(out, 0x61u);
        append_u8(out, 0x73u);
        append_u8(out, 0x6du);
        append_u8(out, 0x01u);
        append_u8(out, 0x00u);
        append_u8(out, 0x00u);
        append_u8(out, 0x00u);

        auto emit_section = [&](::std::uint8_t id, byte_vec const& payload)
        {
            append_u8(out, id);
            append_u32_leb(out, static_cast<::std::uint32_t>(payload.size()));
            append_bytes(out, payload);
        };

        // (func (param externref) (result i32))
        {
            byte_vec sec{};
            append_u32_leb(sec, 1u);
            append_u8(sec, 0x60u);
            append_u32_leb(sec, 1u);
            append_u8(sec, k_ref_externref);
            append_u32_leb(sec, 1u);
            append_u8(sec, k_val_i32);
            emit_section(1u, sec);
        }
        {
            byte_vec sec{};
            append_u32_leb(sec, 1u);
            append_u32_leb(sec, 0u);
            emit_section(3u, sec);
        }
        {
            byte_vec sec{};
            append_u32_leb(sec, 1u);
            append_u8(sec, k_ref_externref);
            append_u8(sec, 0x01u);  // min + max
            append_u32_leb(sec, 6u);
            append_u32_leb(sec, 8u);
            emit_section(4u, sec);
        }
        {
            byte_vec sec{};
            append_u32_leb(sec, 2u);

            // Active, explicitly indexed externref expression segment. The
            // initializer writes slot 4 and must then drop this payload.
            append_u32_leb(sec, 6u);
            append_u32_leb(sec, 0u);
            append_u8(sec, u8(wasm_op::i32_const));
            append_i32_leb(sec, 4);
            append_u8(sec, u8(wasm_op::end));
            append_u8(sec, k_ref_externref);
            append_u32_leb(sec, 1u);
            append_u8(sec, u8(wasm1p1_op::ref_null));
            append_u8(sec, k_ref_externref);
            append_u8(sec, u8(wasm_op::end));

            // Passive externref expression segment used by table.init.
            append_u32_leb(sec, 5u);
            append_u8(sec, k_ref_externref);
            append_u32_leb(sec, 1u);
            append_u8(sec, u8(wasm1p1_op::ref_null));
            append_u8(sec, k_ref_externref);
            append_u8(sec, u8(wasm_op::end));
            emit_section(9u, sec);
        }

        byte_vec code{};
        auto op = [&](wasm_op value) { append_u8(code, u8(value)); };
        auto op1p1 = [&](wasm1p1_op value) { append_u8(code, u8(value)); };
        auto ext = [&](wasm1p1_numeric_op value)
        {
            op1p1(wasm1p1_op::numeric_prefix);
            append_u32_leb(code, u32(value));
        };
        auto i32_const = [&](::std::int32_t value)
        {
            op(wasm_op::i32_const);
            append_i32_leb(code, value);
        };
        auto local_externref = [&]
        {
            op(wasm_op::local_get);
            append_u32_leb(code, 0u);
        };
        auto table_get = [&](::std::int32_t index)
        {
            i32_const(index);
            op1p1(wasm1p1_op::table_get);
            append_u32_leb(code, 0u);
        };

        ext(wasm1p1_numeric_op::table_size);  // accumulator = 6
        append_u32_leb(code, 0u);

        i32_const(0);
        local_externref();
        op1p1(wasm1p1_op::table_set);
        append_u32_leb(code, 0u);
        table_get(0);
        op1p1(wasm1p1_op::ref_is_null);
        op(wasm_op::i32_add);  // +0

        local_externref();
        i32_const(1);
        ext(wasm1p1_numeric_op::table_grow);
        append_u32_leb(code, 0u);
        op(wasm_op::i32_add);  // +6

        ext(wasm1p1_numeric_op::table_size);
        append_u32_leb(code, 0u);
        op(wasm_op::i32_add);  // +7

        i32_const(1);
        local_externref();
        i32_const(2);
        ext(wasm1p1_numeric_op::table_fill);
        append_u32_leb(code, 0u);

        i32_const(3);
        i32_const(1);
        i32_const(2);
        ext(wasm1p1_numeric_op::table_copy);
        append_u32_leb(code, 0u);
        append_u32_leb(code, 0u);
        table_get(3);
        op1p1(wasm1p1_op::ref_is_null);
        op(wasm_op::i32_add);  // +0

        i32_const(5);
        i32_const(0);
        i32_const(1);
        ext(wasm1p1_numeric_op::table_init);
        append_u32_leb(code, 1u);
        append_u32_leb(code, 0u);
        table_get(5);
        op1p1(wasm1p1_op::ref_is_null);
        op(wasm_op::i32_add);  // +1

        ext(wasm1p1_numeric_op::elem_drop);
        append_u32_leb(code, 1u);
        op(wasm_op::end);

        {
            byte_vec body{};
            append_u32_leb(body, 0u);  // local declarations
            append_bytes(body, code);
            byte_vec sec{};
            append_u32_leb(sec, 1u);
            append_u32_leb(sec, static_cast<::std::uint32_t>(body.size()));
            append_bytes(sec, body);
            emit_section(10u, sec);
        }
        return out;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] bool contains_externref_is_null(compiled_local_func_t const& lf) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos != Opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(
                       lf.op.operands,
                       optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_externref_t>(curr, tuple)))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            return bytecode_contains_fptr(
                lf.op.operands,
                optable::translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<Opt, wasm_externref_t>(curr, tuple));
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(byte_vec const& wasm,
                                            ::uwvm2::utils::container::u8string_view name,
                                            wasm_feature_parameter_t const& features) noexcept
    {
        auto prep{prepare_runtime_from_wasm(wasm, name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        auto& rt{const_cast<runtime_module_t&>(*prep.mod)};
        UWVM2TEST_REQUIRE(rt.local_defined_table_vec_storage.size() == 1uz);
        auto& table{rt.local_defined_table_vec_storage.front_unchecked()};
        UWVM2TEST_REQUIRE(table.elems.size() == 6uz);
        for(auto const& elem: table.elems) { UWVM2TEST_REQUIRE(elem.type == table_elem_type::extern_ref); }

        UWVM2TEST_REQUIRE(rt.local_defined_element_vec_storage.size() == 2uz);
        auto const& active{rt.local_defined_element_vec_storage.index_unchecked(0uz).element};
        UWVM2TEST_REQUIRE(active.dropped);
        UWVM2TEST_REQUIRE(active.funcidx_begin == nullptr && active.funcidx_end == nullptr);
        UWVM2TEST_REQUIRE(active.externref_begin == nullptr && active.externref_end == nullptr);
        auto& passive{rt.local_defined_element_vec_storage.index_unchecked(1uz).element};
        UWVM2TEST_REQUIRE(!passive.dropped);
        UWVM2TEST_REQUIRE(passive.externref_begin != nullptr);
        UWVM2TEST_REQUIRE(passive.externref_end == passive.externref_begin + 1);
        UWVM2TEST_REQUIRE(passive.externref_begin[0] == nullptr);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm{compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features))};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        auto const& lf{cm.local_funcs.front_unchecked()};
        auto const& bc{lf.op.operands};
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_get_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_set_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_init_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_grow_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_fill_externref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_table_copy_funcref_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(contains_externref_is_null<Opt>(lf));

        int host_value{42};
        wasm_externref_t ref{};
        ref.ref.storage.ptr = ::std::addressof(host_value);
        ref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
        byte_vec packed(sizeof(ref));
        ::std::memcpy(packed.data(), ::std::addressof(ref), sizeof(ref));

        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(lf, rt.local_defined_function_vec_storage.front_unchecked(), packed, nullptr, nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 20);
        UWVM2TEST_REQUIRE(passive.dropped);
        UWVM2TEST_REQUIRE(passive.externref_begin == nullptr && passive.externref_end == nullptr);
        UWVM2TEST_REQUIRE(table.elems.size() == 7uz);
        UWVM2TEST_REQUIRE(table.elems.index_unchecked(0uz).storage.extern_ptr == ::std::addressof(host_value));
        UWVM2TEST_REQUIRE(table.elems.index_unchecked(3uz).storage.extern_ptr == ::std::addressof(host_value));
        UWVM2TEST_REQUIRE(table.elems.index_unchecked(5uz).storage.extern_ptr == nullptr);
        UWVM2TEST_REQUIRE(table.elems.index_unchecked(6uz).storage.extern_ptr == ::std::addressof(host_value));
        return 0;
    }

    [[nodiscard]] int test_wasm1p1_externref_table() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto const wasm{build_wasm1p1_externref_table_module()};
        auto const features{make_wasm1p1_feature_parameter()};

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_externref_byref", features) == 0);
        }
        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_externref_tail_min", features) == 0);
        }
        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_externref_tail_sysv", features) == 0);
        }
        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_externref_tail_aapcs64", features) == 0);
        }
        return 0;
    }
}

int main()
{
    return test_wasm1p1_externref_table();
}
