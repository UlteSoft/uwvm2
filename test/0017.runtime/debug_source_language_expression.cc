#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <array>
#include <cstring>
using namespace uwvm2::uwvm::debugger;
static void check(bool value, char const* reason)
{ if(!value) { ::fast_io::io::perrln(::fast_io::err(), "debug_source_language_expression: FAIL ", ::fast_io::mnp::os_c_str(reason)); ::fast_io::fast_terminate(); } }
static source_dwarf::type_record scalar()
{ source_dwarf::type_record v{}; v.kind = source_dwarf::type_kind::scalar; v.encoding = 7u; v.byte_count = 4u; v.byte_size = 4u; v.size_known = true; return v; }
static source_dwarf::type_record pointer(::std::uint8_t width)
{ source_dwarf::type_record v{}; v.kind = source_dwarf::type_kind::pointer; v.byte_count = width; v.byte_size = width; v.size_known = true; v.referenced_type = 2u; return v; }
static source_dwarf::source_expression expression(::std::string_view text)
{ source_dwarf::source_expression result{}; check(source_dwarf::parse_source_expression(text, result) == source_dwarf::object_selector_error::none, "finite fixture syntax"); return result; }
static void encode(::std::span<::std::byte> bytes, ::std::uint64_t offset, ::std::uint64_t value, ::std::uint8_t width)
{
    check(offset <= bytes.size() && width <= bytes.size() - static_cast<::std::size_t>(offset), "fixture complete output extent");
    // [owned byte allocation ... offset ... offset+width<=size] end
    // [safe                                                   ] checked BEFORE both char pointer changes.
    auto const first{reinterpret_cast<char*>(bytes.data() + static_cast<::std::size_t>(offset))};
    ::fast_io::basic_obuffer_view<char> sink{first, first + width};
    if(width == 4u) { ::fast_io::io::print(sink, ::fast_io::mnp::le_put<32u>(value)); }
    else { check(width == 8u, "fixture width"); ::fast_io::io::print(sink, ::fast_io::mnp::le_put<64u>(value)); }
}
int main()
{
    for(::std::uint8_t width : {4u, 8u})
    {
        ::std::vector<source_dwarf::type_record> types{scalar(), pointer(width), {}};
        auto& node{types[2u]}; node.kind = source_dwarf::type_kind::structure; node.size_known = true; node.byte_size = width * 2u;
        source_dwarf::member_record value{}; value.name = "value"; value.type = 0u; value.offset_known = true;
        source_dwarf::member_record next{}; next.name = "next"; next.type = 1u; next.byte_offset = width; next.offset_known = true;
        node.members = {value, next};
        ::std::vector<::std::byte> memory(96u), root(width); encode(memory, 32u, 7u, 4u); encode(memory, 32u + width, 48u, width);
        encode(memory, 48u, 41u, 4u); encode(memory, 48u + width, 0u, width); encode(root, 0u, 32u, width);
        ::std::size_t calls{};
        auto const read{[&](::std::uint64_t offset, ::std::size_t length, ::std::vector<::std::byte>& copied) noexcept
        {
            ++calls; copied.clear();
            if(offset > memory.size() || length > memory.size() - static_cast<::std::size_t>(offset)) { return false; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            try
            {
#endif
                copied.resize(length);
                if(length != 0u)
                {
                    // [owned fixture bytes ... offset ... length<=remaining] end
                    // [safe                                                   ] checked BEFORE begin+offset;
                    //  ^^ this finite source-only fake provider is NOT a VM authority test.
                    ::fast_io::freestanding::my_memcpy(copied.data(), memory.data() + static_cast<::std::size_t>(offset), length);
                }
                return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            }
            catch(...) { copied.clear(); return false; }
#endif
        }};
        ::std::vector<source_dwarf::object_node> out{};
        auto const chain{expression("p->next->value")};
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width, read, out) == source_dwarf::inline_query_error::none &&
            calls == 2u && out.size() == 1u && out[0u].value_available && out[0u].bits == 41u, "complete guest-pointer chain from owned LE bytes");
        calls = 0u; types[1u].language = 0x1cu; types[1u].name = ::fast_io::concat_std("&Node");
        auto const rust_chain{expression("p.next.value")};
        check(source_language_expression::value(types,1u,rust_chain.steps,root,{},width,read,out)==source_dwarf::inline_query_error::none &&
            calls==2u && out.size()==1u && out[0u].bits==41u,"Rust reference member auto-dereference remains guest-only");
        calls = 0u; types[1u].language = 0x16u; types[1u].name = ::fast_io::concat_std("*Node");
        check(source_language_expression::value(types,1u,rust_chain.steps,root,{},width,read,out)==source_dwarf::inline_query_error::none &&
            calls==2u && out.size()==1u && out[0u].bits==41u,"Go pointer member auto-dereference remains guest-only");
        calls = 0u;
        ::std::vector<::std::byte> integer_unknown(width);
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width, read, out, {}, integer_unknown) ==
            source_dwarf::inline_query_error::unavailable && calls == 0u && out.empty(), "floating carrier qualification cannot mint pointer even if bytes are known");
        source_dwarf::copied_numeric_local captured{}; captured.available = true; captured.wasm_type = width == 4u ? 0x7du : 0x7cu;
        if(width == 4u)
        {
            ::std::uint32_t carrier{32u}; ::fast_io::freestanding::my_memcpy(captured.bytes.data(), ::std::addressof(carrier), sizeof(carrier));
        }
        else
        {
            ::std::uint64_t carrier{32u}; ::fast_io::freestanding::my_memcpy(captured.bytes.data(), ::std::addressof(carrier), sizeof(carrier));
        }
        source_dwarf::location_plan composite{}; composite.kind = source_dwarf::plan_kind::composite_value;
        composite.reason = source_dwarf::unavailable_reason::none; composite.address_bytes = width; composite.composite_bits = width * 8u;
        source_dwarf::location_piece piece{}; piece.bit_size = width * 8u;
        piece.atom.kind = source_dwarf::plan_kind::wasm_local_value; piece.atom.reason = source_dwarf::unavailable_reason::none; piece.atom.address_bytes = width;
        composite.pieces.push_back(piece);
        source_dwarf::composite_value actual_bits{}, integer_bits{};
        check(source_dwarf::materialize_location_pieces(composite, {::std::addressof(captured), 1u}, 1u, {}, width, actual_bits) ==
            source_dwarf::piece_query_error::none && actual_bits.fully_available, "actual float carrier raw pieces remain displayable");
        captured.available = false;
        check(source_dwarf::materialize_location_pieces(composite, {::std::addressof(captured), 1u}, 1u, {}, width, integer_bits) ==
            source_dwarf::piece_query_error::none && !integer_bits.fully_available, "integer-only qualification records real holes");
        check(source_language_expression::value(types, 1u, chain.steps, actual_bits.bytes, actual_bits.known_bits,
            width, read, out, {}, integer_bits.known_bits) == source_dwarf::inline_query_error::unavailable && calls == 0u && out.empty(),
            "real composite float pieces cannot bypass pointer eligibility");
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width, read, out, {1u}) == source_dwarf::inline_query_error::limit_exceeded &&
            calls == 1u && out.empty(), "read budget charged before second callback and clears stale output");
        calls = 0u;
        ::std::vector<::std::byte> missing(width, ::std::byte{0xffu}); missing[0u] = ::std::byte{};
        check(source_language_expression::value(types, 1u, chain.steps, root, missing, width, read, out) == source_dwarf::inline_query_error::unavailable &&
            calls == 0u && out.empty(), "unknown pointer bits cannot become a guest offset");
        ::std::vector<::std::byte> null(width);
        check(source_language_expression::value(types, 1u, chain.steps, null, {}, width, read, out) == source_dwarf::inline_query_error::unavailable &&
            calls == 0u && out.empty(), "null fails before callback");
        check(source_language_expression::type(types, 1u, chain.steps, width, out) == source_dwarf::inline_query_error::none &&
            out.size() == 1u && out[0u].type == 0u, "pointee type does not depend on pointer value or memory read");
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width == 4u ? 8u : 4u, read, out) == source_dwarf::inline_query_error::unavailable &&
            calls == 0u && out.empty(), "actual memory width mismatch fails before read");
        encode(root, 0u, 95u, width);
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width, read, out) == source_dwarf::inline_query_error::unavailable &&
            calls == 1u && out.empty(), "provider range rejection keeps final result absent");
        calls = 0u;
        types.push_back({}); auto& mixed{types[3u]}; mixed.kind = source_dwarf::type_kind::structure; mixed.size_known = true; mixed.byte_size = width * 2u;
        auto float_type{scalar()}; float_type.encoding = 4u; types.push_back(float_type);
        source_dwarf::member_record floating{}; floating.name = "floating"; floating.type = 4u; floating.offset_known = true;
        source_dwarf::member_record ptr{}; ptr.name = "ptr"; ptr.type = 1u; ptr.byte_offset = width; ptr.offset_known = true;
        types[3u].members = {floating, ptr};
        ::std::vector<::std::byte> aggregate(width * 2u), availability(width * 2u, ::std::byte{0xffu}), qualified(width * 2u);
        encode(aggregate, 0u, 0x3f800000u, 4u); encode(aggregate, width, 48u, width);
        for(::std::size_t i{width}; i != qualified.size(); ++i) { qualified[i] = ::std::byte{0xffu}; }
        auto const mixed_pointer{expression("root.ptr->value")};
        check(source_language_expression::value(types, 3u, mixed_pointer.steps, aggregate, availability, width, read, out, {}, qualified) ==
            source_dwarf::inline_query_error::none && calls == 1u && out.size() == 1u && out[0u].value_available && out[0u].bits == 41u,
            "legitimate aggregate float member does not blanket-reject independent integer pointer field");
        auto const mixed_float{expression("root.floating")};
        check(source_language_expression::value(types, 3u, mixed_float.steps, aggregate, availability, width, read, out, {}, qualified) ==
            source_dwarf::inline_query_error::none && calls == 1u && out.size() == 1u && out[0u].value_available && out[0u].bits == 0x3f800000u,
            "float rendering uses actual availability rather than integer-only eligibility");
        availability[width] = ::std::byte{}; calls = 0u;
        check(source_language_expression::value(types, 3u, mixed_pointer.steps, aggregate, availability, width, read, out, {}, qualified) ==
            source_dwarf::inline_query_error::unavailable && calls == 0u && out.empty(), "qualification cannot restore actually unknown pointer bits");
        types[1u].address_class_known = true; types[1u].address_class = 17u; calls = 0u;
        check(source_language_expression::type(types, 1u, chain.steps, width, out) == source_dwarf::inline_query_error::unavailable && out.empty(), "unqualified address class has no conventional pointee query");
        check(source_language_expression::value(types, 1u, chain.steps, root, {}, width, read, out) == source_dwarf::inline_query_error::unavailable && calls == 0u && out.empty(), "nonzero class cannot read");
        auto oversized{expression("p->value")}; oversized.steps[1u].member.resize(4097u, 'x');
        check(source_language_expression::value(types, 1u, oversized.steps, root, {}, width, read, out) == source_dwarf::inline_query_error::limit_exceeded && calls == 0u && out.empty(), "untrusted hand-built step strings charged before copies or callback");
        check(source_language_expression::type(types, 1u, oversized.steps, width, out) == source_dwarf::inline_query_error::limit_exceeded && out.empty(), "type query same string budget");
        check(source_language_expression::type(types, 0u, {}, 3u, out) == source_dwarf::inline_query_error::malformed && out.empty(), "invalid width also rejected without dereference");
    }
    ::fast_io::io::println(::fast_io::out(), "debug_source_language_expression: PASS finite owned-byte planner/evaluator only; VM source authority unqualified");
}
