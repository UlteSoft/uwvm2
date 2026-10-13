/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include "../strict/uwvm_int_translate_strict_common.h"

namespace
{
    using namespace uwvm2test::uwvm_int_strict;

    template <optable::uwvm_interpreter_translate_option_t Option>
    int check()
    {
        // (global i32 (i32.mul (i32.add (i32.const -1) (i32.const 2)) (i32.const 3)))
        // A table initializer supplies a non-null ref.func 0 and declares that reference for the body.
        // The body checks the table, then exercises delayed local.get/add/local.tee with an observable result.
        constexpr unsigned raw[]{0, 97, 115, 109, 1, 0, 0, 0,
            1, 5, 1, 0x60, 0, 1, 0x7f,
            3, 2, 1, 0,
            4, 9, 1, 0x40, 0, 0x70, 0, 3, 0xd2, 0, 0x0b,
            6, 12, 1, 0x7f, 0, 0x41, 0x7f, 0x41, 2, 0x6a, 0x41, 3, 0x6c, 0x0b,
            10, 40, 1, 38, 1, 2, 0x7f, 0x41, 0, 0x25, 0, 0xd1, 0x04, 0x40, 0, 0x0b,
            0xd2, 0, 0x1a, 0x23, 0, 0x21, 0, 0x41, 7, 0x21, 1, 0x20, 0, 0x01, 0x20, 1,
            0x6a, 0x22, 0, 0x20, 0, 0x6a, 0x41, 24, 0x6b, 0x0b};
        byte_vec wasm{};
        for(auto b: raw) { wasm.push_back(static_cast<std::byte>(b)); }
        wasm_feature_parameter_t parameters{};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters).disable_extended_const = false;
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters).disable_table_initializer = false;
        auto prepared = prepare_runtime_from_wasm(wasm, u8"wasm3_const_backend", {}, parameters);
        uwvm2::validation::error::code_validation_error_impl error{};
        optable::compile_option configuration{};
        auto compiled = compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, configuration, error, &parameters);
        UWVM2TEST_REQUIRE(error.err_code == uwvm2::validation::error::code_validation_error_code::ok);
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
        if constexpr(Option.is_tail_call)
        {
            constexpr auto tuple = compiler::details::make_interpreter_tuple<Option>(
                std::make_index_sequence<compiler::details::interpreter_tuple_size<Option>()>{});
            auto current{make_entry_stacktop_currpos<Option>()};
            bool found{};
            auto const& bytecode{compiled.local_funcs.index_unchecked(0).op.operands};
            for(std::size_t position{Option.i32_stack_top_begin_pos}; position < Option.i32_stack_top_end_pos; ++position)
            {
                current.i32_stack_top_curr_pos = position;
                found = found || bytecode_contains_fptr(bytecode,
                    optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Option, optable::numeric_details::int_binop::add>(current, tuple));
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
                found = found || bytecode_contains_fptr(bytecode,
                    optable::translate::get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple<Option>(current, tuple));
# else
                found = found || bytecode_contains_fptr(bytecode,
                    optable::translate::get_uwvmint_i32_add_2localget_local_tee_common_fptr_from_tuple<Option>(current, tuple));
# endif
            }
            UWVM2TEST_REQUIRE(found);
        }
#endif
        auto result = interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(0), {}, nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_i32(result.results) == -4);
        return 0;
    }
}

int main()
{
    install_unexpected_traps();
    optable::call_func = strict_terminate_call;
    optable::call_indirect_func = strict_terminate_call_indirect;
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call = false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    UWVM2TEST_REQUIRE(check<uncached>() == 0);
    UWVM2TEST_REQUIRE(check<ring>() == 0);
    std::puts("wasm3 extended constants: interpreter execution passed");
}
