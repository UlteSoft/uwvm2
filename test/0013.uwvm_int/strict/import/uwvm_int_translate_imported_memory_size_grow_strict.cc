#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
    using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_i32_one_slot_opt{
        .is_tail_call = true,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 4uz,
        .i64_stack_top_begin_pos = 4uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz,
        .f32_stack_top_end_pos = 6uz,
        .f64_stack_top_begin_pos = 6uz,
        .f64_stack_top_end_pos = 7uz,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX,
    };

    [[nodiscard]] native_memory_t const* resolve_imported_memory(imported_memory_storage_t const& im) noexcept
    {
        using link_kind = imported_memory_storage_t::imported_memory_link_kind;

        switch(im.link_kind)
        {
            case link_kind::defined:
                if(im.target.defined_ptr == nullptr) { return nullptr; }
                return ::std::addressof(im.target.defined_ptr->memory);
            case link_kind::imported:
                if(im.target.imported_ptr == nullptr) { return nullptr; }
                return resolve_imported_memory(*im.target.imported_ptr);
            case link_kind::local_imported:
                return nullptr;
            case link_kind::unresolved:
            default:
                return nullptr;
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i32_pushes(::std::size_t push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != push_count; ++i)
            {
                curr.i32_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_fill1(ByteStorage const& bc) noexcept
    {
        if constexpr(!Opt.is_tail_call)
        {
            return false;
        }
        else if constexpr(Opt.i32_stack_top_begin_pos == SIZE_MAX || Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
        {
            return false;
        }
        else
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
                start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
                ++start_pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                curr.i32_stack_top_curr_pos = start_pos;
                remain.i32_stack_top_remain_size = 1uz;

                auto const fptr = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }

            return false;
        }
    }

    [[nodiscard]] byte_vec build_provider_memory_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 2u;
        mb.add_export_memory("mem");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_imported_memory_size_grow_module()
    {
        module_builder mb{};
        mb.add_import_memory("prov", "mem", 1u, 2u, true);

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_const); i32(c, 65536);
            op(c, wasm_op::i32_const); i32(c, 0x5a);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 65536);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm, native_memory_t const& mem) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const curr0 = make_initial_stacktop_currpos<Opt>();
        auto const curr1 = make_curr_after_i32_pushes<Opt>(1uz);
        auto const curr2 = make_curr_after_i32_pushes<Opt>(2uz);
        auto const curr3 = make_curr_after_i32_pushes<Opt>(3uz);

        auto const exp_memory_size_0 = optable::translate::get_uwvmint_memory_size_fptr_from_tuple<Opt>(curr0, tuple);
        auto const exp_memory_size_1 = optable::translate::get_uwvmint_memory_size_fptr_from_tuple<Opt>(curr1, tuple);
        auto const exp_memory_grow = optable::translate::get_uwvmint_memory_grow_fptr_from_tuple<Opt>(curr1, tuple);
        auto const exp_i32_store8 = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr3, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr3, tuple);
            }
        }();
        auto const exp_i32_load8_u = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr2, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr2, tuple);
            }
        }();

        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_memory_size_0));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_memory_grow));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_memory_size_1));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_load8_u));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_memory_grow));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_memory_size_0));

        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX &&
                     (Opt.i32_stack_top_end_pos - Opt.i32_stack_top_begin_pos) == 1uz)
        {
            UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i32>(cm.local_funcs.index_unchecked(2).op.operands)));
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_once(byte_vec const& provider_wasm,
                               byte_vec const& consumer_wasm,
                               ::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto prep = prepare_runtime_from_wasm(consumer_wasm,
                                              module_name,
                                              {
                                                  preloaded_wasm_module{.wasm_bytes = ::std::addressof(provider_wasm), .module_name = u8"prov"},
                                              });
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(rt.imported_memory_vec_storage.size() == 1uz);
        auto const* const mem_p = resolve_imported_memory(rt.imported_memory_vec_storage.index_unchecked(0));
        UWVM2TEST_REQUIRE(mem_p != nullptr);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, *mem_p) == 0);

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 1);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 3);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr2.results) == 97);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr3.results) == -1);

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                               rt.local_defined_function_vec_storage.index_unchecked(4),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr4.results) == 2);

        return 0;
    }

    [[nodiscard]] int test_translate_imported_memory_size_grow() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm_provider = build_provider_memory_module();
        auto wasm_consumer = build_imported_memory_size_grow_module();

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm_provider, wasm_consumer, u8"cons_imported_memory_size_grow_byref") == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm_provider, wasm_consumer, u8"cons_imported_memory_size_grow_tailmin") == 0);
        }

        {
            constexpr auto opt{k_test_tail_i32_one_slot_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm_provider, wasm_consumer, u8"cons_imported_memory_size_grow_tail_i32_one_slot") == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm_provider, wasm_consumer, u8"cons_imported_memory_size_grow_tail_sysv") == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm_provider, wasm_consumer, u8"cons_imported_memory_size_grow_tail_aapcs64") == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_imported_memory_size_grow();
}
