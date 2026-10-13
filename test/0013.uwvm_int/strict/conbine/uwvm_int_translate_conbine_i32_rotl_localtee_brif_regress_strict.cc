#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec input_i32(::std::int32_t value)
    {
        byte_vec bytes(4);
        ::std::memcpy(bytes.data(), ::std::addressof(value), sizeof(value));
        return bytes;
    }

    [[nodiscard]] byte_vec build_module()
    {
        module_builder mb{};
        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body body{};
        body.locals.push_back({1u, k_val_i32});
        auto& code{body.code};
        auto const op{[&](wasm_op value) { append_u8(code, u8(value)); }};
        auto const index{[&](::std::uint32_t value) { append_u32_leb(code, value); }};

        op(wasm_op::block);
        append_u8(code, k_block_empty);
        op(wasm_op::local_get);
        index(0u);
        op(wasm_op::nop);  // Flush pending local.get; keep i32.const pending for heavy combine.
        op(wasm_op::i32_const);
        append_i32_leb(code, 1);
        op(wasm_op::i32_rotl);
        op(wasm_op::local_tee);
        index(1u);
        op(wasm_op::br_if);
        index(0u);
        op(wasm_op::i32_const);
        append_i32_leb(code, 7);
        op(wasm_op::local_set);
        index(1u);
        op(wasm_op::end);
        op(wasm_op::local_get);
        index(1u);
        op(wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(body));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err)};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(compiled.local_funcs.size() == 1uz);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const curr{make_entry_stacktop_currpos<Opt>()};
        constexpr auto tuple{
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{})};
        auto const contains_variant{[&](auto make_fptr)
        {
            auto const& bytecode{compiled.local_funcs.index_unchecked(0).op.operands};
            if(bytecode_contains_fptr(bytecode, make_fptr(curr))) { return true; }
            if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
            {
                for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos < Opt.i32_stack_top_end_pos; ++pos)
                {
                    auto variant{curr};
                    variant.i32_stack_top_curr_pos = pos;
                    if(bytecode_contains_fptr(bytecode, make_fptr(variant))) { return true; }
                }
            }
            return false;
        }};
        auto const mega{[&](auto const& variant)
        {
            return optable::translate::get_uwvmint_i32_binop_imm_stack_local_tee_fptr_from_tuple<
                Opt, optable::numeric_details::int_binop::rotl>(variant, tuple);
        }};
        auto const regular{[&](auto const& variant)
        {
            return optable::translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<
                Opt, optable::numeric_details::int_binop::rotl>(variant, tuple);
        }};
        UWVM2TEST_REQUIRE(!contains_variant(mega));
        UWVM2TEST_REQUIRE(contains_variant(regular));
#endif

        using Runner = interpreter_runner<Opt>;
        auto const& func{compiled.local_funcs.index_unchecked(0)};
        auto const& runtime_func{rt.local_defined_function_vec_storage.index_unchecked(0)};
        UWVM2TEST_REQUIRE(load_i32(Runner::run(func, runtime_func, input_i32(0), nullptr, nullptr).results) == 7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(func, runtime_func, input_i32(1), nullptr, nullptr).results) == 2);
        return 0;
    }
}  // namespace

int main()
{
    install_unexpected_traps();
    optable::call_func = strict_terminate_call;
    optable::call_indirect_func = strict_terminate_call_indirect;
    auto wasm{build_module()};
    auto prepared{prepare_runtime_from_wasm(wasm, u8"uwvm2test_rotl_localtee_brif")};
    UWVM2TEST_REQUIRE(prepared.mod != nullptr);
    runtime_module_t const& rt{*prepared.mod};

    if(abi_mode_enabled("tail-min"))
    {
        constexpr optable::uwvm_interpreter_translate_option_t option{.is_tail_call = true};
        UWVM2TEST_REQUIRE(run_suite<option>(rt) == 0);
    }
    if(abi_mode_enabled("byref"))
    {
        constexpr optable::uwvm_interpreter_translate_option_t option{.is_tail_call = false};
        UWVM2TEST_REQUIRE(run_suite<option>(rt) == 0);
    }
    if(abi_mode_enabled("tail-sysv")) { UWVM2TEST_REQUIRE(run_suite<k_test_tail_sysv_opt>(rt) == 0); }
    if(abi_mode_enabled("tail-aapcs64")) { UWVM2TEST_REQUIRE(run_suite<k_test_tail_aapcs64_opt>(rt) == 0); }
    return 0;
}
