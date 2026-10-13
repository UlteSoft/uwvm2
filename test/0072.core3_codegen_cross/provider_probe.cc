#include <fast_io.h>
#include <atomic>
#include <bit>
#include <cstdint>
#include <thread>

int main()
{
    std::atomic<std::uint64_t> value{7};
    std::thread worker{[&value] { for(unsigned i{}; i != 1024; ++i) { value.fetch_add(1); } }};
    worker.join();
    if(value.load() != 1031) { return 1; }
    auto const f32{std::bit_cast<std::uint32_t>(0.5f + 0.25f)};
    auto const f64{std::bit_cast<std::uint64_t>(0.5 + 0.25)};
    if(f32 != 0x3f400000u || f64 != 0x3fe8000000000000ull) { return 2; }
    fast_io::io::println("provider ptr=", sizeof(void*) * 8,
        " endian=", fast_io::mnp::cond(std::endian::native == std::endian::little, "le", "be"),
        " f32=", f32, " f64=", f64, " atomic=", value.load());
}
