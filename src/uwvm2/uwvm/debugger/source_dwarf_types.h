/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <optional>
# include <span>
# include <string>
# include <string_view>
# include <vector>
# include <utility>
// The hosted umbrella conditionally enters its stdstring unit only when the
// standard string header was already present. Enter the unit explicitly after
// the owned string types so a prior umbrella include cannot suppress its hooks.
# include <fast_io_unit/string.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    // Cold metadata only. These inputs/records grant no execution, source-owner,
    // native-address, stopped-frame or memory-reading authority. Code addresses
    // are offsets in the Wasm Code section CONTENT, never host pointers.
    struct section
    {
        ::std::string_view name{};
        // The caller owns a live, correctly bounded custom payload AFTER its
        // section name for the entire parse call. Parsing copies accepted data.
        ::std::span<::std::byte const> payload{};
    };
    struct image_input
    {
        ::std::span<section const> sections{};
        ::std::uint64_t code_section_content_size{};
        ::std::uint8_t address_bytes{4u}; // Wasm32/64, independent of host width.
    };
    struct limits
    {
        ::std::size_t max_sections{64u}, max_section_bytes{1024u * 1024u};
        ::std::size_t max_total_bytes{8u * 1024u * 1024u};
        ::std::size_t max_units{256u}, max_unit_bytes{256u * 1024u};
        ::std::size_t max_dies{65536u}, max_depth{64u};
        ::std::size_t max_string_bytes{4096u}, max_total_string_bytes{1024u * 1024u};
        ::std::size_t max_reference_hops{16u}, max_attributes{1024u * 1024u};
        ::std::size_t max_ranges{65536u}, max_locations{65536u}, max_line_rows{65536u};
        ::std::size_t max_expression_bytes{256u}, max_total_expression_bytes{1024u * 1024u};
        ::std::size_t max_types{65536u}, max_type_edges{65536u}, max_array_dimensions{64u};
        ::std::size_t max_location_pieces{64u}, max_composite_bits{524288u};
    };
    enum class error : unsigned
    {
        none, missing_sections, malformed, unsupported_dwarf, unsupported_external,
        limit_exceeded, allocation_failure
    };
    struct budget
    {
        ::std::size_t bytes{}, units{}, dies{}, attributes{}, strings{}, ranges{}, locations{}, expressions{}, line_rows{}, type_edges{};
        [[nodiscard]] static constexpr bool charge(::std::size_t amount, ::std::size_t cap, ::std::size_t& used) noexcept
        {
            if(used > cap || amount > cap - used) { return false; }
            used += amount;
            return true;
        }
    };
    struct line_directory_record
    {
        ::std::uint64_t line_unit_offset{};
        ::std::string directory{}; // owned, bounded DW_AT_comp_dir; metadata only.
    };
    struct die_key
    {
        ::std::uint64_t unit{}, offset{};
        [[nodiscard]] friend constexpr bool operator==(die_key const&, die_key const&) noexcept = default;
    };
    inline constexpr ::std::size_t no_record{(::std::numeric_limits<::std::size_t>::max)()};
    struct code_range { ::std::uint64_t begin{}, end{}; };
    // Owned cooperative numeric snapshot slots, authenticated by the runtime
    // before any source query. Their bytes are native carrier representation;
    // they are never generated-frame pointers or guest read authority.
    struct copied_numeric_local
    {
        ::std::uint_least8_t wasm_type{};
        ::std::array<::std::byte, 16u> bytes{};
        // Original slot/type are retained even if the runtime did not prove
        // initialization at this stop. Default construction is unavailable;
        // zeros are not an initialized value, address or read capability.
        bool available{};
    };
    enum class plan_kind : unsigned { unavailable, wasm_local_value, constant_value, frame_relative_offset, wasm_local_frame_base, composite_value, absolute_guest_offset };
    enum class unavailable_reason : unsigned
    {
        none, no_location, empty_location, unsupported_expression, unsupported_wasm_location,
        indirect_or_ambiguous_local, value_too_large, malformed_expression, expression_limit, allocation_failure
    };
    enum class position_role : unsigned { variable, frame_base };
    enum class wasm_location_space : unsigned { local, global, operand };
    enum class integer_transform_kind : unsigned { and_mask, unsigned_convert, logical_shift_right, nonzero };
    struct integer_transform
    {
        integer_transform_kind kind{};
        ::std::uint64_t operand{}; // mask or CU-relative base-type DIE offset
        die_key type_identity{}; // resolved in this immutable CU only
        ::std::uint8_t bit_width{};
        bool resolved{};
    };
    struct location_piece;
    struct location_plan
    {
        plan_kind kind{plan_kind::unavailable};
        unavailable_reason reason{unavailable_reason::no_location};
        ::std::uint64_t local_index{}, constant_bits{};
        ::std::int64_t displacement{};
        ::std::array<::std::byte, 16u> implicit_bytes{};
        ::std::uint8_t byte_count{}, address_bytes{};
        bool signed_constant{}, implicit_constant{};
        // Delay vector's special-member instantiation until location_piece is
        // complete; its default constructor still produces an empty vector.
        ::std::vector<location_piece> pieces;
        ::std::uint64_t composite_bits{};
        // Metadata provenance only, never read authority. The direct attribute
        // decoder sets this only after exact scalar validation succeeds;
        // ordinary DW_OP constant expressions and copied carriers leave false.
        bool direct_constant_attribute{};
        wasm_location_space storage{wasm_location_space::local};
        // Only finite integer operations on an already copied local. Conversion
        // references are unusable until the index validates the exact base DIE.
        ::std::array<integer_transform, 4u> integer_transforms{};
        ::std::uint8_t integer_transform_count{};
    };
    [[nodiscard]] inline bool transform_copied_integer(location_plan const& plan, ::std::uint64_t& bits, unsigned carrier_bits = 64u) noexcept
    {
        if((carrier_bits != 32u && carrier_bits != 64u) || plan.integer_transform_count > plan.integer_transforms.size()) { return false; }
        if(carrier_bits == 32u) { bits &= 0xffffffffu; }
        for(::std::size_t i{}; i != plan.integer_transform_count; ++i)
        {
            auto const& step{plan.integer_transforms[i]};
            if(step.kind == integer_transform_kind::and_mask) { bits &= step.operand; }
            else if(step.kind == integer_transform_kind::logical_shift_right && step.operand < carrier_bits)
            { bits >>= static_cast<unsigned>(step.operand); }
            else if(step.kind == integer_transform_kind::nonzero && step.operand == 0u)
            { bits = bits != 0u; }
            else if(step.kind == integer_transform_kind::unsigned_convert && step.resolved && step.bit_width != 0u && step.bit_width <= 64u)
            {
                if(step.bit_width != 64u) { bits &= (::std::uint64_t{1u} << step.bit_width) - 1u; }
                carrier_bits = step.bit_width;
            }
            else { return false; }
        }
        return true;
    }
    struct location_piece
    {
        // atom is finite and has NO nested pieces. Empty/unsupported atoms are
        // explicit holes, not zero values. All quantities are scalar bit offsets.
        location_plan atom{};
        ::std::uint64_t object_bit_offset{}, bit_size{}, source_bit_offset{};
    };
    struct location_record
    {
        // nullopt is a DWARF default/all-address location, not a native address.
        ::std::optional<code_range> range{};
        location_plan plan{};
    };
    enum class scope_kind : unsigned { compile_unit, subprogram, inline_subprogram, lexical_block };
    enum class producer_language { none, tinygo, zig };
    // A bounded copied producer spelling supplements C99 only. It selects
    // expression semantics, never a source/frame/native execution capability.
    [[nodiscard]] inline constexpr producer_language copied_producer_language(
        ::std::uint64_t language, ::std::string_view text) noexcept
    {
        if(language != 0x0cu) { return producer_language::none; }
        if(text == "TinyGo") { return producer_language::tinygo; }
        if(text.size() > 128u || !text.starts_with("zig ")) { return producer_language::none; }
        text.remove_prefix(4u);
        for(unsigned part{}; part != 3u; ++part)
        {
            ::std::size_t digits{};
            while(digits < text.size() && ::fast_io::char_category::is_c_digit(text[digits])) { ++digits; }
            if(digits == 0u || digits > 10u) { return producer_language::none; }
            text.remove_prefix(digits);
            if(part != 2u)
            { if(text.empty() || text.front() != '.') { return producer_language::none; } text.remove_prefix(1u); }
        }
        if(text.empty()) { return producer_language::zig; }
        if((text.front() != '-' && text.front() != '+') || text.size() == 1u) { return producer_language::none; }
        bool build{text.front() == '+'}; text.remove_prefix(1u); bool component{};
        for(char c : text)
        {
            if(c == '.' || c == '+')
            { if(!component || (c == '+' && build)) { return producer_language::none; } component = false; build = build || c == '+'; }
            else if(::fast_io::char_category::is_c_digit(c) || (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') || c == '-') { component = true; }
            else { return producer_language::none; }
        }
        return component ? producer_language::zig : producer_language::none;
    }
    struct scope_record
    {
        die_key identity{};
        ::std::size_t parent{no_record};
        scope_kind kind{};
        ::std::string name{}, call_file{};
        ::std::uint64_t call_line{}, call_column{};
        ::std::vector<code_range> ranges{};
        ::std::vector<location_record> frame_base{};
        bool concrete{};
        // Own DIE range attributes are distinct from absent attributes. An
        // explicitly empty range NEVER inherits an ancestor's active range.
        // Abstract origin/specification must not contribute this flag.
        bool own_ranges_declared{};
        die_key object_pointer{}; bool object_pointer_known{};
        // Direct owning CU metadata, published only after bounded origin links
        // stay within that CU. Cross-CU declaration language remains unknown.
        ::std::uint64_t language{};
        bool tinygo_producer{};
        bool zig_producer{}; // Zig LLVM currently labels its owned CUs as C99.
    };
    enum class type_kind : unsigned { unavailable, scalar, pointer, structure, class_type, union_type, array, enumeration, subroutine, member_pointer };
    struct member_record
    {
        ::std::string name{};
        ::std::size_t type{no_record};
        ::std::uint64_t byte_offset{}, data_bit_offset{}, bit_size{};
        bool offset_known{}, bit_field{}, inherited{};
    };
    struct dimension_record
    {
        ::std::int64_t lower_bound{};
        ::std::uint64_t count{};
        bool lower_bound_known{}, count_known{};
    };
    struct enumerator_record
    {
        ::std::string name{};
        ::std::uint64_t bits{};
        bool signed_value{};
    };
    struct variant_selector_record
    {
        // Canonical integer bits: signed endpoints are sign extended to 64 bits.
        // Signedness comes from the discriminant TYPE, never the host width or
        // the DW_FORM used to encode a constant. Both endpoints are inclusive.
        ::std::uint64_t low{}, high{};
    };
    enum class variant_selector_kind : unsigned { unavailable, default_case, intervals };
    struct variant_record
    {
        ::std::string name{};
        variant_selector_kind selector_kind{variant_selector_kind::unavailable};
        ::std::vector<variant_selector_record> selectors{};
        ::std::vector<member_record> members{};
        bool layout_supported{true}; // nested/dynamic variant layouts are explicit.
    };
    struct variant_part_record
    {
        member_record discriminant{};
        ::std::vector<variant_record> variants{};
        ::std::uint8_t discriminant_bytes{};
        bool has_discriminant{}, discriminant_signed{}, discriminant_supported{};
    };
    // Finite C++ base-type DATA. Names are considered only at an actual
    // DW_TAG_base_type, together with its CU language, encoding and extent.
    // Display typedef spellings never establish a standard builtin identity.
    enum class cxx_narrow_builtin { unknown, plain_char, signed_char, unsigned_char, signed_short, unsigned_short };
    inline constexpr bool cxx_language(::std::uint64_t language) noexcept
    {
        switch(language)
        {
            case 0x04u: case 0x11u: case 0x19u: case 0x1au: case 0x21u: case 0x2au: case 0x2bu: case 0x3au: return true;
            default: return false;
        }
    }
    inline constexpr bool cxx_narrow_encoding(cxx_narrow_builtin kind, ::std::uint64_t encoding, unsigned bytes) noexcept
    {
        switch(kind)
        {
            case cxx_narrow_builtin::plain_char: case cxx_narrow_builtin::signed_char: return encoding == 0x06u && bytes == 1u;
            case cxx_narrow_builtin::unsigned_char: return encoding == 0x08u && bytes == 1u;
            case cxx_narrow_builtin::signed_short: return encoding == 0x05u && bytes == 2u;
            case cxx_narrow_builtin::unsigned_short: return encoding == 0x07u && bytes == 2u;
            default: return false;
        }
    }
    inline constexpr cxx_narrow_builtin cxx_narrow_from_base(::std::uint64_t language, bool tinygo,
        ::std::string_view base_name, ::std::uint64_t encoding, unsigned bytes) noexcept
    {
        if(!cxx_language(language) || tinygo) { return cxx_narrow_builtin::unknown; }
        auto kind{cxx_narrow_builtin::unknown};
        if(base_name == "char") { kind = cxx_narrow_builtin::plain_char; }
        else if(base_name == "signed char") { kind = cxx_narrow_builtin::signed_char; }
        else if(base_name == "unsigned char") { kind = cxx_narrow_builtin::unsigned_char; }
        else if(base_name == "short" || base_name == "short int" || base_name == "signed short" || base_name == "signed short int")
        { kind = cxx_narrow_builtin::signed_short; }
        else if(base_name == "unsigned short" || base_name == "unsigned short int" || base_name == "short unsigned int")
        { kind = cxx_narrow_builtin::unsigned_short; }
        return cxx_narrow_encoding(kind,encoding,bytes) ? kind : cxx_narrow_builtin::unknown;
    }
    // Standard integer rank is separate from representation. In Wasm32,
    // int and long have equal width but remain different native types.
    enum class c_wide_builtin { unknown, signed_int, unsigned_int, signed_long, unsigned_long, signed_long_long, unsigned_long_long };
    inline constexpr bool c_integer_language(::std::uint64_t language) noexcept
    {
        if(cxx_language(language)) { return true; }
        switch(language)
        { case 0x01u: case 0x02u: case 0x0cu: case 0x10u: case 0x1du: case 0x2cu: case 0x3eu: return true; default: return false; }
    }
    inline constexpr unsigned c_wide_rank(c_wide_builtin kind) noexcept
    {
        switch(kind)
        {
            case c_wide_builtin::signed_int: case c_wide_builtin::unsigned_int: return 3u;
            case c_wide_builtin::signed_long: case c_wide_builtin::unsigned_long: return 4u;
            case c_wide_builtin::signed_long_long: case c_wide_builtin::unsigned_long_long: return 5u;
            default: return 0u;
        }
    }
    inline constexpr bool c_wide_unsigned(c_wide_builtin kind) noexcept
    { return kind == c_wide_builtin::unsigned_int || kind == c_wide_builtin::unsigned_long || kind == c_wide_builtin::unsigned_long_long; }
    inline constexpr c_wide_builtin c_wide_unsigned_partner(c_wide_builtin kind) noexcept
    {
        switch(kind)
        {
            case c_wide_builtin::signed_int: return c_wide_builtin::unsigned_int;
            case c_wide_builtin::signed_long: return c_wide_builtin::unsigned_long;
            case c_wide_builtin::signed_long_long: return c_wide_builtin::unsigned_long_long;
            default: return c_wide_builtin::unknown;
        }
    }
    inline constexpr bool c_wide_extent(c_wide_builtin kind, unsigned bytes, unsigned address_bytes) noexcept
    {
        auto const rank{c_wide_rank(kind)};
        return rank == 3u ? bytes == 4u : rank == 5u ? bytes == 8u :
            rank == 4u && (address_bytes == 4u || address_bytes == 8u) && bytes == address_bytes;
    }
    inline constexpr c_wide_builtin c_wide_from_base(::std::uint64_t language, bool tinygo,
        ::std::string_view name, ::std::uint64_t encoding, unsigned bytes, unsigned address_bytes) noexcept
    {
        if(!c_integer_language(language) || tinygo) { return c_wide_builtin::unknown; }
        auto kind{c_wide_builtin::unknown};
        if(name == "int" || name == "signed int" || name == "signed") { kind = c_wide_builtin::signed_int; }
        else if(name == "unsigned int" || name == "unsigned") { kind = c_wide_builtin::unsigned_int; }
        else if(name == "long" || name == "long int" || name == "signed long" || name == "signed long int" || name == "long signed int")
        { kind = c_wide_builtin::signed_long; }
        else if(name == "unsigned long" || name == "unsigned long int" || name == "long unsigned int") { kind = c_wide_builtin::unsigned_long; }
        else if(name == "long long" || name == "long long int" || name == "signed long long" || name == "signed long long int" || name == "long long signed int")
        { kind = c_wide_builtin::signed_long_long; }
        else if(name == "unsigned long long" || name == "unsigned long long int" || name == "long long unsigned int") { kind = c_wide_builtin::unsigned_long_long; }
        return c_wide_extent(kind,bytes,address_bytes) && encoding == (c_wide_unsigned(kind) ? 0x07u : 0x05u) ? kind : c_wide_builtin::unknown;
    }
    struct type_record
    {
        die_key identity{};
        die_key declaration_identity{}; // final owned type DIE after cv/typedef wrappers
        ::std::string name{};
        type_kind kind{};
        ::std::uint64_t encoding{};
        ::std::uint8_t byte_count{};
        // Object sizes and offsets are Wasm linear-memory quantities. They are
        // not a read capability, native pointer or permission to allocate this
        // much memory. Scalar byte_count retains the copied-local ABI above.
        ::std::uint64_t byte_size{};
        ::std::size_t referenced_type{no_record};
        // Function and member-pointer declarations remain owned metadata.
        // None of these references is executable or a guest read capability.
        ::std::size_t containing_type{no_record};
        ::std::vector<::std::size_t> parameter_types{};
        ::std::uint64_t calling_convention{};
        ::std::uint8_t method_qualifiers{};
        bool signature_complete{}, variadic{}, calling_convention_known{};
        bool method_lvalue_reference{}, method_rvalue_reference{}, first_parameter_artificial{};
        ::std::vector<member_record> members{};
        ::std::vector<dimension_record> dimensions{};
        ::std::vector<enumerator_record> enumerators{};
        bool size_known{}, contiguous_array{true}, row_major_array{true}, reference_type{}, rvalue_reference_type{};
        ::std::vector<variant_part_record> variant_parts{};
        // DWARF5 5.3 optional pointer/reference dereference class. Preserve its
        // explicit presence; it is metadata, never a memory/segment capability.
        ::std::uint64_t address_class{};
        bool address_class_known{};
        // Original compilation-unit DW_AT_language, not a type-name guess.
        ::std::uint64_t language{};
        bool tinygo_producer{}; // TinyGo currently emits Go CUs as DW_LANG_C99.
        // Display metadata only. Qualifiers before a named typedef describe
        // that spelling; qualifiers inside the typedef are already represented
        // by its alias name. This creates no source write/read authority.
        ::std::uint8_t display_qualifiers{}; // const=1, volatile=2, restrict=4, atomic=8.
        bool named_type_alias{};
        cxx_narrow_builtin narrow_builtin{}; // final same-CU base, not the alias name
        bool atomic_scalar{}; // any atomic wrapper, including wrappers hidden by a named typedef
        c_wide_builtin wide_builtin{}; // validated standard rank on this CU's finite Wasm ABI
        bool zig_producer{}; // Direct owning CU producer metadata, never a file/type-name guess.
    };
    struct variable_record
    {
        die_key identity{};
        ::std::size_t scope{no_record}, type{no_record};
        ::std::string name{}, declaration_file{};
        ::std::uint64_t declaration_line{};
        ::std::vector<location_record> locations{};
        bool parameter{};
        ::std::string qualified_name{};
        bool global{}, external{}, declaration{}, static_storage{};
        bool immutable_cpp_object_pointer{};
    };

    namespace details
    {
        struct reader
        {
            ::std::span<::std::byte const> bytes{};
            ::std::size_t cursor{};
            [[nodiscard]] bool byte(::std::uint8_t& value) noexcept
            {
                if(cursor >= bytes.size()) { return false; }
                // [expression bytes] expression_end
                // [safe           ] unsafe (one-past)
                //  ^^ cursor < size; read then advance by exactly one byte.
                value = ::std::to_integer<::std::uint8_t>(bytes[cursor++]);
                return true;
            }
            [[nodiscard]] bool fixed(::std::size_t count, ::std::uint64_t& value) noexcept
            {
                if(cursor > bytes.size() || (count != 1u && count != 2u && count != 4u && count != 8u) || count > bytes.size() - cursor)
                { return false; }
                // [owned bytes ... cursor ... cursor+count ... end]
                // [safe                                         ] unsafe (one-past)
                //                  ^^ checked count precedes either borrow.
                auto const first{reinterpret_cast<char const*>(bytes.data() + cursor)};
                auto const last{first + count};
                ::fast_io::parse_result<char const*> parsed{};
                switch(count)
                {
                    case 1u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<8>(value)); break;
                    case 2u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<16>(value)); break;
                    case 4u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<32>(value)); break;
                    case 8u: parsed = ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<64>(value)); break;
                    default: return false;
                }
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return false; }
                // [safe] the checked count advances cursor at most to bytes.end.
                cursor += count;
                return true;
            }
            template<typename T> [[nodiscard]] bool leb(T& value) noexcept
            {
                if(cursor >= bytes.size()) { return false; }
                // [owned expression ... cursor ... end]
                // [safe                         ] unsafe (one-past)
                //                       ^^ both borrows stay in the same span.
                auto const first{reinterpret_cast<char const*>(bytes.data() + cursor)};
                auto const last{reinterpret_cast<char const*>(bytes.data() + bytes.size())};
                auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::leb128_get(value))};
                if(parsed.code != ::fast_io::parse_code::ok) { return false; }
                // [safe] fast_io returns the next byte in [first,last]; cursor may reach end.
                cursor += static_cast<::std::size_t>(parsed.iter - first);
                return true;
            }
        };
        // Finite constant integer tails only; no general expression stack.
        [[nodiscard]] inline unavailable_reason integer_tail(reader& input, integer_transform& step) noexcept
        {
            ::std::uint8_t opcode{};
            if(!input.byte(opcode)) { return unavailable_reason::malformed_expression; }
            if(opcode == 0xa8u /* DW_OP_convert */)
            {
                if(!input.leb(step.operand)) { return unavailable_reason::malformed_expression; }
                if(step.operand == 0u) { return unavailable_reason::unsupported_expression; }
                step.kind = integer_transform_kind::unsigned_convert;
                return unavailable_reason::none; // exact CU-owned DIE resolution is required later.
            }
            if(opcode >= 0x30u && opcode <= 0x4fu) { step.operand = opcode - 0x30u; }
            else if(opcode == 0x10u /* DW_OP_constu */)
            { if(!input.leb(step.operand)) { return unavailable_reason::malformed_expression; } }
            else { return unavailable_reason::unsupported_expression; }
            if(!input.byte(opcode)) { return unavailable_reason::malformed_expression; }
            if(opcode == 0x1au /* DW_OP_and */) { step.kind = integer_transform_kind::and_mask; }
            else if(opcode == 0x25u /* DW_OP_shr */ && step.operand < 64u)
            { step.kind = integer_transform_kind::logical_shift_right; }
            else if(opcode == 0x2eu /* DW_OP_ne */ && step.operand == 0u)
            { step.kind = integer_transform_kind::nonzero; }
            else { return unavailable_reason::unsupported_expression; }
            return unavailable_reason::none;
        }
    }

    // Recognize a finite set of metadata shapes. There is no expression stack,
    // branching, host register lookup, dereference, function call or memory read.
    // In LLVM's Wasm emitter local-indirect also uses subopcode 0 but omits the
    // terminal stack_value. Therefore a plain local is not admitted as a value.
    [[nodiscard]] inline location_plan decode_single_location_plan(::std::span<::std::byte const> bytes,
        ::std::uint8_t address_bytes, limits const& cap = {}, position_role role = position_role::variable) noexcept
    {
        location_plan result{};
        result.address_bytes = address_bytes;
        if(address_bytes != 4u && address_bytes != 8u) { result.reason = unavailable_reason::malformed_expression; return result; }
        if(bytes.empty()) { result.reason = unavailable_reason::empty_location; return result; }
        if(bytes.size() > cap.max_expression_bytes) { result.reason = unavailable_reason::expression_limit; return result; }
        details::reader input{bytes};
        ::std::uint8_t op{};
        if(!input.byte(op)) { result.reason = unavailable_reason::malformed_expression; return result; }
        auto const terminal_value{[&]() noexcept
        {
            ::std::uint8_t terminal{};
            return input.byte(terminal) && terminal == 0x9fu /* DW_OP_stack_value */ && input.cursor == bytes.size();
        }};
        if(op == 0xedu) // DW_OP_WASM_location
        {
            ::std::uint64_t kind{}, index{};
            if(!input.leb(kind)) { result.reason = unavailable_reason::malformed_expression; return result; }
            if(kind > 3u) { result.reason = unavailable_reason::unsupported_wasm_location; return result; }
            if(!(kind == 3u ? input.fixed(4u, index) : input.leb(index)))
            { result.reason = unavailable_reason::malformed_expression; return result; }
            result.storage = kind == 0u ? wasm_location_space::local : kind == 2u ? wasm_location_space::operand : wasm_location_space::global;
            result.local_index = index;
            if(role == position_role::frame_base && input.cursor == bytes.size())
            { result.kind = plan_kind::wasm_local_frame_base; result.reason = unavailable_reason::none; return result; }
            if(input.cursor == bytes.size()) { result.reason = unavailable_reason::indirect_or_ambiguous_local; return result; }
            while(input.cursor < bytes.size() && bytes[input.cursor] != ::std::byte{0x9fu})
            {
                // No transform is a frame-base/guest-offset capability. Global
                // and operand snapshots are not available to this finite path.
                if(role != position_role::variable || result.storage != wasm_location_space::local)
                { result.reason = unavailable_reason::unsupported_expression; return result; }
                if(result.integer_transform_count == result.integer_transforms.size())
                { result.reason = unavailable_reason::expression_limit; return result; }
                integer_transform step{};
                auto const status{details::integer_tail(input, step)};
                if(status != unavailable_reason::none) { result.reason = status; return result; }
                result.integer_transforms[result.integer_transform_count++] = step;
            }
            if(!terminal_value()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            // The exact terminal stack_value describes this local's contents.
            // DW_AT_frame_base uses them as its guest-offset carrier; a variable
            // location remains a copied value. Neither role grants a memory read.
            result.kind = role == position_role::frame_base ? plan_kind::wasm_local_frame_base : plan_kind::wasm_local_value;
            result.reason = unavailable_reason::none;
            return result;
        }
        if(op == 0x91u) // DW_OP_fbreg; displacement metadata only.
        {
            if(!input.leb(result.displacement)) { result.reason = unavailable_reason::malformed_expression; return result; }
            if(input.cursor != bytes.size()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.kind = plan_kind::frame_relative_offset; result.reason = unavailable_reason::none;
            return result;
        }
        if(op == 0x03u) // DW_OP_addr: Wasm producer ABI guest offset only.
        {
            if(!input.fixed(address_bytes, result.constant_bits))
            { result.reason = unavailable_reason::malformed_expression; return result; }
            if(input.cursor != bytes.size() || role != position_role::variable)
            { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.kind = plan_kind::absolute_guest_offset; result.reason = unavailable_reason::none;
            return result; // Never resolve linkage symbols, host addresses or memory here.
        }
        if(op == 0x9eu) // DW_OP_implicit_value
        {
            ::std::uint64_t count{};
            if(!input.leb(count) || count > bytes.size() - input.cursor)
            { result.reason = unavailable_reason::malformed_expression; return result; }
            if(count == 0u || count > result.implicit_bytes.size()) { result.reason = unavailable_reason::value_too_large; return result; }
            if(count != bytes.size() - input.cursor) { result.reason = unavailable_reason::unsupported_expression; return result; }
            for(::std::size_t i{}; i != count; ++i)
            {
                // [safe] count <= 16 and exactly matches the remaining expression;
                // cursor+i is live, and no source pointer advances or escapes.
                result.implicit_bytes[i] = bytes[input.cursor + i];
            }
            result.byte_count = static_cast<::std::uint8_t>(count);
            result.implicit_constant = true;
        }
        else if(op == 0x10u) // DW_OP_constu
        {
            if(!input.leb(result.constant_bits)) { result.reason = unavailable_reason::malformed_expression; return result; }
            if(!terminal_value()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.byte_count = address_bytes;
        }
        else if(op == 0x11u) // DW_OP_consts
        {
            ::std::int64_t value{};
            if(!input.leb(value)) { result.reason = unavailable_reason::malformed_expression; return result; }
            result.constant_bits = static_cast<::std::uint64_t>(value); result.signed_constant = true;
            if(!terminal_value()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.byte_count = address_bytes;
        }
        else if(op >= 0x30u && op <= 0x4fu) // DW_OP_lit0 ... DW_OP_lit31
        {
            result.constant_bits = op - 0x30u;
            if(!terminal_value()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.byte_count = address_bytes;
        }
        else { result.reason = unavailable_reason::unsupported_expression; return result; }
        result.kind = plan_kind::constant_value; result.reason = unavailable_reason::none;
        return result;
    }

    [[nodiscard]] inline location_plan decode_composite_location_plan(::std::span<::std::byte const> bytes,
        ::std::uint8_t address_bytes, limits const& cap = {}) noexcept
    {
        location_plan result{}; result.address_bytes = address_bytes;
        if(address_bytes != 4u && address_bytes != 8u) { result.reason = unavailable_reason::malformed_expression; return result; }
        if(bytes.empty()) { result.reason = unavailable_reason::empty_location; return result; }
        if(bytes.size() > cap.max_expression_bytes) { result.reason = unavailable_reason::expression_limit; return result; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            details::reader input{bytes};
            while(input.cursor != bytes.size())
            {
                auto const atom_begin{input.cursor}; ::std::uint8_t op{};
                if(!input.byte(op)) { result.reason = unavailable_reason::malformed_expression; return result; }
                bool const empty{op == 0x93u || op == 0x9du};
                if(!empty)
                {
                    if(op == 0xedu)
                    {
                        ::std::uint64_t kind{}, index{};
                        if(!input.leb(kind)) { result.reason = unavailable_reason::malformed_expression; return result; }
                        if(kind > 3u) { result.reason = unavailable_reason::unsupported_wasm_location; return result; }
                        if(!(kind == 3u ? input.fixed(4u, index) : input.leb(index)))
                        { result.reason = unavailable_reason::malformed_expression; return result; }
                        ::std::size_t transforms{};
                        while(input.cursor < bytes.size() && bytes[input.cursor] != ::std::byte{0x9fu} &&
                              bytes[input.cursor] != ::std::byte{0x93u} && bytes[input.cursor] != ::std::byte{0x9du})
                        {
                            if(transforms++ == result.integer_transforms.size())
                            { result.reason = unavailable_reason::expression_limit; return result; }
                            integer_transform step{};
                            auto const status{details::integer_tail(input, step)};
                            if(status != unavailable_reason::none) { result.reason = status; return result; }
                        }
                    }
                    else if(op == 0x91u || op == 0x11u)
                    { ::std::int64_t ignored{}; if(!input.leb(ignored)) { result.reason = unavailable_reason::malformed_expression; return result; } }
                    else if(op == 0x10u)
                    { ::std::uint64_t ignored{}; if(!input.leb(ignored)) { result.reason = unavailable_reason::malformed_expression; return result; } }
                    else if(op == 0x9eu)
                    {
                        ::std::uint64_t count{};
                        if(!input.leb(count) || count > bytes.size() - input.cursor)
                        { result.reason = unavailable_reason::malformed_expression; return result; }
                        // [expression ... cursor ... cursor+count ... end]
                        // [safe                                        ] unsafe (one-past)
                        //                 ^^ checked count BEFORE scalar advance.
                        input.cursor += static_cast<::std::size_t>(count);
                    }
                    else if(op < 0x30u || op > 0x4fu) { result.reason = unavailable_reason::unsupported_expression; return result; }
                    if(input.cursor < bytes.size() && bytes[input.cursor] == ::std::byte{0x9fu})
                    {
                        // [safe] cursor<size before the optional stack_value read
                        // and its one-byte scalar advance through reader::byte.
                        if(!input.byte(op)) { result.reason = unavailable_reason::malformed_expression; return result; }
                    }
                }
                auto const atom_end{empty ? atom_begin : input.cursor};
                if(!empty && (!input.byte(op) || (op != 0x93u && op != 0x9du)))
                { result.reason = unavailable_reason::unsupported_expression; return result; }
                ::std::uint64_t size{}, source_offset{};
                if(!input.leb(size) || (op == 0x9du && !input.leb(source_offset)))
                { result.reason = unavailable_reason::malformed_expression; return result; }
                if(op == 0x93u)
                {
                    if(size > (::std::numeric_limits<::std::uint64_t>::max)() / 8u)
                    { result.reason = unavailable_reason::expression_limit; return result; }
                    size *= 8u;
                }
                if(result.pieces.size() >= cap.max_location_pieces || result.composite_bits > cap.max_composite_bits ||
                   size > cap.max_composite_bits - result.composite_bits)
                { result.reason = unavailable_reason::expression_limit; return result; }
                location_piece piece{};
                // [expression ... atom_begin ... atom_end ... end]
                // [safe                                         ] unsafe (one-past)
                //                 ^^ the bounded token reader proved both scalar
                // offsets BEFORE forming a finite synchronous subspan.
                piece.atom = decode_single_location_plan(bytes.subspan(atom_begin, atom_end - atom_begin), address_bytes, cap);
                if(piece.atom.reason == unavailable_reason::malformed_expression)
                { result.reason = unavailable_reason::malformed_expression; return result; }
                piece.object_bit_offset = result.composite_bits; piece.bit_size = size; piece.source_bit_offset = source_offset;
                result.composite_bits += size; // preceding subtraction checked cumulative bit count.
                result.pieces.push_back(::std::move(piece));
            }
            if(result.pieces.empty()) { result.reason = unavailable_reason::unsupported_expression; return result; }
            result.kind = plan_kind::composite_value; result.reason = unavailable_reason::none; return result;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { result = {}; result.address_bytes = address_bytes; result.reason = unavailable_reason::allocation_failure; return result; }
#endif
    }
    [[nodiscard]] inline location_plan decode_location_plan(::std::span<::std::byte const> bytes,
        ::std::uint8_t address_bytes, limits const& cap = {}, position_role role = position_role::variable) noexcept
    {
        auto result{decode_single_location_plan(bytes, address_bytes, cap, role)};
        if(role == position_role::variable &&
           (result.reason == unavailable_reason::unsupported_expression || result.reason == unavailable_reason::unsupported_wasm_location ||
            result.reason == unavailable_reason::indirect_or_ambiguous_local || result.reason == unavailable_reason::value_too_large))
        {
            auto composite{decode_composite_location_plan(bytes, address_bytes, cap)};
            if(composite.kind == plan_kind::composite_value || composite.reason == unavailable_reason::malformed_expression ||
               composite.reason == unavailable_reason::expression_limit || composite.reason == unavailable_reason::allocation_failure)
            { return composite; }
        }
        return result;
    }
}
