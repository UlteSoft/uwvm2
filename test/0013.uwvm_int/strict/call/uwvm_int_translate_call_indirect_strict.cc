#include "../uwvm_int_translate_strict_common.h"

#include <cstdlib>

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using kind_t = optable::trivial_defined_call_kind;
    using info_t = optable::compiled_defined_call_info;

    static runtime_module_t const* g_rt{};
    static compiled_module_t const* g_cm{};
    static bool g_trace_ci{};

    template <typename... Args>
    inline void trace_ci(Args&&... args) noexcept
    {
        if(!g_trace_ci) { return; }
        ::fast_io::perrln(::std::forward<Args>(args)...);
    }

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

    // Minimal trivial-call bridge (same encoding contract as translate.h local-defined call fast path).
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

    // Minimal call_indirect bridge for tests: resolves local-defined table element and dispatches via `call_bridge`.
    static void call_indirect_bridge(::std::size_t /*wasm_module_id*/,
                                     ::std::size_t type_index,
                                     ::std::size_t table_index,
                                     ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        auto die = [&](char const* msg) noexcept
        {
            trace_ci("[ci] fail: ", ::fast_io::manipulators::os_c_str(msg));
            ::fast_io::fast_terminate();
        };

        if(g_rt == nullptr || g_cm == nullptr) [[unlikely]] { die("g_rt/g_cm is null"); }
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { die("stack_top_ptr is null"); }

        // Pop selector index (i32) - Wasm stack layout: [args..., idx]
        wasm_i32 selector_i32{};  // init
        *stack_top_ptr -= sizeof(selector_i32);
        ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
        auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};
        trace_ci("[ci] type=", type_index, " table=", table_index, " selector=", selector_u32);

        auto const type_begin{g_rt->type_section_storage.type_section_begin};
        auto const type_end{g_rt->type_section_storage.type_section_end};
        if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { die("type_section_begin/end is null"); }
        auto const type_total{static_cast<::std::size_t>(type_end - type_begin)};
        trace_ci("[ci] type_total=", type_total);
        if(type_index >= type_total) [[unlikely]] { die("type_index out of bounds"); }
        auto const* const expected_ft_ptr{type_begin + type_index};

        auto const imported_table_n{g_rt->imported_table_vec_storage.size()};
        trace_ci("[ci] imported_table_n=", imported_table_n, " local_table_n=", g_rt->local_defined_table_vec_storage.size());
        if(table_index < imported_table_n) [[unlikely]] { die("table_index refers to imported table (unsupported in test)"); }
        auto const local_table_index{table_index - imported_table_n};
        if(local_table_index >= g_rt->local_defined_table_vec_storage.size()) [[unlikely]] { die("local_table_index out of bounds"); }

        auto const& table{g_rt->local_defined_table_vec_storage.index_unchecked(local_table_index)};
        trace_ci("[ci] table_elems_n=", table.elems.size());
        if(selector_u32 >= table.elems.size()) [[unlikely]] { die("selector out of bounds"); }

        auto const& elem{table.elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};
        trace_ci("[ci] elem.type=", static_cast<unsigned>(elem.type),
                 " elem.ptr=", reinterpret_cast<::std::uintptr_t>(elem.storage.defined_ptr));
        if(elem.type != ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined) [[unlikely]]
        {
            die("elem.type not func_ref_defined");
        }

        auto const* const def_ptr{elem.storage.defined_ptr};
        if(def_ptr == nullptr) [[unlikely]] { die("defined_ptr is null"); }
        if(def_ptr->function_type_ptr != expected_ft_ptr) [[unlikely]]
        {
            trace_ci("[ci] expected_ft_ptr=", reinterpret_cast<::std::uintptr_t>(expected_ft_ptr),
                     " actual_ft_ptr=", reinterpret_cast<::std::uintptr_t>(def_ptr->function_type_ptr));
            die("function_type_ptr mismatch");
        }

        auto const base{g_rt->local_defined_function_vec_storage.data()};
        auto const local_n{g_rt->local_defined_function_vec_storage.size()};
        if(base == nullptr || def_ptr < base || def_ptr >= base + local_n) [[unlikely]] { die("defined_ptr not in local_defined_function_vec_storage"); }
        auto const local_idx{static_cast<::std::size_t>(def_ptr - base)};
        trace_ci("[ci] local_idx=", local_idx);

        if(local_idx >= g_cm->local_defined_call_info.size()) [[unlikely]] { die("local_idx out of bounds for compiled call_info"); }
        auto const* const info_ptr{::std::addressof(g_cm->local_defined_call_info.index_unchecked(local_idx))};

        call_bridge(SIZE_MAX, reinterpret_cast<::std::size_t>(info_ptr), stack_top_ptr);
    }

    // The strict-test harness only decays module storage and does not apply active element segments into tables.
    // Do a minimal instantiation for table init so `call_indirect` has valid targets.
    static void apply_active_element_segments(runtime_module_t& rt) noexcept
    {
        using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        using wasm_elem_kind = ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind;

        auto const imported_func_count{rt.imported_function_vec_storage.size()};
        auto const local_func_count{rt.local_defined_function_vec_storage.size()};
        auto const all_func_count{imported_func_count + local_func_count};

        auto const imported_table_count{rt.imported_table_vec_storage.size()};

        for(auto const& seg : rt.local_defined_element_vec_storage)
        {
            auto const& elem = seg.element;
            if(elem.kind != wasm_elem_kind::active || elem.dropped) { continue; }

            auto const table_idx{static_cast<::std::size_t>(elem.table_idx)};
            if(table_idx < imported_table_count) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const local_table_idx{table_idx - imported_table_count};
            if(local_table_idx >= rt.local_defined_table_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto& table = rt.local_defined_table_vec_storage.index_unchecked(local_table_idx);

            auto const offset{static_cast<::std::size_t>(elem.offset)};
            if((elem.funcidx_begin == nullptr) != (elem.funcidx_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const func_count{
                (elem.funcidx_begin == nullptr) ? 0uz : static_cast<::std::size_t>(elem.funcidx_end - elem.funcidx_begin)};

            if(offset > table.elems.size() || func_count > (table.elems.size() - offset)) [[unlikely]] { ::fast_io::fast_terminate(); }

            for(::std::size_t i{}; i != func_count; ++i)
            {
                auto& slot = table.elems.index_unchecked(offset + i);
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

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32x3(::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32x4(::std::int32_t a, ::std::int32_t b, ::std::int32_t c, ::std::int32_t d)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        ::std::memcpy(out.data() + 12, ::std::addressof(d), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32x5(::std::int32_t a, ::std::int32_t b, ::std::int32_t c, ::std::int32_t d, ::std::int32_t e)
    {
        byte_vec out(20);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        ::std::memcpy(out.data() + 12, ::std::addressof(d), 4);
        ::std::memcpy(out.data() + 16, ::std::addressof(e), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_call_indirect_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 16u;
        mb.table_has_max = true;
        mb.table_max = 16u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // Callees (table[0..9]) - keep bodies trivially matchable for our call bridge.
        // 0: () -> i32 = 11
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 1: () -> void
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 2: (i32) -> i32 = x + 7
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 3: (i32) -> void
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 4: (i32,i32) -> i32 = a ^ b
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 5: (i32,i32) -> void
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 6: (i32,i32,i32) -> i32 = param0 (keeps trivial-call bridge support for ParamCount=3)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 7: (i32,i32,i32,i32) -> i32 = param0
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 8: (i32,i32,i32,i32) -> void
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 9: (i32,i32,i32,i32,i32) -> i32 = param0 (forces slow-path: param_count>4)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // Element init: table[0..9] = func[0..9]
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
            .func_indices = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u},
        });

        auto emit_ci = [&](byte_vec& c, ::std::uint32_t tyidx, ::std::uint32_t tabidx)
        {
            op(c, wasm_op::call_indirect);
            u32(c, tyidx);
            u32(c, tabidx);
        };

        // Callers (10..)
        // 10: () -> i32 : call_indirect ()->i32 selector=0
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 0);
            emit_ci(c, 0u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 11: () -> i32 : call_indirect ()->void selector=1 ; return 123
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 1);
            emit_ci(c, 1u, 0u);
            op(c, wasm_op::i32_const); i32(c, 123);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 12: (i32)->i32 : call_indirect (i32)->i32 selector=2
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 2);
            emit_ci(c, 2u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 13: (i32)->i32 : call_indirect (i32)->void selector=3 ; return 55
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 3);
            emit_ci(c, 3u, 0u);
            op(c, wasm_op::i32_const); i32(c, 55);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 14: (i32,i32)->i32 : call_indirect (i32,i32)->i32 selector=4
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 4);
            emit_ci(c, 4u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 15: (i32,i32,i32)->i32 : call_indirect (i32,i32,i32)->i32 selector=6
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_const); i32(c, 6);
            emit_ci(c, 6u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 16: (i32,i32,i32,i32)->i32 : call_indirect (4p)->i32 selector=7
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            for(::std::uint32_t i{}; i != 4u; ++i) { op(c, wasm_op::local_get); u32(c, i); }
            op(c, wasm_op::i32_const); i32(c, 7);
            emit_ci(c, 7u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 17: (i32,i32,i32,i32)->i32 : call_indirect (4p)->void selector=8 ; return 66
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            for(::std::uint32_t i{}; i != 4u; ++i) { op(c, wasm_op::local_get); u32(c, i); }
            op(c, wasm_op::i32_const); i32(c, 8);
            emit_ci(c, 8u, 0u);
            op(c, wasm_op::i32_const); i32(c, 66);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 18: (i32,i32,i32,i32,i32)->i32 : forces slow path (param_count>4) selector=9
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            for(::std::uint32_t i{}; i != 5u; ++i) { op(c, wasm_op::local_get); u32(c, i); }
            op(c, wasm_op::i32_const); i32(c, 9);
            emit_ci(c, 9u, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 19: (i32)->i32 : call_indirect + drop fusion candidate selector=2 ; return 77
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 2);
            emit_ci(c, 2u, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const); i32(c, 77);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // 20: (i32)->i32 (local i32 tmp): call_indirect + local.set fusion candidate selector=2 ; return tmp
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 2);
            emit_ci(c, 2u, 0u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr, typename... Fptrs>
    [[nodiscard]] bool contains_any_stacktop_ci(ByteStorage const& bc, Fptr f0, Fptrs... fs) noexcept
    {
        bool ok{bytecode_contains_fptr(bc, f0)};
        ((ok = ok || bytecode_contains_fptr(bc, fs)), ...);
        return ok;
    }

    [[nodiscard]] int test_translate_call_indirect() noexcept
    {
        g_trace_ci = (::std::getenv("UWVM2TEST_TRACE_CI") != nullptr);

        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +call_indirect_bridge;

        auto wasm = build_call_indirect_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_indirect");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;
        g_rt = ::std::addressof(rt);
        apply_active_element_segments(const_cast<runtime_module_t&>(rt));
        trace_ci("[ci] imported_funcs=", rt.imported_function_vec_storage.size(), " local_funcs=", rt.local_defined_function_vec_storage.size());
        trace_ci("[ci] imported_tables=", rt.imported_table_vec_storage.size(), " local_tables=", rt.local_defined_table_vec_storage.size());
        trace_ci("[ci] element_segs=", rt.local_defined_element_vec_storage.size());
        if(!rt.local_defined_element_vec_storage.empty())
        {
            auto const& e0 = rt.local_defined_element_vec_storage.index_unchecked(0uz).element;
            ::std::size_t func_n{};
            if(e0.funcidx_begin != nullptr && e0.funcidx_end != nullptr)
            {
                func_n = static_cast<::std::size_t>(e0.funcidx_end - e0.funcidx_begin);
            }
            trace_ci("[ci] elem0.table_idx=", static_cast<::std::size_t>(e0.table_idx),
                     " offset=", e0.offset,
                     " kind=", static_cast<unsigned>(e0.kind),
                     " dropped=", static_cast<unsigned>(e0.dropped),
                     " func_n=", func_n);
        }
        if(!rt.local_defined_table_vec_storage.empty())
        {
            auto const& t0 = rt.local_defined_table_vec_storage.index_unchecked(0uz);
            ::std::size_t const n{t0.elems.size() < 10uz ? t0.elems.size() : 10uz};
            for(::std::size_t i{}; i != n; ++i)
            {
                auto const& slot = t0.elems.index_unchecked(i);
                trace_ci("[ci] table0[", i, "] type=", static_cast<unsigned>(slot.type),
                         " ptr=", reinterpret_cast<::std::uintptr_t>(slot.storage.defined_ptr));
            }
        }

        // Mode C: tailcall + stacktop caching (scalar4 merged). Verify stacktop call_indirect fast paths + drop/local_set fusion.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            cop.curr_wasm_id = 0uz;
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            g_cm = ::std::addressof(cm);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr optable::uwvm_interpreter_stacktop_currpos_t c3{
                .i32_stack_top_curr_pos = 3uz,
                .i64_stack_top_curr_pos = 3uz,
                .f32_stack_top_curr_pos = 3uz,
                .f64_stack_top_curr_pos = 3uz,
                .v128_stack_top_curr_pos = SIZE_MAX,
            };
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c4{.i32_stack_top_curr_pos = 4uz,
                                                                      .i64_stack_top_curr_pos = 4uz,
                                                                      .f32_stack_top_curr_pos = 4uz,
                                                                      .f64_stack_top_curr_pos = 4uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c5{.i32_stack_top_curr_pos = 5uz,
                                                                      .i64_stack_top_curr_pos = 5uz,
                                                                      .f32_stack_top_curr_pos = 5uz,
                                                                      .f64_stack_top_curr_pos = 5uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c6{.i32_stack_top_curr_pos = 6uz,
                                                                      .i64_stack_top_curr_pos = 6uz,
                                                                      .f32_stack_top_curr_pos = 6uz,
                                                                      .f64_stack_top_curr_pos = 6uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};

            // caller0_i32: call_indirect_stacktop_i32 ParamCount=0 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(10).op.operands, f3, f4, f5, f6));
            }

            // caller0_void: call_indirect_stacktop_i32 ParamCount=0 Ret=void
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 0uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(11).op.operands, f3, f4, f5, f6));
            }

            // caller1_i32: call_indirect_stacktop_i32 ParamCount=1 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 1uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(12).op.operands, f3, f4, f5, f6));
            }

            // caller2_i32: call_indirect_stacktop_i32 ParamCount=2 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 2uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 2uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 2uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 2uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(14).op.operands, f3, f4, f5, f6));
            }

            // caller3_i32: call_indirect_stacktop_i32 ParamCount=3 Ret=wasm_i32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<opt, 3uz, wasm_i32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(15).op.operands, f3, f4, f5, f6));
            }

            // caller4_{i32,void}: selector+4 params do not fit in a 4-slot cache; expect slow path (`uwvmint_call_indirect`).
            {
                constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
                constexpr auto exp = optable::translate::get_uwvmint_call_indirect_fptr_from_tuple<opt>(curr, tuple);
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(17).op.operands, exp));
            }

            // caller1_drop: call_indirect_stacktop_i32_drop ParamCount=1
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<opt, 1uz>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(19).op.operands, f3, f4, f5, f6));
            }

            // caller1_lset: call_indirect_stacktop_i32_local_set ParamCount=1
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<opt, 1uz>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_stacktop_ci(cm.local_funcs.index_unchecked(20).op.operands, f3, f4, f5, f6));
            }
#endif

            using Runner = interpreter_runner<opt>;

#define UWVM2TEST_RUN_EXPECT_I32(FuncIdx, Params, Expected)                                                                  \
    do                                                                                                                       \
    {                                                                                                                        \
        ::std::size_t const func_idx__{(FuncIdx)};                                                                           \
        trace_ci("[ci] exec caller func_idx=", func_idx__);                                                                  \
        auto r__ = Runner::run(cm.local_funcs.index_unchecked(func_idx__),                                                   \
                               rt.local_defined_function_vec_storage.index_unchecked(func_idx__),                           \
                               (Params),                                                                                    \
                               nullptr,                                                                                      \
                               nullptr);                                                                                     \
        auto const got__ = load_i32(r__.results);                                                                            \
        trace_ci("[ci] exec caller func_idx=", func_idx__, " got=", got__, " expected=", static_cast<::std::int32_t>(Expected)); \
        UWVM2TEST_REQUIRE(got__ == static_cast<::std::int32_t>(Expected));                                                   \
    }                                                                                                                        \
    while(false)

            UWVM2TEST_RUN_EXPECT_I32(10uz, pack_no_params(), 11);
            UWVM2TEST_RUN_EXPECT_I32(11uz, pack_no_params(), 123);
            UWVM2TEST_RUN_EXPECT_I32(12uz, pack_i32(5), 12);
            UWVM2TEST_RUN_EXPECT_I32(14uz, pack_i32x2(0x1234, 0xff00), (0x1234 ^ 0xff00));
            UWVM2TEST_RUN_EXPECT_I32(15uz, pack_i32x3(1, 2, 3), 1);
            UWVM2TEST_RUN_EXPECT_I32(16uz, pack_i32x4(9, 8, 7, 6), 9);
            UWVM2TEST_RUN_EXPECT_I32(17uz, pack_i32x4(1, 2, 3, 4), 66);
            UWVM2TEST_RUN_EXPECT_I32(18uz, pack_i32x5(42, 1, 2, 3, 4), 42);
            UWVM2TEST_RUN_EXPECT_I32(19uz, pack_i32(123), 77);
            UWVM2TEST_RUN_EXPECT_I32(20uz, pack_i32(5), 12);

#undef UWVM2TEST_RUN_EXPECT_I32
        }

        // Mode A: byref - semantics smoke for call_indirect byref opfunc layout.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            cop.curr_wasm_id = 0uz;
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            g_cm = ::std::addressof(cm);

            using Runner = interpreter_runner<opt>;
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                                  rt.local_defined_function_vec_storage.index_unchecked(12),
                                                  pack_i32(5),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 12);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call_indirect();
}
