#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using kind_t = optable::trivial_defined_call_kind;
    using info_t = optable::compiled_defined_call_info;

    static runtime_module_t const* g_rt{};
    static compiled_module_t const* g_cm{};

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

    // Minimal trivial-call bridge (same encoding contract as wasm1.h local-defined call fast path).
    static void call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(wasm_module_id != SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const* const info = reinterpret_cast<info_t const*>(call_function);
        if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(info->trivial_kind == kind_t::none) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const top = *stack_top_ptr;
        auto const top_addr = reinterpret_cast<::std::uintptr_t>(top);
        if(top_addr < info->param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::byte* const base = reinterpret_cast<::std::byte*>(top_addr - info->param_bytes);

        ::std::uint_least32_t const imm_u32{::std::bit_cast<::std::uint_least32_t>(info->trivial_imm)};

        auto finish_void = [&]() noexcept { *stack_top_ptr = base; };
        auto finish_i32 = [&](::std::uint32_t v) noexcept
        {
            store_u32(base, v);
            *stack_top_ptr = base + 4;
        };

        switch(info->trivial_kind)
        {
            case kind_t::nop_void:
                finish_void();
                break;
            case kind_t::const_i32:
                finish_i32(static_cast<::std::uint32_t>(imm_u32));
                break;
            case kind_t::param0_i32:
                finish_i32(load_u32(base));
                break;
            case kind_t::add_const_i32:
            {
                auto const a = load_u32(base);
                finish_i32(static_cast<::std::uint32_t>(a + imm_u32));
                break;
            }
            case kind_t::xor_i32:
            {
                auto const a = load_u32(base);
                auto const b = load_u32(base + 4);
                finish_i32(static_cast<::std::uint32_t>(a ^ b));
                break;
            }
            [[unlikely]] default:
                ::fast_io::fast_terminate();
        }
    }

    using imported_table_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t;
    using local_table_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;

    [[nodiscard]] static local_table_t* resolve_imported_table_target(imported_table_t const* it) noexcept
    {
        if(it == nullptr) { return nullptr; }
        switch(it->link_kind)
        {
            case imported_table_t::imported_table_link_kind::defined:
                return it->target.defined_ptr;
            case imported_table_t::imported_table_link_kind::imported:
                return resolve_imported_table_target(it->target.imported_ptr);
            case imported_table_t::imported_table_link_kind::unresolved:
            default:
                return nullptr;
        }
    }

    [[nodiscard]] static local_table_t* resolve_table_storage(runtime_module_t& rt, ::std::size_t table_index) noexcept
    {
        auto const imported_n = rt.imported_table_vec_storage.size();
        if(table_index < imported_n)
        {
            auto const& it = rt.imported_table_vec_storage.index_unchecked(table_index);
            return resolve_imported_table_target(::std::addressof(it));
        }

        auto const local_idx = table_index - imported_n;
        if(local_idx >= rt.local_defined_table_vec_storage.size()) { return nullptr; }
        return ::std::addressof(rt.local_defined_table_vec_storage.index_unchecked(local_idx));
    }

    // Apply active element segments, including those targeting imported tables (after import link).
    static void apply_active_element_segments_any_table(runtime_module_t& rt) noexcept
    {
        using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        using wasm_elem_kind = ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind;

        auto const imported_func_count{rt.imported_function_vec_storage.size()};
        auto const local_func_count{rt.local_defined_function_vec_storage.size()};
        auto const all_func_count{imported_func_count + local_func_count};

        for(auto const& seg : rt.local_defined_element_vec_storage)
        {
            auto const& elem = seg.element;
            if(elem.kind != wasm_elem_kind::active || elem.dropped) { continue; }

            auto const table_idx{static_cast<::std::size_t>(elem.table_idx)};
            auto* const table = resolve_table_storage(rt, table_idx);
            if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const offset{static_cast<::std::size_t>(elem.offset)};
            if((elem.funcidx_begin == nullptr) != (elem.funcidx_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const func_count{
                (elem.funcidx_begin == nullptr) ? 0uz : static_cast<::std::size_t>(elem.funcidx_end - elem.funcidx_begin)};

            if(offset > table->elems.size() || func_count > (table->elems.size() - offset)) [[unlikely]] { ::fast_io::fast_terminate(); }

            for(::std::size_t i{}; i != func_count; ++i)
            {
                auto& slot = table->elems.index_unchecked(offset + i);
                auto const func_idx{static_cast<::std::size_t>(elem.funcidx_begin[i])};
                if(func_idx >= all_func_count) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(func_idx < imported_func_count)
                {
                    slot.storage.imported_ptr = ::std::addressof(rt.imported_function_vec_storage.index_unchecked(func_idx));
                    slot.type = table_elem_type::func_ref_imported;
                }
                else
                {
                    auto const local_idx{func_idx - imported_func_count};
                    slot.storage.defined_ptr = ::std::addressof(rt.local_defined_function_vec_storage.index_unchecked(local_idx));
                    slot.type = table_elem_type::func_ref_defined;
                }
            }
        }
    }

    // call_indirect bridge for tests: resolves imported table and dispatches via `call_bridge`.
    static void call_indirect_bridge(::std::size_t /*wasm_module_id*/,
                                     ::std::size_t type_index,
                                     ::std::size_t table_index,
                                     ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(g_rt == nullptr || g_cm == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        // Pop selector index (i32) - Wasm stack layout: [args..., idx]
        wasm_i32 selector_i32{};
        *stack_top_ptr -= sizeof(selector_i32);
        ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
        auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

        auto const type_begin{g_rt->type_section_storage.type_section_begin};
        auto const type_end{g_rt->type_section_storage.type_section_end};
        if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const type_total{static_cast<::std::size_t>(type_end - type_begin)};
        if(type_index >= type_total) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const* const expected_ft_ptr{type_begin + type_index};

        auto& rt_mut = const_cast<runtime_module_t&>(*g_rt);
        auto* const table = resolve_table_storage(rt_mut, table_index);
        if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(selector_u32 >= table->elems.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};
        if(elem.type != ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto const* const def_ptr{elem.storage.defined_ptr};
        if(def_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(def_ptr->function_type_ptr != expected_ft_ptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const base{g_rt->local_defined_function_vec_storage.data()};
        auto const local_n{g_rt->local_defined_function_vec_storage.size()};
        if(base == nullptr || def_ptr < base || def_ptr >= base + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const local_idx{static_cast<::std::size_t>(def_ptr - base)};

        if(local_idx >= g_cm->local_defined_call_info.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const* const info_ptr{::std::addressof(g_cm->local_defined_call_info.index_unchecked(local_idx))};

        call_bridge(SIZE_MAX, reinterpret_cast<::std::size_t>(info_ptr), stack_top_ptr);
    }

    [[nodiscard]] byte_vec build_lib_table_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        mb.table_has_max = true;
        mb.table_max = 1u;
        mb.add_export_table(0u, "tab");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_main_import_table_call_indirect_module()
    {
        module_builder mb{};

        mb.add_import_table("modT", "tab", 1u, 1u, true);

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (i32)->i32 : x + 7 (trivial-call bridge supports add_const_i32)
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

        // f1: (i32)->i32 : call_indirect (i32)->i32 via imported table[0]
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 0);  // selector idx
            op(c, wasm_op::call_indirect);
            u32(c, 0u);  // typeidx for f0
            u32(c, 0u);  // tableidx=0 (imported)
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // element segment: imported table[0] = func[0] (f0)
        mb.elements.push_back(element_segment{
            .table_index = 0u,
            .offset_expr = [&]
            {
                byte_vec e{};
                op(e, wasm_op::i32_const);
                i32(e, 0);
                op(e, wasm_op::end);
                return e;
            }(),
            .func_indices = {0u},
        });

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);

        g_rt = ::std::addressof(rt);
        g_cm = ::std::addressof(cm);

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(5),
                                              nullptr,
                                              nullptr)
                                       .results) == 12);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);

        return 0;
    }

    [[nodiscard]] int test_translate_imported_table_call_indirect() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +call_bridge;
        optable::call_indirect_func = +call_indirect_bridge;

        auto lib = build_lib_table_module();
        auto wasm = build_main_import_table_call_indirect_module();
        auto prep = prepare_runtime_from_wasm(wasm,
                                              u8"uwvm2test_imported_table_call_indirect_main",
                                              {preloaded_wasm_module{.wasm_bytes = ::std::addressof(lib), .module_name = u8"modT"}});
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto& rt_mut = const_cast<runtime_module_t&>(*prep.mod);
        runtime_module_t const& rt = rt_mut;

        apply_active_element_segments_any_table(rt_mut);

        // Byref mode: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall mode: exercise call_indirect lowering + imported table resolution.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_imported_table_call_indirect();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

