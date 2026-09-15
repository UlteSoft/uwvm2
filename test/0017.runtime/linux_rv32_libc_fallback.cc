// RV32 has no old lseek/clock_settime syscall numbers. Merely including
// fast_io used to fail before any floating-point test could run. Check the
// libc seek fallback above 32 bits without allocating an 8-GiB file; the
// timestamp setter is compiled/address-taken only (never change a real clock).
#include <fast_io.h>
#include <cstdio>
#include <cstdint>

int main()
{
    auto* file = std::tmpfile();
    if(!file) { return 1; }
    auto fd = ::fileno(file);
    constexpr std::uint64_t offset{(std::uint64_t{1} << 33) + 7};
    auto result = fast_io::details::posix_seek_impl(fd, offset, fast_io::seekdir::beg);
    auto observed = fast_io::details::posix_seek_impl(fd, 0, fast_io::seekdir::cur);
    auto reset = fast_io::details::posix_seek_impl(fd, 0, fast_io::seekdir::beg);
    // The volatile function pointer keeps the fallback body instantiated at O3
    // too, while avoiding a setter invocation or a privileged system mutation.
    auto volatile setter = &fast_io::posix_clock_settime;
    bool okay = result == offset && observed == offset && reset == 0 && setter != nullptr;
    okay = std::fclose(file) == 0 && okay;
    return !okay;
}
