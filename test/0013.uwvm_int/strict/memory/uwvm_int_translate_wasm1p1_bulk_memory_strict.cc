#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_wasm1p1_bulk_memory_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        mb.passive_datas.push_back(passive_data_segment{.bytes = byte_vec{::std::byte{10}, ::std::byte{20}, ::std::byte{30},
                                                                           ::std::byte{40}, ::std::byte{50}, ::std::byte{60}}});

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto op1p1 = [&](byte_vec& c, wasm1p1_op o) { append_u8(c, u8(o)); };
        auto ext = [&](byte_vec& c, wasm1p1_numeric_op o)
        {
            op1p1(c, wasm1p1_op::numeric_prefix);
            append_u32_leb(c, u32(o));
        };
        auto u32_leb = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;

        op(c, wasm_op::i32_const); i32(c, 0);
        op(c, wasm_op::i32_const); i32(c, 0);
        op(c, wasm_op::i32_const); i32(c, 16);
        ext(c, wasm1p1_numeric_op::memory_fill); append_u8(c, 0u);

        op(c, wasm_op::i32_const); i32(c, 4);
        op(c, wasm_op::i32_const); i32(c, 1);
        op(c, wasm_op::i32_const); i32(c, 4);
        ext(c, wasm1p1_numeric_op::memory_init); u32_leb(c, 0u); append_u8(c, 0u);

        op(c, wasm_op::i32_const); i32(c, 8);
        op(c, wasm_op::i32_const); i32(c, 4);
        op(c, wasm_op::i32_const); i32(c, 4);
        ext(c, wasm1p1_numeric_op::memory_copy); append_u8(c, 0u); append_u8(c, 0u);

        ext(c, wasm1p1_numeric_op::data_drop); u32_leb(c, 0u);

        op(c, wasm_op::i32_const); i32(c, 12);
        op(c, wasm_op::i32_const); i32(c, 7);
        op(c, wasm_op::i32_const); i32(c, 2);
        ext(c, wasm1p1_numeric_op::memory_fill); append_u8(c, 0u);

        for(::std::uint32_t offset{4u}; offset != 14u; ++offset)
        {
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load8_u); u32_leb(c, 0u); u32_leb(c, offset);
            if(offset != 4u) { op(c, wasm_op::i32_add); }
        }
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};

        auto const& bc = cm.local_funcs.index_unchecked(0).op.operands;
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_memory_init_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_data_drop_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_memory_copy_fptr_from_tuple<Opt>(curr, tuple)));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, optable::translate::get_uwvmint_memory_fill_fptr_from_tuple<Opt>(curr, tuple)));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(byte_vec const& wasm,
                                            ::uwvm2::utils::container::u8string_view name,
                                            wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, name, {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm) == 0);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                              rt.local_defined_function_vec_storage.index_unchecked(0),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 294);
        return 0;
    }

    [[nodiscard]] int test_translate_wasm1p1_bulk_memory() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto wasm = build_wasm1p1_bulk_memory_module();
        auto features = make_wasm1p1_feature_parameter();

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_bulk_memory_byref", features) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_bulk_memory_tailmin", features) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_bulk_memory_sysv", features) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_bulk_memory_aapcs64", features) == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_wasm1p1_bulk_memory();
}
