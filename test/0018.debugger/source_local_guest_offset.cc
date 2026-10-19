// Portable copied-DATA regression. This decodes locations and returns scalar
// guest offsets only; it issues no source/stop/VM/memory-read authority.
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
static void require(bool value,unsigned line)
{
    if(!value) { std::fprintf(stderr,"local guest offset DATA assertion line=%u\n",line);std::abort(); }
}
#define REQUIRE(x) require(bool(x),__LINE__)
int main()
{
    for(unsigned width:{4u,8u})
    {
        std::array<std::byte,3u> expression{std::byte{0xed},std::byte{0},std::byte{1}};
        auto plan=d::decode_single_location_plan(expression,width);
        REQUIRE(plan.kind==d::plan_kind::wasm_local_guest_offset && plan.reason==d::unavailable_reason::none);
        std::array<d::copied_numeric_local,2u> locals{};auto& local=locals[1u];
        local.available=true;local.wasm_type=width==4u?0x7fu:0x7eu;
        std::uint64_t const expected=width==4u?0xfedcba98ull:0xfedcba9876543210ull;
        // Captured scalar carriers use the native host representation. Guest
        // memory bytes have a separate Wasm little-endian decoding contract.
        if(width==4u) { auto value=static_cast<std::uint32_t>(expected);std::memcpy(local.bytes.data(),&value,sizeof(value)); }
        else { std::memcpy(local.bytes.data(),&expected,sizeof(expected)); }
        std::uint64_t offset=42u;
        REQUIRE(d::resolve_local_guest_offset(plan,locals,3u,offset)==d::object_location_error::none && offset==expected);
        auto check=[&](d::object_location_error error) {
            offset=42u;REQUIRE(d::resolve_local_guest_offset(plan,locals,3u,offset)==error && offset==0u);
        };
        local.available=false;check(d::object_location_error::local_unavailable);local.available=true;
        local.wasm_type=0x7du;check(d::object_location_error::carrier_mismatch);local.wasm_type=width==4u?0x7fu:0x7eu;
        plan.local_index=2u;check(d::object_location_error::local_not_captured);plan.local_index=1u;
        offset=42u;REQUIRE(d::resolve_local_guest_offset(plan,locals,1u,offset)==d::object_location_error::malformed_metadata && offset==0u);
        auto original=plan;
        plan.storage=d::wasm_location_space::global;check(d::object_location_error::malformed_metadata);plan=original;
        plan.integer_transform_count=1u;check(d::object_location_error::malformed_metadata);plan=original;
        plan.displacement=1;check(d::object_location_error::malformed_metadata);plan=original;
        plan.constant_bits=1u;check(d::object_location_error::malformed_metadata);plan=original;
        plan.indirect_addend=1;check(d::object_location_error::malformed_metadata);plan=original;
        plan.byte_count=1u;check(d::object_location_error::malformed_metadata);plan=original;
        plan.pieces.push_back({});check(d::object_location_error::malformed_metadata);plan=original;
        plan.reason=d::unavailable_reason::no_location;check(d::object_location_error::malformed_metadata);plan=original;
        plan.address_bytes=16u;check(d::object_location_error::malformed_metadata);plan=original;
        for(unsigned space:{1u,2u})
        {
            expression[1u]=std::byte(space);auto rejected=d::decode_single_location_plan(expression,width);
            REQUIRE(rejected.kind==d::plan_kind::unavailable);
        }
        std::array<std::byte,4u> value_expression{std::byte{0xed},std::byte{0},std::byte{1},std::byte{0x9f}};
        plan=d::decode_single_location_plan(value_expression,width);
        REQUIRE(plan.kind==d::plan_kind::wasm_local_value);
        check(d::object_location_error::unsupported_location);
    }
    ::fast_io::io::println("PASS local guest offset DATA native-carrier-endianness wasm32-wasm64 prefix availability malformed no-read-authority");
}
