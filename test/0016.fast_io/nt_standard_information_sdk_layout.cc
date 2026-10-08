#include <cstddef>
#include <cstdint>
#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
// Build against the ACTUAL Windows target SDK with NOMINMAX and the supported
// _WIN32_WINNT. A private guessed test structure is not an ABI oracle.
#include <windows.h>
#ifdef UWVM_TEST_IMPORT_FAST_IO
import fast_io;
#else
#include <fast_io.h>
#endif
int main()
{
    using native = ::FILE_STANDARD_INFO;
    using provider = ::fast_io::win32::nt::file_standard_information;
    constexpr auto native_allocation{offsetof(native, AllocationSize)};
    constexpr auto native_end{offsetof(native, EndOfFile)};
    constexpr auto native_links{offsetof(native, NumberOfLinks)};
    constexpr auto native_delete{offsetof(native, DeletePending)};
    constexpr auto native_directory{offsetof(native, Directory)};
    constexpr auto provider_allocation{offsetof(provider, allocation_size)};
    constexpr auto provider_end{offsetof(provider, end_of_file)};
    constexpr auto provider_links{offsetof(provider, number_of_links)};
    constexpr auto provider_delete{offsetof(provider, delete_pending)};
    constexpr auto provider_directory{offsetof(provider, directory)};
    ::fast_io::println(::fast_io::out(), "sdk FILE_STANDARD_INFO size=", sizeof(native), " align=", alignof(native),
        " allocation=", native_allocation, " end=", native_end, " links=", native_links,
        " delete=", native_delete, " directory=", native_directory,
        " delete-width=", sizeof(decltype(native::DeletePending)), " directory-width=", sizeof(decltype(native::Directory)));
    ::fast_io::println(::fast_io::out(), "provider file_standard_information size=", sizeof(provider), " align=", alignof(provider),
        " allocation=", provider_allocation, " end=", provider_end, " links=", provider_links,
        " delete=", provider_delete, " directory=", provider_directory,
        " delete-width=", sizeof(decltype(provider::delete_pending)), " directory-width=", sizeof(decltype(provider::directory)));
    constexpr bool match{sizeof(native) == sizeof(provider) && alignof(native) == alignof(provider) &&
        native_allocation == provider_allocation && native_end == provider_end && native_links == provider_links &&
        native_delete == provider_delete && native_directory == provider_directory &&
        sizeof(decltype(native::AllocationSize)) == sizeof(decltype(provider::allocation_size)) &&
        sizeof(decltype(native::EndOfFile)) == sizeof(decltype(provider::end_of_file)) &&
        sizeof(decltype(native::NumberOfLinks)) == sizeof(decltype(provider::number_of_links)) &&
        sizeof(decltype(native::DeletePending)) == sizeof(decltype(provider::delete_pending)) &&
        sizeof(decltype(native::Directory)) == sizeof(decltype(provider::directory))};
    ::fast_io::println(::fast_io::out(), "nt standard information SDK layout match=", static_cast<unsigned>(match));
    // The original production structure is expected to expose a mismatch.
    // Keep that ACTUAL native counterexample and rerun this unchanged oracle
    // on the separately approved correction; never alter expectations to green.
    return match ? 0 : 1;
}
#else
int main() { return 77; }
#endif
