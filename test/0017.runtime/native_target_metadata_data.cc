// DATA-only structural negative corpus. No native/code-owner/stop authority.
#include <uwvm2/uwvm/debugger/native_target_metadata.h>
#include <array>
#include <cstddef>
#include <cstdint>
struct description
{
    ::std::array<char, 16u> triple{'x','8','6','_','6','4','\0'};
    ::std::array<char, 8u> cpu{};
    ::std::array<char, 8u> features{};
    ::std::size_t triple_size{6u}, cpu_size{}, features_size{};
    unsigned description_version{1u}, pointer_bits{64u}, maximum_instruction_bytes{15u}, minimum_instruction_alignment{1u};
    bool little_endian{true}, available{true};
};
namespace data = ::uwvm2::uwvm::debugger::native_target_metadata;
static_assert(data::valid(description{}));
static_assert(!data::valid([] { description d{}; d.description_version = 0u; return d; }()));
static_assert(!data::valid([] { description d{}; d.description_version = 2u; return d; }()));
static_assert(!data::valid([] { description d{}; d.available = false; return d; }()));
static_assert(!data::valid([] { description d{}; d.triple_size = 16u; return d; }()));
static_assert(!data::valid([] { description d{}; d.cpu_size = 8u; return d; }()));
static_assert(!data::valid([] { description d{}; d.features_size = 8u; return d; }()));
static_assert(!data::valid([] { description d{}; d.triple_size = 0u; return d; }()));
static_assert(!data::valid([] { description d{}; d.triple[3u] = '\0'; return d; }()));
static_assert(!data::valid([] { description d{}; d.triple[6u] = 'X'; return d; }()));
static_assert(!data::valid([] { description d{}; d.pointer_bits = 16u; return d; }()));
static_assert(!data::valid([] { description d{}; d.maximum_instruction_bytes = 0u; return d; }()));
static_assert(!data::valid([] { description d{}; d.maximum_instruction_bytes = 33u; return d; }()));
static_assert(!data::valid([] { description d{}; d.minimum_instruction_alignment = 0u; return d; }()));
static_assert(!data::valid([] { description d{}; d.minimum_instruction_alignment = 3u; return d; }()));
static_assert(!data::valid([] { description d{}; d.minimum_instruction_alignment = 16u; return d; }()));
static_assert(data::valid([] { description d{}; d.maximum_instruction_bytes = 32u; d.minimum_instruction_alignment = 4u; return d; }()));
int main() {}
