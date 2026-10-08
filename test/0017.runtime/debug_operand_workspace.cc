// Native storage DATA regression; no copied ID/PC/owner grants preview.
#include <uwvm2/runtime/lib/uwvm_runtime_debug_operand_workspace.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io.h>
#include <array>
#include <limits>
namespace lib = ::uwvm2::runtime::lib::details;
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool valid,char const* message)
{ if(!valid) { ::fast_io::io::perrln("debug operand workspace: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    require(!cp::native_workspace_fits(1001u,1003u) && cp::observer_workspace_fits(1001u,1003u),
        "heap observation preserves native resume workspace limit");
    require(!cp::observer_workspace_fits((::std::numeric_limits<::std::size_t>::max)(),
        (::std::numeric_limits<::std::size_t>::max)()),"flags and slot extent overflow rejected");
    lib::debug_operand_workspace_stack storage{};
    require(storage.size()==0u && storage.enter(0u).empty() &&
        storage.enter((::std::numeric_limits<::std::size_t>::max)()).empty(),"empty/overflow allocation rejected without ownership change");
    ::std::array<::std::byte*,80u> addresses{};
    constexpr ::std::size_t bytes{10001u*2u + 10003u*cp::native_slot_bytes};
    for(::std::size_t n{}; n!=addresses.size(); ++n)
    {
        auto const workspace{storage.enter(bytes)};
        require(workspace.size()==bytes && workspace.front()==::std::byte{} && workspace.back()==::std::byte{},"full heap extent initialized");
        addresses[n]=workspace.data();workspace.back()=::std::byte{static_cast<unsigned char>(n+1u)};
        for(::std::size_t i{}; i<=n; ++i)
        { require(addresses[i][bytes-1u]==::std::byte{static_cast<unsigned char>(i+1u)},"nested frame vector growth retains each actual heap buffer/value"); }
    }
    require(!storage.leave(0u) && !storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[0u])) && storage.size()==80u,
        "wrong or non-top owner cannot retire any buffer");
    // Storage cleanup stays possible after diagnostic identity quotas; the
    // data-only class cannot grant a complete Wasm capture at that depth.
    for(::std::size_t n{addresses.size()};n!=0u;--n)
    { require(storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[n-1u])) && storage.size()==n-1u,"return/tail/EH producer pops the exact top native buffer"); }
    require(!storage.leave(reinterpret_cast<::std::uintptr_t>(addresses[0u])),"retired owner cannot release twice");
    auto const fresh{storage.enter(1u)};require(fresh.size()==1u && storage.leave(reinterpret_cast<::std::uintptr_t>(fresh.data())),"zero-slot function still owns one unique buffer");
    ::fast_io::io::println("debug operand workspace heap/LIFO/80-frames/overflow DATA PASS");
}
