#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_expression.h>
#include <array>
#include <vector>

int main()
{
    namespace d = uwvm2::uwvm::debugger::source_dwarf;
    using status = d::immutable_local_status;
    unsigned checks{};
    auto check = [&](auto const& bytes, auto const& bitmap, std::uint64_t local, status expected)
    {
        ++checks;
        if(d::immutable_wasm_local(bytes,bitmap,local) != expected) { fast_io::fast_terminate(); }
    };
    std::array<std::byte,4> write{std::byte{0x22},std::byte{0},std::byte{0x01},std::byte{0x0b}};
    std::array<std::uint_least8_t,1> complete{0x0d};
    check(write,complete,0,status::overwritten);
    check(write,complete,1,status::intact);
    write[0]=std::byte{0x21};check(write,complete,0,status::overwritten);
    // A local.tee byte inside an i32 constant immediate is not a write.
    std::array<std::byte,3> immediate{std::byte{0x41},std::byte{0x22},std::byte{0x0b}};
    std::array<std::uint_least8_t,1> immediate_boundaries{0x05};
    check(immediate,immediate_boundaries,0,status::intact);
    std::array<std::byte,7> padded{std::byte{0x22},std::byte{0x80},std::byte{0x80},std::byte{0x80},std::byte{0x80},std::byte{0},std::byte{0x0b}};
    std::array<std::uint_least8_t,1> padded_boundaries{0x41};
    check(padded,padded_boundaries,0,status::overwritten);
    padded[5]=std::byte{0x10};check(padded,padded_boundaries,0,status::unavailable); // u32 overflow
    padded[5]=std::byte{0x80};check(padded,padded_boundaries,0,status::unavailable); // overlong LEB
    std::array<std::uint_least8_t,1> false_immediate_boundary{0x0f};
    check(write,false_immediate_boundary,0,status::unavailable);
    std::array<std::uint_least8_t,1> invalid_tail{0x8d};
    check(write,invalid_tail,0,status::unavailable);
    std::array<std::uint_least8_t,0> missing{};
    check(write,missing,0,status::unavailable);
    check(write,complete,256,status::unavailable);
    std::array<std::byte,1> truncated{std::byte{0x21}};
    std::array<std::uint_least8_t,1> first{1};
    check(truncated,first,0,status::unavailable);
    std::vector<std::byte> too_large(65537,std::byte{0x01});
    std::vector<std::uint_least8_t> large_bits(8193,0);
    check(too_large,large_bits,0,status::unavailable);
    fast_io::io::println("PASS immutable local bitmap/LEB validity ",checks," cases; no live read authority");
}
