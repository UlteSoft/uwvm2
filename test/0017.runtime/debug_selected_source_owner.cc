// Native root comparison DATA only; no returned pin, stop or admission grant.
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <fast_io.h>
#include <memory>
namespace full = ::uwvm2::uwvm::runtime::full;
static void require(bool valid, char const* message)
{ if(!valid) { ::fast_io::io::perrln("selected source owner: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    auto first{full::full_source_instance::create_unparsed(u8"first.wasm")};
    auto second{full::full_source_instance::create_unparsed(u8"second.wasm")};
    full::full_source_instance::owner old{first}, next{second};
    require(!full::selected_full_source_matches_owner({}) &&
        !full::selected_full_source_matches_owner(old),"no selected root rejects even a canonical source");
    require(full::select_unparsed_full_source_after_drain(first),"serialized native setup selects canonical root");
    auto const strong_count{old.use_count()};
    for(unsigned n{};n!=128u;++n)
    { require(full::selected_full_source_matches_owner(old) && old.use_count()==strong_count,
        "lexical comparison never retains an extra lifetime owner"); }
    auto marker{::std::make_shared<int>(0)};
    full::full_source_instance::owner foreign{marker,old.get()};
    full::full_source_instance::owner wrong_pointer{old,next.get()};
    full::full_source_instance::owner unreadable{marker,
        reinterpret_cast<full::full_source_instance const*>(::std::uintptr_t{1u})};
    require(!full::selected_full_source_matches_owner(foreign) &&
        !full::selected_full_source_matches_owner(wrong_pointer) &&
        !full::selected_full_source_matches_owner(unreadable) &&
        !full::selected_full_source_matches_owner(next),"pointer and controlblock are both required before any candidate read");
    full::retire_selected_full_source_after_drain();
    require(!full::selected_full_source_matches_owner(old),"a retained old owner cannot substitute for a drained root");
    require(full::select_unparsed_full_source_after_drain(second) &&
        full::selected_full_source_matches_owner(next) && !full::selected_full_source_matches_owner(old),
        "new selected root rejects the previous canonical controlblock");
    full::retire_selected_full_source_after_drain();
    ::fast_io::io::println("selected source pointer/controlblock/retirement/replacement DATA PASS");
}
