// Detached model tests only. These values issue no GC/root/runtime authority.
#include <uwvm2/uwvm/debugger/wasm_state.h>
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
namespace
{
    unsigned checks{};
    bool expect(bool yes, ::fast_io::string_view what)
    {
        ++checks;
        if(!yes) { ::fast_io::print(::fast_io::err(), "FAIL GC member-page DATA: ", what, "\n"); }
        return yes;
    }
    ws::value reference(ws::reference_kind kind, ::std::int64_t heap, ::std::uint64_t object)
    {
        ws::value out{}; out.type = {ws::value_kind::reference, heap, true, true};
        out.available = true; out.ref.kind = kind; out.ref.object = object; return out;
    }
    ws::value number(::std::uint32_t n)
    {
        ws::value out{}; out.available = true; out.type.known = true;
        auto* first{reinterpret_cast<unsigned char*>(out.bits.data())};
        // [actual fixed16 bytes][first+4 <= fixed end] end
        // [safe] complete4-byte destination checked BEFORE end formation.
        ::fast_io::basic_obuffer_view<unsigned char> bytes{first, first + 4u};
        ::fast_io::print(bytes, ::fast_io::mnp::le_put<32u>(n)); return out;
    }
    ws::view page(::std::uint64_t first = 256u, ::std::uint64_t total = 1024u)
    {
        ws::view out{}; out.result = ws::status::available; out.runtime_epoch = 3u;
        out.requested = {ws::selection::globals, 7u, 0u, 0u, 0u, 9u, 1u};
        out.requested.member_first = first; out.requested.member_count = 64u;
        out.first = 9u; out.total_values = 10u; out.selected_object = 1u;
        out.rows = {{9u, reference(ws::reference_kind::array, -22, 1u)}};
        ws::object array{}; array.identifier = 1u; array.kind = ws::object_kind::array;
        array.total_members = total; array.first_member = first;
        auto const remaining{total - first}; auto const count{remaining < 64u ? remaining : 64u};
        array.next_member = first + count; array.has_more_members = count != remaining;
        array.members_truncated = first != 0u || count != total;
        for(::std::uint64_t i{}; i != count; ++i)
        { array.members.push_back({first + i, number(static_cast<::std::uint32_t>(first + i)), true, true}); }
        out.objects.push_back(::std::move(array)); out.graph_truncated = out.objects[0u].members_truncated;
        return out;
    }
    bool contains(::std::string const& s, ::std::string_view needle)
    { return ::std::string_view{s}.find(needle) != ::std::string_view::npos; }
}
int main()
{
    bool good{true}; auto data{page()};
    good &= expect(ws::valid(data) && data.objects[0u].members.front().index == 256u &&
        data.objects[0u].members.back().index == 319u, "nonzero first preserves original array element indices");
    auto text{ws::format(data)};
    good &= expect(contains(text, "members=1024 first=256 next=320 more=yes") && contains(text, "256 i32 = 256 mutable"),
        "formatter gives exact next and field mutability");
    auto final{page(960u)};
    good &= expect(ws::valid(final) && final.objects[0u].next_member == 1024u && !final.objects[0u].has_more_members,
        "last nonzero page ends exactly at total");
    good &= expect(!contains(ws::format(final), "continue with"), "final page does not request a nonexistent next page");
    auto empty{page(1024u)};
    good &= expect(ws::valid(empty) && empty.objects[0u].members.empty() && empty.objects[0u].next_member == 1024u,
        "end page returns explicit empty window without overflow");
    auto zero{page(0u, 0u)};
    good &= expect(ws::valid(zero) && !zero.objects[0u].members_truncated, "empty GC array has a complete zero-member page");
    auto bad{data}; bad.objects[0u].members[0u].index = 0u;
    good &= expect(!ws::valid(bad), "rebased member indices are rejected");
    bad = data; bad.objects[0u].next_member = 256u;
    good &= expect(!ws::valid(bad), "false continuation cannot skip returned members");
    bad = data; bad.objects[0u].has_more_members = false;
    good &= expect(!ws::valid(bad), "false end-of-object flag is rejected");
    bad = data; bad.objects[0u].first_member = 1025u;
    good &= expect(!ws::valid(bad), "first checked before total-first subtraction");
    bad = data; bad.requested.count = 2u;
    good &= expect(!ws::valid(bad.requested), "member query selects exactly one original root");
    bad = data; bad.requested.member_count = 65u;
    good &= expect(!ws::valid(bad.requested), "member page quota64");
    bad = data; bad.requested.path_size = 17u;
    good &= expect(!ws::valid(bad.requested), "path quota16 is checked before indexing");
    bad = data; bad.requested.path[15u] = 1u;
    good &= expect(!ws::valid(bad.requested), "unused path cells cannot hide alternate selectors");
    bad = data; bad.requested.member_count = 0u;
    good &= expect(!ws::valid(bad.requested), "row query cannot retain member-page offset");
    bad = data; bad.selected_object = 129u;
    good &= expect(!ws::valid(bad), "query-local selected ID is checked before use and never input");
    bad = data; bad.objects[0u].members[0u].mutability_known = false;
    good &= expect(!ws::valid(bad), "mutable storage flag needs known declaration");

    // One page of64 distinct ref children needs65 objects including its array.
    auto children{data};
    for(::std::size_t i{}; i != children.objects[0u].members.size(); ++i)
    {
        ws::object child{}; child.identifier = i + 2u; child.kind = ws::object_kind::structure;
        child.total_members = 1u; child.members_truncated = true; child.has_more_members = true;
        children.objects[0u].members[i].data = reference(ws::reference_kind::structure, -21, child.identifier);
        children.objects.push_back(::std::move(child));
    }
    good &= expect(ws::valid(children) && children.objects.size() == 65u, "64 child identities fit without recursive expansion");
    good &= expect(ws::format(children).size() <= ws::maximum_reply_bytes, "shallow child metadata respects finite reply cap");
    bad = children; bad.objects[1u].members.push_back({0u, number(1u)}); bad.objects[1u].next_member = 1u;
    bad.objects[1u].members_truncated = false; bad.objects[1u].has_more_members = false;
    good &= expect(!ws::valid(bad), "a member page cannot silently recurse into a child");
    auto aliases{data};
    for(auto& member : aliases.objects[0u].members)
    { member.data = reference(ws::reference_kind::array, -22, 1u); }
    good &= expect(ws::valid(aliases), "array self-cycle preserves exact root alias");
    auto exn{page(0u, 1u)}; exn.rows[0u].data = reference(ws::reference_kind::exception, -23, 1u);
    exn.objects[0u].kind = ws::object_kind::exception; exn.objects[0u].tag_identity_available = true;
    exn.objects[0u].members[0u].data = reference(ws::reference_kind::i31, -20, 0u);
    exn.objects[0u].members[0u].data.ref.i31_bits = 0x7fffffffu;
    exn.objects[0u].members[0u].mutable_storage = false;
    good &= expect(ws::valid(exn) && contains(ws::format(exn), "i31 signed=-1 unsigned=2147483647"),
        "exception payload page preserves i31 and exact tag metadata");
    auto wrapper{page(0u, 1u)}; wrapper.rows[0u].data = reference(ws::reference_kind::external_wrapper, -17, 1u);
    wrapper.objects[0u].kind = ws::object_kind::external_wrapper;
    ws::object host{}; host.identifier = 2u; host.kind = ws::object_kind::host_reference;
    wrapper.objects[0u].members[0u].data = reference(ws::reference_kind::host_reference, -18, 2u);
    wrapper.objects[0u].members[0u].mutable_storage = false; wrapper.objects.push_back(host);
    good &= expect(ws::valid(wrapper) && contains(ws::format(wrapper), "payload unavailable"),
        "extern wrapper exposes only sanitized inner host identity");
    bad = wrapper; bad.selected_object = 2u;
    good &= expect(!ws::valid(bad), "opaque host payload cannot be selected for expansion");
    auto large{page(0u, 64u)};
    for(auto& member : large.objects[0u].members)
    {
        member.data = reference(ws::reference_kind::function, 4294967295ll, 0u);
        member.data.type.type_module = (::std::numeric_limits<::std::uint64_t>::max)();
        member.data.ref.function_module = (::std::numeric_limits<::std::uint64_t>::max)();
        member.data.ref.function_index = (::std::numeric_limits<::std::uint64_t>::max)();
        member.data.ref.function_identity_available = true;
    }
    text = ws::format(large);
    good &= expect(ws::valid(large) && text.size() <= ws::maximum_reply_bytes &&
        contains(text, "members=64 first=0 next=64 more=no") && contains(text, "63 (ref"),
        "all64 longest declared function reference values fit before metadata truncation");
    good &= expect(!large.snapshot_or_restore_authority(), "detached pagination is neither native-memory nor restore authority");
    // Format-only conservative maximum: path16 + selected object +64
    // distinct child metadata AND64 longest function values, even though a
    // function carrier cannot itself enqueue a GC child. No runtime authority.
    auto maximum{page(0u, 64u)};
    auto const u64max{(::std::numeric_limits<::std::uint64_t>::max)()};
    maximum.requested.selected = ws::selection::table; maximum.requested.index = u64max;
    maximum.requested.module = u64max; maximum.requested.participant = u64max;
    maximum.requested.first = u64max - 1u; maximum.first = u64max - 1u;
    maximum.total_values = u64max; maximum.module = u64max; maximum.runtime_epoch = u64max;
    maximum.requested.path_size = 16u; maximum.requested.path.fill(u64max);
    maximum.requested.member_first = u64max - 64u; maximum.selected_object = 17u;
    maximum.rows = {{u64max - 1u, reference(ws::reference_kind::structure, 4294967295ll, 1u)}};
    maximum.rows[0u].data.type.type_module = u64max;
    maximum.objects.clear(); maximum.objects.reserve(81u);
    for(::std::size_t index{}; index != 81u; ++index)
    {
        ws::object object{}; object.identifier = index + 1u; object.module = u64max; object.type_index = 4294967295u;
        object.kind = index == 0u ? ws::object_kind::structure : ws::object_kind::exception;
        object.tag_identity_available = true; object.tag_module = u64max; object.tag_index = u64max;
        object.total_members = u64max; object.members_truncated = true; object.has_more_members = true;
        if(object.identifier == maximum.selected_object)
        {
            object.first_member = u64max - 64u; object.next_member = u64max; object.has_more_members = false;
            for(::std::uint64_t member{}; member != 64u; ++member)
            {
                auto value{reference(ws::reference_kind::function, 4294967295ll, 0u)};
                value.type.type_module = u64max; value.ref.function_module = u64max; value.ref.function_index = u64max;
                value.ref.function_identity_available = true;
                object.members.push_back({object.first_member + member, value, false, true});
            }
        }
        maximum.objects.push_back(::std::move(object));
    }
    maximum.graph_truncated = true; text = ws::format(maximum);
    good &= expect(ws::valid(maximum) && maximum.objects.size() == 81u && text.size() < ws::maximum_reply_bytes - 256u,
        "path16 +64 longest function values +64 worst child labels fit FULL append budget");
    good &= expect(contains(text, "Wasm members object=17 first=18446744073709551551 count=64 path=") &&
        contains(text, "object #81 exception tag-module=18446744073709551615 tag=18446744073709551615") &&
        contains(text, "18446744073709551614 (ref null type-index=4294967295 module=18446744073709551615)"),
        "complete selected page and final dense child survive formatter without missing labels");
    good &= expect(contains(text, "Wasm state truncated: rows=1 objects=81"),
        "only SHALLOW metadata is marked truncated; all81 objects and64 requested members were printed");
    if(!good) { return 1; }
    ::fast_io::print(::fast_io::out(), "GC member-page DATA checks=", ::fast_io::mnp::dec(checks),
        "; no runtime pause, GC borrow or restore qualification\n");
}
