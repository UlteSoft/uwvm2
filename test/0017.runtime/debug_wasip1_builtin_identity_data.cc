// DATA/phase refusal test only. Positive identity requires the actual loader,
// final registry, initializer and fused validation epoch; not this component.
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <fast_io.h>
#include <type_traits>
namespace full=::uwvm2::uwvm::runtime::full;
static_assert(!::std::is_constructible_v<full::builtin_wasip1_loader_identity>);
static_assert(!::std::is_copy_constructible_v<full::builtin_wasip1_loader_identity>);
int main()
{
    auto source{full::full_source_instance::create_unparsed(u8"unparsed-input.wasm")};
    full::builtin_wasip1_function_data descriptor{};
    if(source->actual_builtin_wasip1_function(0u,1u,nullptr,0u,descriptor))
    { ::fast_io::io::perrln("WASIp1 builtin identity DATA FAIL: unparsed labels issued a provider");return 1; }
    if(descriptor.parameter_count != 0u || descriptor.result_count != 0u)
    { ::fast_io::io::perrln("WASIp1 builtin identity DATA FAIL: refusal mutated output");return 1; }
    ::fast_io::io::println("WASIp1 builtin identity DATA PASS runtime-authority=false implementation-owner=false");
}
