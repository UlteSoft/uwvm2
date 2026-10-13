/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <bit>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <string>
# include <string_view>
# include <vector>
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_unit/string.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasm_state
{
    // Copied Wasm DATA only. Neither a selector nor any logical identifier
    // authorizes native memory, an object-token lookup, execution or restore.
    inline constexpr ::std::uint32_t view_version{3u};
    inline constexpr ::std::uint64_t maximum_rows{64u}, maximum_objects{128u}, maximum_members{256u};
    inline constexpr ::std::size_t maximum_path{16u}, maximum_long_path{4096u};
    inline constexpr ::std::size_t maximum_reply_bytes{32768u};
    inline constexpr ::fast_io::string_view operand_snapshot_notice{
        "Note: Last Wasm safepoint snapshot; may differ from current native state."};
    enum class selection : unsigned char
    { locals, operands, globals, table, saved_parameters, controls, handlers, control_parameters, control_results, handler_parameters };
    [[nodiscard]] constexpr bool frame_selection(selection selected) noexcept
    { return selected != selection::globals && selected != selection::table; }
    [[nodiscard]] constexpr bool value_selection(selection selected) noexcept
    { return selected == selection::locals || selected == selection::operands || selected == selection::globals ||
        selected == selection::table || selected == selection::saved_parameters; }
    [[nodiscard]] constexpr bool declaration_selection(selection selected) noexcept
    { return selected == selection::control_parameters || selected == selection::control_results || selected == selection::handler_parameters; }
    [[nodiscard]] constexpr bool metadata_selection(selection selected) noexcept
    { return selected == selection::controls || selected == selection::handlers || declaration_selection(selected); }
    struct request
    {
        selection selected{selection::operands};
        ::std::uint64_t participant{}, module{}, frame{}, index{}, first{}, count{16u};
        // member_count==0 selects the existing root-row query. Otherwise first
        // is ONE original local/operand/global/table-element index and count
        // MUST be1. Follow path[0..path_size) only from this real current root;
        // each index denotes a struct field, array element, exception payload,
        // or the sole0 inner edge of a Wasm-created extern wrapper. Dense reply
        // object numbers and native tokens are NEVER accepted as selectors.
        ::std::uint64_t member_first{}, member_count{};
        ::std::array<::std::uint64_t, maximum_path> path{};
        unsigned char path_size{};
        // Complete immutable ORIGINAL index path copied from a session DATA
        // ledger. Labels identify only that path, NEVER dense/native objects.
        ::std::vector<::std::uint64_t> long_path{};
        ::std::uint64_t path_session{}, path_handle{};
        [[nodiscard]] constexpr bool compressed_path() const noexcept { return path_session!=0u; }
        [[nodiscard]] constexpr bool member_page() const noexcept { return member_count != 0u; }
    };
    // Failure diagnostics carry only scalar/fixed-index DATA. Copying a
    // nonempty owned long_path can allocate: NEVER do that in the noexcept
    // admission/failure path. The full path is copied only inside the actual
    // borrow's guarded success work and is never reconstructed from labels.
    [[nodiscard]] inline request unavailable_request(request const& input) noexcept
    {
        request out{};
        out.selected=input.selected;out.participant=input.participant;out.module=input.module;
        out.frame=input.frame;out.index=input.index;out.first=input.first;out.count=input.count;
        out.member_first=input.member_first;out.member_count=input.member_count;
        out.path=input.path;out.path_size=input.path_size;
        out.path_session=input.path_session;out.path_handle=input.path_handle;
        return out; // std::vector's empty default and owned move allocate NOTHING.
    }
    [[nodiscard]] constexpr bool valid(request const& item) noexcept
    {
        if((item.path_session==0u)!=(item.path_handle==0u) || item.long_path.size()>maximum_long_path ||
           (item.compressed_path() ? (!item.member_page() || item.path_size!=0u) : !item.long_path.empty()) ||
           item.path_size > maximum_path || item.member_count > maximum_rows ||
           (item.member_page() ? (item.count != 1u || !value_selection(item.selected)) : (item.member_first != 0u || item.path_size != 0u))) { return false; }
        for(::std::size_t index{item.path_size}; index != maximum_path; ++index)
        { if(item.path[index] != 0u) { return false; } }
        return item.participant != 0u && item.count != 0u && item.count <= maximum_rows &&
            static_cast<unsigned char>(item.selected) <= static_cast<unsigned char>(selection::handler_parameters) &&
            (item.selected == selection::table || declaration_selection(item.selected) || item.index == 0u) &&
            (frame_selection(item.selected) || item.frame == 0u);
    }
    enum class status : unsigned char
    {
        available, requires_llvm_jit_full, not_selected, requires_current_cooperative_stop,
        stale_stop_or_generation, missing_participant, incomplete_cohort, unavailable_activation,
        unavailable_typed_site, unavailable_gc_roots, unavailable_foreign_host_state,
        invalid_selection, out_of_range, resource_limit, allocation_failed, invalid_data
    };
    [[nodiscard]] constexpr ::fast_io::string_view status_text(status result) noexcept
    {
        switch(result)
        {
            case status::available: return "available";
            case status::requires_llvm_jit_full: return "requires LLVM-JIT full";
            case status::not_selected: return "typed value observation was not reserved before compilation";
            case status::requires_current_cooperative_stop: return "requires a current cooperative Wasm stop";
            case status::stale_stop_or_generation: return "stop or code generation is stale";
            case status::missing_participant: return "participant is not in the actual stopped cohort";
            case status::incomplete_cohort: return "the complete current participant cohort was not captured";
            case status::unavailable_activation: return "current activation ownership is unavailable";
            case status::unavailable_typed_site: return "this control or exception site has no complete typed packet";
            case status::unavailable_gc_roots: return "actual GC root and store borrowing is unavailable";
            case status::unavailable_foreign_host_state: return "foreign state cannot be borrowed safely";
            case status::invalid_selection: return "invalid Wasm state selector";
            case status::out_of_range: return "Wasm index or page is out of range";
            case status::resource_limit: return "bounded Wasm state query exceeded its quota";
            case status::allocation_failed: return "Wasm state copy allocation failed";
            case status::invalid_data: return "invalid copied Wasm state data";
        }
        return "invalid copied Wasm state status";
    }
    enum class value_kind : unsigned char { i32, i64, f32, f64, v128, reference };
    struct declaration
    {
        value_kind kind{value_kind::i32};
        ::std::int64_t heap{}; // Exact s33 abstract heap code or module type index.
        bool nullable{}, known{};
        // Exact origin of a declared defined heap index. An imported value's
        // declaration origin can differ from its actual object's store module.
        ::std::uint64_t type_module{};
    };
    enum class reference_kind : unsigned char
    { null, function, host_reference, external_wrapper, structure, array, exception, i31 };
    struct reference
    {
        reference_kind kind{reference_kind::null};
        ::std::uint64_t object{}; // Dense query-local ID; never a host token/address.
        ::std::uint64_t function_module{}, function_index{};
        ::std::uint32_t i31_bits{};
        bool function_identity_available{};
    };
    enum class unavailable_reason : unsigned char
    { none, not_initialized, incomplete_typed_site, unresolved_reference, unavailable_root_borrow, foreign_host_state };
    [[nodiscard]] constexpr ::fast_io::string_view unavailable_text(unavailable_reason reason) noexcept
    {
        switch(reason)
        {
            case unavailable_reason::none: return "no current value";
            case unavailable_reason::not_initialized: return "local is not initialized";
            case unavailable_reason::incomplete_typed_site: return "typed site is incomplete";
            case unavailable_reason::unresolved_reference: return "reference identity could not be resolved";
            case unavailable_reason::unavailable_root_borrow: return "actual reference root borrow is unavailable";
            case unavailable_reason::foreign_host_state: return "opaque host payload is not inspectable";
        }
        return "invalid unavailable reason";
    }
    struct value
    {
        declaration type{};
        // Canonical LE numeric/vector bits. References NEVER use these bytes.
        ::std::array<::std::byte, 16u> bits{};
        reference ref{};
        unavailable_reason unavailable{unavailable_reason::none};
        unsigned char packed_bits{}; // 0, 8 or 16, only for an i32 GC storage field.
        bool available{};
    };
    struct row
    {
        ::std::uint64_t index{};
        value data{};
        bool mutable_storage{}, mutability_known{};
    };
    enum class object_kind : unsigned char { structure, array, exception, host_reference, external_wrapper };
    struct object
    {
        ::std::uint64_t identifier{}, module{}, type_index{}, total_members{};
        ::std::uint64_t first_member{}, next_member{};
        object_kind kind{object_kind::structure};
        ::std::uint64_t tag_module{}, tag_index{};
        ::std::vector<row> members{};
        bool tag_identity_available{}, members_truncated{}, has_more_members{};
    };
    // Exact lexical DATA from the current authenticated sealed site. These
    // records contain no native catch object, PC, owner, restore entry or token.
    // Handler order is the producer's inner-to-outer source clause order; the
    // flat list deliberately does not guess which try owns a clause.
    enum class control_kind : unsigned char { function, block, loop, if_then, if_else };
    struct control
    {
        ::std::uint64_t index{}, entry_offset{}, end_offset{}, outer_operand_height{};
        ::std::uint64_t first_saved_parameter{}, saved_parameter_count{}, parameter_count{}, result_count{};
        control_kind kind{control_kind::function};
    };
    struct handler
    {
        ::std::uint64_t index{}, tag_index{}, target_control{}, target_offset{}, parameter_count{};
        bool catch_all{}, with_reference{};
    };
    struct declared_row { ::std::uint64_t index{}; declaration type{}; };
    struct view
    {
        status result{status::not_selected};
        request requested{};
        ::std::uint64_t runtime_epoch{}, module{}, total_values{}, first{};
        ::std::uint64_t selected_object{}; // DISPLAY only: terminal rooted path result.
        ::std::vector<row> rows{};
        ::std::vector<object> objects{};
        ::std::vector<control> controls{};
        ::std::vector<handler> handlers{};
        ::std::vector<declared_row> declarations{};
        bool rows_truncated{}, graph_truncated{};
        // A view has no native pointers, pause tickets or retained runtime
        // owners. A later command must authenticate the actual stop again.
        [[nodiscard]] constexpr bool snapshot_or_restore_authority() const noexcept { return false; }
    };
    namespace details
    {
        [[nodiscard]] constexpr bool valid_declaration(declaration const& type) noexcept
        {
            if(!type.known) { return false; }
            if(type.kind == value_kind::reference)
            { return type.heap <= 0xffffffffll && ((type.heap >= -23 && type.heap <= -12) || type.heap >= 0) &&
                (type.heap >= 0 || type.type_module == 0u); }
            return type.kind <= value_kind::v128 && type.heap == 0 && !type.nullable && type.type_module == 0u;
        }
        [[nodiscard]] constexpr bool kind_valid(value_kind kind) noexcept
        {
            return kind == value_kind::i32 || kind == value_kind::i64 || kind == value_kind::f32 ||
                kind == value_kind::f64 || kind == value_kind::v128 || kind == value_kind::reference;
        }
        [[nodiscard]] constexpr bool reference_kind_valid(reference_kind kind) noexcept
        {
            return kind == reference_kind::null || kind == reference_kind::function ||
                kind == reference_kind::host_reference || kind == reference_kind::external_wrapper ||
                kind == reference_kind::structure || kind == reference_kind::array ||
                kind == reference_kind::exception || kind == reference_kind::i31;
        }
        [[nodiscard]] constexpr bool object_kind_valid(object_kind kind) noexcept
        {
            return kind == object_kind::structure || kind == object_kind::array || kind == object_kind::exception ||
                kind == object_kind::host_reference || kind == object_kind::external_wrapper;
        }
        [[nodiscard]] inline bool valid_value(value const& item, view const& owner) noexcept
        {
            if(!kind_valid(item.type.kind) || (item.available && !item.type.known)) { return false; }
            if(item.type.known && item.type.kind == value_kind::reference &&
                ((item.type.heap < -23 || item.type.heap > 0xffffffffll) ||
                 (item.type.heap < 0 && item.type.heap > -12))) { return false; }
            auto const width{!item.available || item.type.kind == value_kind::reference ? 0u :
                item.type.kind == value_kind::v128 ? 16u :
                item.type.kind == value_kind::i64 || item.type.kind == value_kind::f64 ? 8u : 4u};
            for(::std::size_t index{width}; index != item.bits.size(); ++index)
            { if(item.bits[index] != ::std::byte{}) { return false; } }
            if(item.packed_bits != 0u && (item.type.kind != value_kind::i32 ||
                (item.packed_bits != 8u && item.packed_bits != 16u))) { return false; }
            if(!item.available || item.type.kind != value_kind::reference) { return true; }
            if(!reference_kind_valid(item.ref.kind) || item.packed_bits != 0u ||
                (item.ref.kind != reference_kind::i31 && item.ref.i31_bits != 0u) ||
                (item.ref.kind != reference_kind::function && (item.ref.function_module != 0u ||
                 item.ref.function_index != 0u || item.ref.function_identity_available))) { return false; }
            if(item.ref.kind == reference_kind::null)
            { return item.type.nullable && item.ref.object == 0u && !item.ref.function_identity_available; }
            if(item.ref.kind == reference_kind::i31)
            { return item.ref.i31_bits <= 0x7fffffffu && item.ref.object == 0u && !item.ref.function_identity_available; }
            if(item.ref.kind == reference_kind::function)
            { return item.ref.object == 0u; }
            if(item.ref.object == 0u || item.ref.object > owner.objects.size()) { return false; }
            // [owned dense objects ... object-1 < size] end
            // [safe                                  ] unsafe (one-past)
            //  ^^ No logical ID is ever converted to a native pointer/token.
            auto const& target{owner.objects[static_cast<::std::size_t>(item.ref.object - 1u)]};
            return (item.ref.kind == reference_kind::structure && target.kind == object_kind::structure) ||
                (item.ref.kind == reference_kind::array && target.kind == object_kind::array) ||
                (item.ref.kind == reference_kind::exception && target.kind == object_kind::exception) ||
                (item.ref.kind == reference_kind::host_reference && target.kind == object_kind::host_reference) ||
                (item.ref.kind == reference_kind::external_wrapper && target.kind == object_kind::external_wrapper);
        }
        template<unsigned Bits, typename T> [[nodiscard]] inline bool read_numeric(value const& item, T& result) noexcept
        {
            static_assert(Bits == 32u || Bits == 64u);
            static_assert(Bits / 8u <= ::std::tuple_size_v<decltype(item.bits)>);
            auto const* first{reinterpret_cast<unsigned char const*>(item.bits.data())};
            // [owned numeric bytes ... Bits/8 <= 16] end
            // [safe                               ] unsafe (one-past)
            //  ^^ first; the fixed array extent is checked before end formation.
            auto const* end{first + Bits / 8u};
            auto const parsed{::fast_io::parse_by_scan(first, end, ::fast_io::mnp::le_get<Bits>(result))};
            return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
        }
        template<typename Output> inline void print_heap(Output output, ::std::int64_t heap)
        {
            switch(heap)
            {
                case -12: ::fast_io::io::print(output, "noexn"); break;
                case -13: ::fast_io::io::print(output, "nofunc"); break;
                case -14: ::fast_io::io::print(output, "noextern"); break;
                case -15: ::fast_io::io::print(output, "none"); break;
                case -16: ::fast_io::io::print(output, "func"); break;
                case -17: ::fast_io::io::print(output, "extern"); break;
                case -18: ::fast_io::io::print(output, "any"); break;
                case -19: ::fast_io::io::print(output, "eq"); break;
                case -20: ::fast_io::io::print(output, "i31"); break;
                case -21: ::fast_io::io::print(output, "struct"); break;
                case -22: ::fast_io::io::print(output, "array"); break;
                case -23: ::fast_io::io::print(output, "exn"); break;
                default: ::fast_io::io::print(output, "type-index=", ::fast_io::mnp::dec(heap)); break;
            }
        }
        template<typename Output> inline void print_type(Output output, declaration const& type)
        {
            if(!type.known) { ::fast_io::io::print(output, "unknown"); return; }
            switch(type.kind)
            {
                case value_kind::i32: ::fast_io::io::print(output, "i32"); break;
                case value_kind::i64: ::fast_io::io::print(output, "i64"); break;
                case value_kind::f32: ::fast_io::io::print(output, "f32"); break;
                case value_kind::f64: ::fast_io::io::print(output, "f64"); break;
                case value_kind::v128: ::fast_io::io::print(output, "v128"); break;
                case value_kind::reference:
                    ::fast_io::io::print(output, "(ref ");
                    if(type.nullable) { ::fast_io::io::print(output, "null "); }
                    print_heap(output, type.heap);
                    if(type.heap >= 0) { ::fast_io::io::print(output, " module=", ::fast_io::mnp::dec(type.type_module)); }
                    ::fast_io::io::print(output, ::fast_io::mnp::chvw(')')); break;
            }
        }
        template<typename Output> inline void print_value(Output output, value const& item)
        {
            print_type(output, item.type); ::fast_io::io::print(output, " = ");
            if(!item.available)
            {
                ::fast_io::io::print(output, "unavailable (", unavailable_text(item.unavailable), ::fast_io::mnp::chvw(')'));
                return;
            }
            switch(item.type.kind)
            {
                case value_kind::i32:
                case value_kind::f32:
                {
                    ::std::uint32_t bits{};
                    if(!read_numeric<32u>(item, bits)) { ::fast_io::io::print(output, "invalid numeric data"); return; }
                    if(item.type.kind == value_kind::f32) { ::fast_io::io::print(output, "bits=0x", ::fast_io::mnp::hex<false, true>(bits)); }
                    else if(item.packed_bits != 0u)
                    {
                        auto const mask{item.packed_bits == 8u ? 0xffu : 0xffffu};
                        ::fast_io::io::print(output, "packed-u", ::fast_io::mnp::dec(static_cast<unsigned>(item.packed_bits)),
                            ::fast_io::mnp::chvw('='), ::fast_io::mnp::dec(bits & mask));
                    }
                    else { ::fast_io::io::print(output, ::fast_io::mnp::dec(::std::bit_cast<::std::int32_t>(bits))); }
                    break;
                }
                case value_kind::i64:
                case value_kind::f64:
                {
                    ::std::uint64_t bits{};
                    if(!read_numeric<64u>(item, bits)) { ::fast_io::io::print(output, "invalid numeric data"); return; }
                    if(item.type.kind == value_kind::f64) { ::fast_io::io::print(output, "bits=0x", ::fast_io::mnp::hex<false, true>(bits)); }
                    else { ::fast_io::io::print(output, ::fast_io::mnp::dec(::std::bit_cast<::std::int64_t>(bits))); }
                    break;
                }
                case value_kind::v128:
                    ::fast_io::io::print(output, "bytes=");
                    for(auto const byte : item.bits)
                    { ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(::std::to_integer<unsigned char>(byte))); }
                    break;
                case value_kind::reference:
                    switch(item.ref.kind)
                    {
                        case reference_kind::null: ::fast_io::io::print(output, "null"); break;
                        case reference_kind::i31:
                        {
                            auto const signed_bits{(item.ref.i31_bits ^ 0x40000000u) - 0x40000000u};
                            ::fast_io::io::print(output, "i31 signed=", ::fast_io::mnp::dec(::std::bit_cast<::std::int32_t>(signed_bits)),
                                " unsigned=", ::fast_io::mnp::dec(item.ref.i31_bits)); break;
                        }
                        case reference_kind::function:
                            if(item.ref.function_identity_available)
                            { ::fast_io::io::print(output, "function module=", ::fast_io::mnp::dec(item.ref.function_module),
                                " index=", ::fast_io::mnp::dec(item.ref.function_index)); }
                            else { ::fast_io::io::print(output, "function identity unavailable"); }
                            break;
                        case reference_kind::structure: ::fast_io::io::print(output, "struct #", ::fast_io::mnp::dec(item.ref.object)); break;
                        case reference_kind::array: ::fast_io::io::print(output, "array #", ::fast_io::mnp::dec(item.ref.object)); break;
                        case reference_kind::exception: ::fast_io::io::print(output, "exception #", ::fast_io::mnp::dec(item.ref.object)); break;
                        case reference_kind::host_reference: ::fast_io::io::print(output, "opaque host-reference #", ::fast_io::mnp::dec(item.ref.object)); break;
                        case reference_kind::external_wrapper: ::fast_io::io::print(output, "extern-wrapper #", ::fast_io::mnp::dec(item.ref.object)); break;
                    }
                    break;
            }
        }
    }
    [[nodiscard]] inline bool valid(view const& result) noexcept
    {
        if(static_cast<unsigned char>(result.result) > static_cast<unsigned char>(status::invalid_data)) { return false; }
        if(result.result != status::available) { return result.rows.empty() && result.objects.empty() && result.controls.empty() &&
            result.handlers.empty() && result.declarations.empty() && result.selected_object == 0u; }
        if(metadata_selection(result.requested.selected))
        {
            if(!valid(result.requested) || !result.rows.empty() || !result.objects.empty() || result.selected_object != 0u || result.graph_truncated ||
               result.first != result.requested.first || result.first > result.total_values) { return false; }
            // Each owned vector is bounded BEFORE adding their row counts;
            // malformed copied DATA cannot wrap the aggregate size.
            if(result.controls.size()>maximum_rows || result.handlers.size()>maximum_rows || result.declarations.size()>maximum_rows) { return false; }
            auto const count{result.controls.size()+result.handlers.size()+result.declarations.size()};
            if(count > maximum_rows || count > result.requested.count || count > result.total_values-result.first ||
               count != (result.requested.count < result.total_values-result.first ? result.requested.count : result.total_values-result.first) ||
               result.rows_truncated != (count != result.total_values-result.first)) { return false; }
            if((result.requested.selected != selection::controls && !result.controls.empty()) ||
               (result.requested.selected != selection::handlers && !result.handlers.empty()) ||
               (!declaration_selection(result.requested.selected) && !result.declarations.empty())) { return false; }
            for(::std::size_t i{}; i != result.controls.size(); ++i)
            {
                auto const& item{result.controls[i]};
                if(item.index != result.first+i || item.kind > control_kind::if_else || item.entry_offset > item.end_offset ||
                   item.saved_parameter_count > item.parameter_count || item.saved_parameter_count >
                       (::std::numeric_limits<::std::uint64_t>::max)()-item.first_saved_parameter ||
                   (item.kind != control_kind::if_then && item.kind != control_kind::if_else && item.saved_parameter_count != 0u) ||
                   (item.kind == control_kind::function && item.index != 0u)) { return false; }
            }
            for(::std::size_t i{}; i != result.handlers.size(); ++i)
            { auto const& item{result.handlers[i]}; if(item.index != result.first+i ||
                (item.catch_all && (item.tag_index != 0u || item.parameter_count != 0u))) { return false; } }
            for(::std::size_t i{}; i != result.declarations.size(); ++i)
            { if(result.declarations[i].index != result.first+i || !details::valid_declaration(result.declarations[i].type)) { return false; } }
            return true;
        }
        if(!result.controls.empty() || !result.handlers.empty() || !result.declarations.empty()) { return false; }
        if(!valid(result.requested) || result.rows.size() > maximum_rows || result.rows.size() > result.requested.count ||
           result.objects.size() > maximum_objects || result.first != result.requested.first || result.first > result.total_values ||
           result.rows.size() > result.total_values - result.first ||
           (result.requested.member_page() ? (result.rows.size() != 1u || result.selected_object == 0u ||
               result.selected_object > result.objects.size() || result.objects.size() >
               (result.requested.compressed_path() ? 2u : static_cast<::std::size_t>(result.requested.path_size)+1u) + result.requested.member_count) :
               result.selected_object != 0u)) { return false; }
        if(result.requested.compressed_path())
        {
            // The real long-path reader retains original root1 and terminal1/2;
            // intermediate nodes are not exported as a fabricated full graph.
            auto const& root{result.rows.front().data};
            if(result.selected_object>2u || !root.available || root.type.kind!=value_kind::reference || root.ref.object!=1u ||
               (root.ref.kind!=reference_kind::structure && root.ref.kind!=reference_kind::array &&
                root.ref.kind!=reference_kind::exception && root.ref.kind!=reference_kind::external_wrapper)) { return false; }
        }
        for(::std::size_t index{}; index != result.rows.size(); ++index)
        {
            if(result.rows[index].index != result.first + index || !details::valid_value(result.rows[index].data, result)) { return false; }
        }
        ::std::size_t members{};
        for(::std::size_t index{}; index != result.objects.size(); ++index)
        {
            auto const& item{result.objects[index]};
            if(item.identifier != index + 1u || !details::object_kind_valid(item.kind) ||
                item.members.size() > maximum_members - members || item.first_member > item.total_members ||
                item.members.size() > item.total_members - item.first_member ||
                item.next_member != item.first_member + item.members.size() ||
                item.has_more_members != (item.next_member != item.total_members) ||
                item.members_truncated != (item.first_member != 0u || item.members.size() != item.total_members) ||
                (item.kind == object_kind::host_reference && (item.total_members != 0u || !item.members.empty())) ||
                (item.kind == object_kind::external_wrapper && item.total_members != 1u)) { return false; }
            if(result.requested.member_page())
            {
                if(item.identifier == result.selected_object)
                {
                    if(item.kind == object_kind::host_reference || item.first_member != result.requested.member_first ||
                       item.members.size() != (result.requested.member_count < item.total_members - item.first_member ?
                           result.requested.member_count : item.total_members - item.first_member)) { return false; }
                }
                else if(!item.members.empty() || item.first_member != 0u) { return false; }
            }
            members += item.members.size();
            for(::std::size_t member{}; member != item.members.size(); ++member)
            {
                if(item.members[member].index != item.first_member + member ||
                   (!item.members[member].mutability_known && item.members[member].mutable_storage) ||
                   !details::valid_value(item.members[member].data, result)) { return false; }
            }
        }
        // GC struct/array cycles are valid. Wrapper-only cycles are not runtime
        // ref.extern values: follow at most the bounded dense object population.
        for(auto const& origin : result.objects)
        {
            if(origin.kind != object_kind::external_wrapper) { continue; }
            auto const* current{__builtin_addressof(origin)};
            for(::std::size_t depth{}; depth != result.objects.size(); ++depth)
            {
                if(current->kind != object_kind::external_wrapper || current->members.empty()) { break; }
                auto const& wrapped{current->members.front().data};
                if(!wrapped.available || wrapped.type.kind != value_kind::reference ||
                    wrapped.ref.kind != reference_kind::external_wrapper) { break; }
                // [validated dense wrapper ID 1..objects.size()] end
                // [safe                                        ] unsafe (one-past)
                //  ^^ All values were preflighted before this bounded graph walk.
                current = __builtin_addressof(result.objects[static_cast<::std::size_t>(wrapped.ref.object - 1u)]);
                if(depth + 1u == result.objects.size()) { return false; }
            }
        }
        return true;
    }
    [[nodiscard]] inline ::std::string format(view const& result)
    {
        if(!valid(result)) { return ::fast_io::concat_std("error: invalid bounded Wasm state result\n"); }
        if(result.result != status::available)
        { return ::fast_io::concat_std("Wasm state unavailable: ", status_text(result.result), ::fast_io::mnp::chvw('\n')); }
        ::std::string text{}; ::fast_io::ostring_ref_std output{__builtin_addressof(text)};
        ::fast_io::io::print(output, "Wasm state thread=", ::fast_io::mnp::dec(result.requested.participant),
            " module=", ::fast_io::mnp::dec(result.module), " epoch=", ::fast_io::mnp::dec(result.runtime_epoch),
            " first=", ::fast_io::mnp::dec(result.first), " total=", ::fast_io::mnp::dec(result.total_values), ::fast_io::mnp::chvw('\n'));
        if(result.requested.selected == selection::operands)
        { ::fast_io::io::print(output, operand_snapshot_notice, ::fast_io::mnp::chvw('\n')); }
        if(result.requested.member_page())
        {
            ::fast_io::io::print(output, "Wasm members object=", ::fast_io::mnp::dec(result.selected_object),
                " first=", ::fast_io::mnp::dec(result.requested.member_first), " count=", ::fast_io::mnp::dec(result.requested.member_count), " path=");
            if(result.requested.compressed_path())
            { ::fast_io::io::print(output,"@",::fast_io::mnp::dec(result.requested.path_session),":",
                ::fast_io::mnp::dec(result.requested.path_handle)," depth=",::fast_io::mnp::dec(result.requested.long_path.size())); }
            else if(result.requested.path_size == 0u) { ::fast_io::io::print(output, ::fast_io::mnp::chvw('-')); }
            for(::std::size_t index{}; index != result.requested.path_size; ++index)
            {
                // [valid fixed path0...path_size<=16] end
                // [safe] index<path_size<=array.size BEFORE each decimal read.
                if(index != 0u) { ::fast_io::io::print(output, ::fast_io::mnp::chvw(',')); }
                ::fast_io::io::print(output, ::fast_io::mnp::dec(result.requested.path[index]));
            }
            ::fast_io::io::print(output, ::fast_io::mnp::chvw('\n'));
        }
        if(metadata_selection(result.requested.selected))
        {
            // The entire requested lexical page must be present. At most64
            // fixed scalar rows OR64 bounded Core3 types are formatted; no
            // unbounded tuple, guest label or native address enters this view.
            for(auto const& item : result.controls)
            {
                ::fast_io::string_view kind{};
                switch(item.kind)
                {
                    case control_kind::function: kind="function"; break;
                    case control_kind::block: kind="block"; break;
                    case control_kind::loop: kind="loop"; break;
                    case control_kind::if_then: kind="if-then"; break;
                    case control_kind::if_else: kind="if-else"; break;
                }
                ::fast_io::io::print(output,"control ",::fast_io::mnp::dec(item.index)," kind=",kind,
                    " entry=",::fast_io::mnp::dec(item.entry_offset)," end=",::fast_io::mnp::dec(item.end_offset),
                    " height=",::fast_io::mnp::dec(item.outer_operand_height)," saved-first=",::fast_io::mnp::dec(item.first_saved_parameter),
                    " saved-count=",::fast_io::mnp::dec(item.saved_parameter_count)," params=",::fast_io::mnp::dec(item.parameter_count),
                    " results=",::fast_io::mnp::dec(item.result_count),::fast_io::mnp::chvw('\n'));
            }
            for(auto const& item : result.handlers)
            {
                ::fast_io::io::print(output,"handler ",::fast_io::mnp::dec(item.index)," catch=",
                    item.catch_all ? (item.with_reference ? ::fast_io::string_view{"catch-all-ref"} : ::fast_io::string_view{"catch-all"}) : (item.with_reference ? ::fast_io::string_view{"catch-ref"} : ::fast_io::string_view{"catch"}),
                    " tag-index=",::fast_io::mnp::dec(item.tag_index)," target-control=",::fast_io::mnp::dec(item.target_control),
                    " target-offset=",::fast_io::mnp::dec(item.target_offset)," params=",::fast_io::mnp::dec(item.parameter_count),
                    ::fast_io::mnp::chvw('\n'));
            }
            for(auto const& item : result.declarations)
            {
                ::fast_io::io::print(output,result.requested.selected == selection::handler_parameters ? ::fast_io::string_view{"handler "} : ::fast_io::string_view{"control "},
                    ::fast_io::mnp::dec(result.requested.index),result.requested.selected == selection::control_results ? ::fast_io::string_view{" result "} : ::fast_io::string_view{" parameter "},
                    ::fast_io::mnp::dec(item.index),::fast_io::mnp::chvw(' '));
                details::print_type(output,item.type); ::fast_io::io::print(output,::fast_io::mnp::chvw('\n'));
            }
            if(result.rows_truncated)
            { ::fast_io::io::print(output,"Wasm layout page next=",::fast_io::mnp::dec(result.first+result.controls.size()+result.handlers.size()+result.declarations.size()),::fast_io::mnp::chvw('\n')); }
            if(text.size() > maximum_reply_bytes-256u)
            { return ::fast_io::concat_std("Wasm state unavailable: ",status_text(status::resource_limit),::fast_io::mnp::chvw('\n')); }
            return text;
        }
        auto append = [&](::std::string const& row_text)
        {
            // [owned bounded formatted prefix][new owned row] reply_end
            // [safe                                         ] subtract BEFORE addition/append;
            //  ^^ no terminal/guest labels, native pointers or runtime tokens occur here.
            if(text.size() > maximum_reply_bytes - 256u || row_text.size() > maximum_reply_bytes - 256u - text.size()) { return false; }
            ::fast_io::io::print(output, ::std::string_view{row_text}); return true;
        };
        ::std::size_t shown_rows{}, shown_objects{};
        for(auto const& item : result.rows)
        {
            ::std::string row_text{}; ::fast_io::ostring_ref_std row_output{__builtin_addressof(row_text)};
            switch(result.requested.selected)
            {
                case selection::locals: ::fast_io::io::print(row_output, "local "); break;
                case selection::operands: ::fast_io::io::print(row_output, "operand "); break;
                case selection::saved_parameters: ::fast_io::io::print(row_output, "saved-parameter "); break;
                default: return ::fast_io::concat_std("error: invalid bounded Wasm value selection\n");
                case selection::globals: ::fast_io::io::print(row_output, "global "); break;
                case selection::table: ::fast_io::io::print(row_output, "table ", ::fast_io::mnp::dec(result.requested.index), " element "); break;
            }
            ::fast_io::io::print(row_output, ::fast_io::mnp::dec(item.index), ::fast_io::mnp::chvw(' '));
            details::print_value(row_output, item.data); ::fast_io::io::print(row_output, ::fast_io::mnp::chvw('\n'));
            if(!append(row_text))
            {
                if(result.requested.member_page())
                {
                    // A requested original-root page must remain COMPLETE.
                    // Even a future longer value format must not expose a
                    // prefix with unresolved dense object references.
                    return ::fast_io::concat_std("Wasm state unavailable: ", status_text(status::resource_limit), ::fast_io::mnp::chvw('\n'));
                }
                break;
            }
            ++shown_rows;
        }
        // In member-page mode emit the selected object's actual page FIRST;
        // child/intermediate metadata cannot consume the page's reply budget.
        for(::std::size_t ordinal{}; ordinal != result.objects.size(); ++ordinal)
        {
            auto index{ordinal};
            if(result.requested.member_page())
            {
                auto const selected{static_cast<::std::size_t>(result.selected_object - 1u)};
                index = ordinal == 0u ? selected : ordinal <= selected ? ordinal - 1u : ordinal;
            }
            auto const& item{result.objects[index]};
            ::std::string row_text{}; ::fast_io::ostring_ref_std row_output{__builtin_addressof(row_text)};
            ::fast_io::io::print(row_output, "object #", ::fast_io::mnp::dec(item.identifier), ::fast_io::mnp::chvw(' '));
            switch(item.kind)
            {
                case object_kind::structure: ::fast_io::io::print(row_output, "struct module=", ::fast_io::mnp::dec(item.module), " type=", ::fast_io::mnp::dec(item.type_index)); break;
                case object_kind::array: ::fast_io::io::print(row_output, "array module=", ::fast_io::mnp::dec(item.module), " type=", ::fast_io::mnp::dec(item.type_index)); break;
                case object_kind::exception:
                    ::fast_io::io::print(row_output, "exception");
                    if(item.tag_identity_available)
                    { ::fast_io::io::print(row_output, " tag-module=", ::fast_io::mnp::dec(item.tag_module), " tag=", ::fast_io::mnp::dec(item.tag_index)); }
                    else { ::fast_io::io::print(row_output, " tag identity unavailable"); }
                    break;
                case object_kind::host_reference: ::fast_io::io::print(row_output, "opaque host-reference (payload unavailable)"); break;
                case object_kind::external_wrapper: ::fast_io::io::print(row_output, "extern-wrapper"); break;
            }
            ::fast_io::io::print(row_output, " members=", ::fast_io::mnp::dec(item.total_members),
                " first=", ::fast_io::mnp::dec(item.first_member), " next=", ::fast_io::mnp::dec(item.next_member),
                " more=", item.has_more_members ? ::fast_io::string_view{"yes"} : ::fast_io::string_view{"no"}, ::fast_io::mnp::chvw('\n'));
            for(auto const& member : item.members)
            {
                ::fast_io::io::print(row_output, "  ", ::fast_io::mnp::dec(member.index), ::fast_io::mnp::chvw(' '));
                details::print_value(row_output, member.data);
                if(member.mutability_known) { ::fast_io::io::print(row_output, member.mutable_storage ? ::fast_io::string_view{" mutable"} : ::fast_io::string_view{" immutable"}); }
                ::fast_io::io::print(row_output, ::fast_io::mnp::chvw('\n'));
            }
            if(result.requested.member_page() && item.identifier != result.selected_object && item.total_members != 0u)
            { ::fast_io::io::print(row_output, "  metadata only; expand through the original root and member path\n"); }
            else if(item.has_more_members) { ::fast_io::io::print(row_output, "  member page; continue with original root/path and next\n"); }
            else if(item.first_member != 0u) { ::fast_io::io::print(row_output, "  final member page; earlier members are outside this window\n"); }
            if(!append(row_text))
            {
                if(result.requested.member_page())
                {
                    // Discard EVERY partial field/object label before return;
                    // detached IDs in a truncated prefix have no query value.
                    return ::fast_io::concat_std("Wasm state unavailable: ", status_text(status::resource_limit), ::fast_io::mnp::chvw('\n'));
                }
                break;
            }
            ++shown_objects;
        }
        if(result.rows_truncated || result.graph_truncated || shown_rows != result.rows.size() || shown_objects != result.objects.size())
        { ::fast_io::io::print(output, "Wasm state truncated: rows=", ::fast_io::mnp::dec(shown_rows),
            " objects=", ::fast_io::mnp::dec(shown_objects), ::fast_io::mnp::chvw('\n')); }
        return text;
    }
}
