// Allocation fault at the detached diagnostic seam; DATA only, no
// canonical capture/GC/root/restore authority or guest execution is fabricated.
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <fast_io.h>
#include <cstdlib>
#include <new>
#include <type_traits>
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
static bool refuse_allocations{};
void* operator new(::std::size_t bytes)
{
    if(refuse_allocations) { throw ::std::bad_alloc{}; }
    auto const allocated{::std::malloc(bytes==0u ? 1u : bytes)};
    if(allocated==nullptr) { throw ::std::bad_alloc{}; }return allocated;
}
void operator delete(void* allocated) noexcept { ::std::free(allocated); }
void operator delete(void* allocated,::std::size_t) noexcept { ::std::free(allocated); }
static void require(bool condition,::fast_io::string_view text)
{ if(!condition) { ::fast_io::println(::fast_io::err(),"path diagnostic DATA: ",text);::fast_io::fast_terminate(); } }
int main()
{
    static_assert(::std::is_nothrow_move_constructible_v<ws::request> && ::std::is_nothrow_move_assignable_v<ws::request>);
    ws::request original{ws::selection::globals,7u,3u,0u,0u,9u,1u};
    original.member_first=17u;original.member_count=64u;original.path_session=11u;original.path_handle=13u;
    original.long_path.assign(ws::maximum_long_path,19u);require(ws::valid(original),"genuine complete owned DATA path setup");
    ws::view failure{};refuse_allocations=true;
    failure.requested=ws::unavailable_request(original);
    auto second{ws::unavailable_request(original)};
    failure.requested=::std::move(second);
    refuse_allocations=false;
    require(failure.requested.long_path.empty() && failure.requested.long_path.capacity()==0u &&
        failure.requested.selected==original.selected && failure.requested.participant==original.participant &&
        failure.requested.module==original.module && failure.requested.first==original.first &&
        failure.requested.member_first==original.member_first && failure.requested.member_count==original.member_count &&
        failure.requested.path_session==original.path_session && failure.requested.path_handle==original.path_handle,
        "no allocation under fault; scalar labels retained only as unavailable DATA");
    ws::view guarded{};bool caught{};
    try { refuse_allocations=true;guarded.requested=original; }
    catch(::std::bad_alloc const&) { refuse_allocations=false;caught=true; }
    refuse_allocations=false;require(caught,"complete owned path allocation failure must remain inside real borrow try");
    guarded.requested=original;require(guarded.requested.long_path==original.long_path,
        "success DATA preserves every original edge, not the unavailable projection");
    ::fast_io::println("DEEP_GC_PATH_DIAGNOSTIC noalloc failure DATA/fullpath guarded PASS; native authority=false");
}
