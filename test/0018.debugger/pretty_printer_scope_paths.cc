// Pure synthetic metadata selection only. No real ticket/source owner/frame
// is constructed and no runtime, Wasm guest or value access occurs in this unit.
#include <uwvm2/uwvm/debugger/source_scope_path.h>
#include <fast_io.h>
#include <atomic>
#include <thread>
using namespace uwvm2::uwvm::debugger::source_dwarf;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_scope_path: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static ::std::vector<scope_record> fixture()
{
    ::std::vector<scope_record> value(5u);
    for(::std::size_t i{}; i != value.size(); ++i) { value[i].identity = {10u, 100u + i}; }
    value[0].kind = scope_kind::compile_unit;
    value[1].parent = 0u; value[1].kind = scope_kind::subprogram; value[1].concrete = true; value[1].ranges = {{10u, 30u}};
    value[2].parent = 1u; value[2].kind = scope_kind::lexical_block;
    value[3].parent = 2u; value[3].kind = scope_kind::inline_subprogram; value[3].concrete = true; value[3].ranges = {{12u, 22u}};
    value[4].parent = 3u; value[4].kind = scope_kind::inline_subprogram; value[4].concrete = true; value[4].ranges = {{14u, 18u}};
    value[3].name = value[4].name = "same_name"; // names are not identities.
    return value;
}
static void clean(concrete_scope_path const& out)
{ check(out.physical_and_inline.empty() && out.record_indices.empty(), "failure never emits partial identities/indices"); }
static void legacy_cases()
{
    auto scopes{fixture()}; concrete_scope_path out{};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.physical_and_inline.size() == 3u &&
        out.physical_and_inline[0] == scopes[1].identity && out.physical_and_inline[1] == scopes[3].identity &&
        out.physical_and_inline[2] == scopes[4].identity && out.record_indices == ::std::vector<::std::size_t>{1u, 3u, 4u},
        "physical + outer-to-inner concrete keys despite same names");
    scopes[4].identity.offset = 999u;
    check(out.physical_and_inline[2].offset == 104u, "result owns key values");
    scopes = fixture();
    check(query_concrete_scope_path(scopes, 22u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "half-open inline end leaves physical key");
    check(query_concrete_scope_path(scopes, 35u, out) == scope_path_error::unavailable, "no fabricated caller physical scope"); clean(out);
    scopes[2].own_ranges_declared = true;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "explicit empty lexical scope excludes its inline descendants");
    scopes[2].own_ranges_declared = false;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 3u,
        "absent lexical range attributes inherit");
    scopes[2].ranges = {{24u, 26u}};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "explicit inactive lexical range excludes child");
    scopes[2].ranges = {{10u, 16u}};
    check(query_concrete_scope_path(scopes, 16u, out) == scope_path_error::none && out.record_indices.size() == 1u,
        "lexical range is also half-open");
    scopes = fixture(); scopes[4].parent = 1u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::ambiguous, "overlapping inline siblings fail closed"); clean(out);
    scopes = fixture(); scopes[4].kind = scope_kind::subprogram;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::ambiguous, "overlapping physical scopes fail closed"); clean(out);
    scopes = fixture(); scopes[4].concrete = false;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 2u,
        "abstract/descriptive scope never supplies concrete identity");
    scopes = fixture(); scopes[4].identity = scopes[1].identity;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "duplicate selected concrete key rejected"); clean(out);
    scopes = fixture(); scopes[4].identity.unit = 11u; scopes[4].identity.offset = scopes[1].identity.offset;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::none && out.record_indices.size() == 3u,
        "unit identity distinguishes equal DIE offsets");
    scopes = fixture(); scopes[4].parent = 4u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "self parent rejected"); clean(out);
    scopes = fixture(); scopes[4].parent = 99u;
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "out-of-span parent rejected"); clean(out);
    scopes = fixture(); scopes[2].kind = static_cast<scope_kind>(99u);
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "unknown scope enum rejected"); clean(out);
    scopes = fixture(); scopes[1].ranges = {{30u, 10u}};
    check(query_concrete_scope_path(scopes, 15u, out) == scope_path_error::malformed, "reversed range rejected before selection"); clean(out);
    scopes = fixture(); scopes[4].ranges = {{14u, 14u}};
    check(query_concrete_scope_path(scopes, 11u, out) == scope_path_error::malformed, "late malformed inactive record still rejects"); clean(out);
    scopes = fixture(); scope_path_limits cap{}; cap.max_keys = 2u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "physical counts toward total key budget"); clean(out);
    cap.max_keys = 0u;
    check(query_concrete_scope_path(scopes, 11u, out, cap) == scope_path_error::limit_exceeded, "zero key budget cannot emit physical key"); clean(out);
    cap = {}; cap.max_keys = 1u;
    check(query_concrete_scope_path(scopes, 22u, out, cap) == scope_path_error::none && out.record_indices.size() == 1u,
        "exact one-key physical budget");
    cap = {}; cap.max_scopes = 4u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "scope budget"); clean(out);
    cap = {}; cap.max_ranges = 2u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "sum ranges budget"); clean(out);
    cap = {}; cap.max_depth = 1u;
    check(query_concrete_scope_path(scopes, 15u, out, cap) == scope_path_error::limit_exceeded, "complete ancestry budget"); clean(out);
    ::std::size_t used{(::std::numeric_limits<::std::size_t>::max)() - 1u};
    check(!budget::charge(2u, (::std::numeric_limits<::std::size_t>::max)(), used) &&
        used == (::std::numeric_limits<::std::size_t>::max)() - 1u, "sum overflow rejected without changing budget");
    check(query_concrete_scope_path({}, 0u, out) == scope_path_error::unavailable, "empty metadata unavailable"); clean(out);
    ::fast_io::io::println("debug_source_scope_path: PASS synthetic bounded concrete DIE path; no runtime authority/integration");
}

// Large linked-library metadata must retain inactive-record validation,
// half-open boundaries, lexical inheritance and ambiguity/error precedence.
static void owned_cases()
{
    auto original{fixture()};::std::unique_ptr<owned_scope_paths> owner{};
    check(owned_scope_paths::copy(original,owner) && owner,"bounded independent scope owner");
    auto immutable{owner->scopes()};concrete_scope_path a{},b{};
    check(query_concrete_scope_path(immutable,15u,a,{},owner.get())==scope_path_error::none,"prime owned concrete path");
    original[4].parent=99u;
    check(query_concrete_scope_path(original,15u,b,{},owner.get())==scope_path_error::malformed,"another mutable span cannot borrow cached validation");clean(b);
    check(query_concrete_scope_path(immutable,15u,b,{},owner.get())==scope_path_error::none && b.record_indices==a.record_indices,
        "original storage mutation cannot change independent owned metadata");
    a.record_indices.clear();a.physical_and_inline[0].offset=999u;
    check(query_concrete_scope_path(immutable,15u,b,{},owner.get())==scope_path_error::none && b.physical_and_inline[0].offset==101u,
        "caller result mutations never alter retained metadata path");
    for(unsigned repeat{};repeat!=3u;++repeat)for(::std::uint64_t pc{};pc!=40u;++pc)for(unsigned profile{};profile!=6u;++profile)
    {
        scope_path_limits cap{};
        if(profile==1u)cap.max_keys=1u;
        if(profile==2u)cap.max_depth=2u;
        if(profile==3u)cap.max_scopes=4u;
        if(profile==4u)cap.max_ranges=2u;
        if(profile==5u)cap.max_keys=(::std::numeric_limits<::std::size_t>::max)();
        auto raw=query_concrete_scope_path(immutable,pc,a,cap);
        auto cached=query_concrete_scope_path(immutable,pc,b,cap,owner.get());
        check(raw==cached && a.record_indices==b.record_indices && a.physical_and_inline==b.physical_and_inline,
            "PC changes, all cap fields, eviction and failures preserve original selector");
    }
    auto replacement{fixture()};replacement[4].identity.unit=33u;replacement[4].ranges={{17u,18u}};
    ::std::unique_ptr<owned_scope_paths> newer{};
    check(owned_scope_paths::copy(replacement,newer),"fresh independent metadata incarnation");
    check(query_concrete_scope_path(newer->scopes(),15u,b,{},owner.get())==scope_path_error::none && b.record_indices.size()==2u,
        "a different owner never reuses old concrete path at the same PC");
    ::std::atomic<bool> consistent{true};::std::vector<::std::thread> threads{};
    for(unsigned worker{};worker!=6u;++worker)threads.emplace_back([&]
    {
        for(unsigned i{};i!=200u;++i)
        {
            concrete_scope_path expected{},actual{};auto const pc=10u+i%20u;
            auto const raw=query_concrete_scope_path(immutable,pc,expected);
            auto const cached=query_concrete_scope_path(immutable,pc,actual,{},owner.get());
            if(raw!=cached || expected.record_indices!=actual.record_indices || expected.physical_and_inline!=actual.physical_and_inline)
                consistent.store(false,::std::memory_order_relaxed);
        }
    });
    for(auto& thread:threads)thread.join();
    check(consistent.load(::std::memory_order_relaxed),"contended cache publishes only complete independent metadata copies");
    auto malformed{fixture()};malformed[4].ranges={{14u,14u}};
    check(owned_scope_paths::copy(malformed,newer),"owning a copy never asserts semantic validity");
    check(query_concrete_scope_path(newer->scopes(),11u,b,{},newer.get())==scope_path_error::malformed,
        "inactive malformed metadata remains checked on first query");clean(b);
    auto nested{fixture()};nested[0].frame_base.resize(1u);nested[0].frame_base[0].plan.pieces.resize(1u);
    nested[0].frame_base[0].plan.pieces[0].atom.pieces.resize(1u);
    check(!owned_scope_paths::copy(nested,newer) && !newer,"nested expressions cannot trigger an unbounded metadata copy");
    auto large{fixture()};large[0].name.assign(8u*1024u*1024u,'x');
    check(!owned_scope_paths::copy(large,newer) && !newer,"independent metadata copy has a fixed aggregate memory budget");
}
int main()
{
    owned_cases();
    legacy_cases();
    std::vector<scope_record> scopes(60000u);
    for(std::size_t i{};i<scopes.size();++i)
    {
        scopes[i].identity={21u,100u+i};
        if(i) {scopes[i].parent=0u;scopes[i].kind=scope_kind::subprogram;scopes[i].concrete=true;
            scopes[i].ranges={{1000u+i*2u,1001u+i*2u}};}
    }
    scopes[0].kind=scope_kind::compile_unit;
    scopes[1].ranges={{100u,200u}};
    scopes[2].kind=scope_kind::lexical_block;scopes[2].parent=1u;scopes[2].ranges.clear();
    scopes[3].kind=scope_kind::inline_subprogram;scopes[3].parent=2u;scopes[3].ranges={{120u,180u}};
    scopes[4].kind=scope_kind::lexical_block;scopes[4].parent=3u;scopes[4].own_ranges_declared=true;scopes[4].ranges.clear();
    scopes[5].kind=scope_kind::inline_subprogram;scopes[5].parent=4u;scopes[5].ranges={{130u,170u}};
    scopes[6].kind=scope_kind::inline_subprogram;scopes[6].parent=3u;scopes[6].ranges={{130u,170u}};
    scopes[7].kind=scope_kind::inline_subprogram;scopes[7].parent=8u; // deliberately forward, corrected below.
    scopes[7].parent=0u;scopes[7].ranges={{130u,170u}};
    concrete_scope_path out{};
    check(query_concrete_scope_path(scopes,150u,out)==scope_path_error::none &&
          out.record_indices==std::vector<std::size_t>{1u,3u,6u},"large unrelated CUs and explicitly empty lexical block");
    check(query_concrete_scope_path(scopes,170u,out)==scope_path_error::none &&
          out.record_indices==std::vector<std::size_t>{1u,3u},"large graph retains half-open inline boundary");
    scopes.back().ranges={{140u,160u}};
    check(query_concrete_scope_path(scopes,150u,out)==scope_path_error::ambiguous,"late physical conflict");clean(out);
    scopes[59998u].parent=59999u;
    check(query_concrete_scope_path(scopes,150u,out)==scope_path_error::malformed,"late malformed metadata precedes physical ambiguity");clean(out);
    scopes[59998u].parent=0u;scopes.back().ranges={{999999u,1000000u}};
    scopes[5].parent=3u;
    check(query_concrete_scope_path(scopes,150u,out)==scope_path_error::ambiguous,"active overlapping sibling paths");clean(out);
    scopes[5].parent=4u;scopes[6].identity=scopes[1].identity;
    check(query_concrete_scope_path(scopes,150u,out)==scope_path_error::malformed,"large path duplicate concrete key");clean(out);
    std::vector<scope_record> deep(66u);
    for(std::size_t i{};i<deep.size();++i)
    {deep[i].identity={22u,i};deep[i].kind=scope_kind::lexical_block;if(i) {deep[i].parent=i-1u;}}
    deep[0].kind=scope_kind::subprogram;deep[0].concrete=true;deep[0].ranges={{1u,2u}};
    scope_path_limits cap{};cap.max_depth=64u;
    check(query_concrete_scope_path(std::span<scope_record const>{deep.data(),65u},1u,out,cap)==scope_path_error::none,
          "exact complete ancestry depth cap");
    check(query_concrete_scope_path(deep,1u,out,cap)==scope_path_error::limit_exceeded,"inactive deep tail still validates");clean(out);
    cap.max_depth=std::numeric_limits<std::size_t>::max();
    check(query_concrete_scope_path(deep,1u,out,cap)==scope_path_error::none,"extreme cap cannot overflow preceding-parent depth");
    ::fast_io::io::println("pretty_printer_scope_paths: PASS large graph and original fail-closed contracts");
}
