#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

    // Minimal local-call bridge:
    // Our module only calls a trivial local-defined `nop_void` function, so we implement only that fast path.
    static void call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        using kind_t = optable::trivial_defined_call_kind;
        using info_t = optable::compiled_defined_call_info;

        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(wasm_module_id != SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const* const info = reinterpret_cast<info_t const*>(call_function);
        if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(info->trivial_kind != kind_t::nop_void) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const top = *stack_top_ptr;
        auto const top_addr = reinterpret_cast<::std::uintptr_t>(top);
        if(top_addr < info->param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::std::byte* const base = reinterpret_cast<::std::byte*>(top_addr - info->param_bytes);
        *stack_top_ptr = base;
    }

    [[nodiscard]] byte_vec build_stacktop_spill_fillN_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: nop void with one i64 param (callee for call barrier)
        // Use i64 param to:
        // - defeat stacktop `call0_void_fast` (param_count != 0),
        // - defeat stacktop same-type fast-call (param type i64 is not supported),
        // so the translator must materialize the operand stack and spill cached values.
        {
            func_type ty{{k_val_i64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 spillN+fillN
        // Push 3 i32 values, call nop(i64), drop 1, add remaining two => 3.
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

            op(c, wasm_op::i64_const);
            i64(c, 0);  // call param (dummy)
            op(c, wasm_op::call);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_stacktop_to_operand_stack_spillN(ByteStorage const& bc) noexcept
    {
        // Multi-count spill opfunc is selected by (start_pos, count). Brute-force all plausible combinations for this range.
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
            start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
            ++start_pos)
        {
            for(::std::size_t count = 2; count <= 4; ++count)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                if constexpr(::std::same_as<ValType, wasm_i32>)
                {
                    curr.i32_stack_top_curr_pos = start_pos;
                    remain.i32_stack_top_remain_size = count;
                }
                else { continue; }

                auto const fptr = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_operand_stack_to_stacktop_fillN(ByteStorage const& bc) noexcept
    {
        // Multi-count fill opfunc is selected by (start_pos, count). Brute-force all plausible combinations for this range.
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
            start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
            ++start_pos)
        {
            for(::std::size_t count = 2; count <= 4; ++count)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                if constexpr(::std::same_as<ValType, wasm_i32>)
                {
                    curr.i32_stack_top_curr_pos = start_pos;
                    remain.i32_stack_top_remain_size = count;
                }
                else { continue; }

                auto const fptr = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
        }
        return false;
    }

    [[nodiscard]] int test_translate_stacktop_spill_fillN_i32() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +call_bridge;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill_fillN_i32_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill_fillN_i32");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall + disjoint scalar rings to enable typed multi-count spill/fill opfuncs.
        constexpr optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = 3uz,
            .i32_stack_top_end_pos = 7uz,
            .i64_stack_top_begin_pos = 7uz,
            .i64_stack_top_end_pos = 11uz,
            .f32_stack_top_begin_pos = 11uz,
            .f32_stack_top_end_pos = 15uz,
            .f64_stack_top_begin_pos = 15uz,
            .f64_stack_top_end_pos = 19uz,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX,
        };
        static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        // Translation assertions: verify the multi-count spill/fill opfuncs exist in bytecode.
        {
            auto const& bc_i32 = cm.local_funcs.index_unchecked(1).op.operands;
            UWVM2TEST_REQUIRE((contains_stacktop_to_operand_stack_spillN<opt, wasm_i32>(bc_i32)));
            UWVM2TEST_REQUIRE((contains_operand_stack_to_stacktop_fillN<opt, wasm_i32>(bc_i32)));
        }

        // Semantics.
        {
            using Runner = interpreter_runner<opt>;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1), rt.local_defined_function_vec_storage.index_unchecked(1), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 3);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill_fillN_i32();
}

