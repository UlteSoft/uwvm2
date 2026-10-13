// Cold effective policy projection regressions. No fabricated world/stop proof.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
namespace st = ::uwvm2::uwvm::runtime::storage;
using limits_type = ::uwvm2::uwvm::wasm::type::module_memory_limit_t;
static_assert(st::checkpoint_effective_memory_bounds(false, limits_type{.min=1u,.max=8u,.present_max=true}).minimum == 1u);
static_assert(st::checkpoint_effective_memory_bounds(false, limits_type{.min=1u,.max=8u,.present_max=true}).maximum == 8u);
static_assert(st::checkpoint_effective_memory_bounds(false, limits_type{.min=3u,.max=3u,.present_max=true}).maximum == 3u);
static_assert(st::checkpoint_effective_memory_bounds(false, limits_type{}).maximum == 65536u);
static_assert(st::checkpoint_effective_memory_bounds(true, limits_type{}).maximum == (::std::uint64_t{1u} << 48u));
static void require(bool passed)
{ if(!passed) { ::fast_io::io::perrln("checkpoint effective memory policy FAIL"); ::fast_io::fast_terminate(); } }
int main()
{
    // Declaration 2..4 deliberately is not a projection input: ordinary
    // configured policy replaces it with 1..8, and warned widening is allowed.
    auto const widened{st::checkpoint_effective_memory_bounds(false,{.min=1u,.max=8u,.present_max=true})};
    require(widened.minimum==1u && widened.maximum==8u);
    auto const narrowed{st::checkpoint_effective_memory_bounds(false,{.min=3u,.max=3u,.present_max=true})};
    require(narrowed.minimum==3u && narrowed.maximum==3u);
    auto const address32{st::checkpoint_effective_memory_bounds(false,{.min=0u,
        .max=(::std::numeric_limits<::std::size_t>::max)(),.present_max=true})};
    require(address32.minimum==0u && address32.maximum==65536u);
    auto const absent{st::checkpoint_effective_memory_bounds(true,{.min=0u,.max=0u,.present_max=false})};
    require(absent.minimum==0u && absent.maximum==(::std::uint64_t{1u} << 48u));
    // An impossible effective minimum is not silently normalized. The real
    // census/allocation consumer compares pages to min/max and must reject it.
    auto const invalid{st::checkpoint_effective_memory_bounds(false,{.min=65537u,.max=0u,.present_max=false})};
    require(invalid.minimum>invalid.maximum);
    ::fast_io::io::println("checkpoint effective memory policy PASS");
}
