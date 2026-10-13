// Compliant native plugin: stable static tables and the documented byte-packed ABI.
#include <uwvm2/uwvm/wasm/type/dl.h>
#include <uwvm2/uwvm/wasm/type/wasip1_api.h>
#include <cstring>
using namespace uwvm2::uwvm::wasm::type;
static uwvm_wasip1_host_api_v1 const* wasi_api{};
extern "C" void uwvm_set_wasip1_host_api_v1(uwvm_wasip1_host_api_v1 const* api) noexcept { wasi_api=api; }
static void add(::std::byte* result, ::std::byte* parameters) noexcept {
 ::std::uint_least32_t lhs{},rhs{},sum{};
 ::std::memcpy(&lhs,parameters,4); ::std::memcpy(&rhs,parameters+4,4);
 sum=lhs+rhs; ::std::memcpy(result,&sum,4);
}
static void wasi_sizes(::std::byte* result, ::std::byte* parameters) noexcept {
 ::std::uint_least32_t count_offset{},size_offset{};
 ::std::memcpy(&count_offset,parameters,4); ::std::memcpy(&size_offset,parameters+4,4);
 auto const error = wasi_api == nullptr ? ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable : wasi_api->args_sizes_get(count_offset,size_offset);
 auto const code=static_cast<::std::uint_least32_t>(error); ::std::memcpy(result,&code,4);
}
static ::std::uint_least8_t const i32_args[]{0x7f,0x7f},i32_results[]{0x7f};
static capi_function_t const functions[]{
 {"add",3,i32_args,2,i32_results,1,add},
 {"sizes",5,i32_args,2,i32_results,1,wasi_sizes}
};
extern "C" capi_module_name_t uwvm_get_module_name() noexcept { return {"compliant",9}; }
extern "C" capi_function_vec_t uwvm_function() noexcept { return {functions,2}; }
