// Run with a read-only Wasm input and a new file path on a writable volume.
// All file operations use FastIO; Windows additionally exercises native paths
// and verifies that ordinary read handles retain timestamp access.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <string_view>
#include <stdexcept>
#include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>

int main(int argc, char** argv)
{
    if(argc != 5) { return 64; }
    unsigned checks{};
    auto require = [&](bool condition, char const* label)
    {
        ++checks;
        fast_io::io::println("READONLY_CHECK ", checks, " ", fast_io::mnp::os_c_str(label), " ",
                             fast_io::mnp::os_c_str(condition ? "PASS" : "FAIL"));
        if(!condition) { throw std::runtime_error{label}; }
    };
    constexpr auto mode{fast_io::open_mode::in | fast_io::open_mode::follow | fast_io::open_mode::no_write_attributes};
    auto input_path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(argv[1]))};
    auto writable_path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(argv[2]))};
    fast_io::native_file file{input_path, mode};
    auto const stat{fast_io::status(file)};
    require(stat.type == fast_io::file_type::regular && stat.size >= 8u, "regular read-only input");
    char bytes[8]{};
    fast_io::operations::read_all(file, bytes, bytes + 8u);
    require(std::string_view{bytes, 4u} == std::string_view{"\0asm", 4u}, "Wasm binary read through FastIO");
    fast_io::native_file_loader mapping{input_path, mode};
    require(mapping.size() == stat.size && std::string_view{mapping.data(), 8u} == std::string_view{bytes, 8u},
            "read-only native mapping matches file bytes");
#if defined(_WIN32) && !defined(__CYGWIN__)
    auto const ordinary{fast_io::win32::nt::details::calculate_nt_open_mode({fast_io::open_mode::in, fast_io::perms::owner_all})};
    auto const minimal{fast_io::win32::nt::details::calculate_nt_open_mode(
        {fast_io::open_mode::in | fast_io::open_mode::no_write_attributes, fast_io::perms::owner_all})};
    require((ordinary.DesiredAccess & 0x100u) != 0u, "default read retains FILE_WRITE_ATTRIBUTES");
    require((ordinary.DesiredAccess ^ minimal.DesiredAccess) == 0x100u, "explicit flag removes only attribute write access");
    require(ordinary.ShareAccess == minimal.ShareAccess && ordinary.CreateDisposition == minimal.CreateDisposition &&
            ordinary.CreateOptions == minimal.CreateOptions && ordinary.FileAttributes == minimal.FileAttributes &&
            ordinary.ObjAttributes == minimal.ObjAttributes, "disposition, sharing, following and object flags preserved");
    auto native_path{fast_io::concat_fast_io("\\??\\", fast_io::mnp::os_c_str(argv[1]))};
    fast_io::native_file_loader native_mapping{fast_io::io_kernel, native_path, mode};
    require(native_mapping.size() == mapping.size() &&
            std::string_view{native_mapping.data(), native_mapping.size()} == std::string_view{mapping.data(), mapping.size()},
            "native NT path read-only mapping matches Win32 path");
    {
        fast_io::native_file created{writable_path, fast_io::open_mode::out | fast_io::open_mode::creat | fast_io::open_mode::excl};
        fast_io::operations::write_all(created, bytes, bytes + 8u);
    }
    fast_io::native_file ordinary_file{writable_path, fast_io::open_mode::in};
    constexpr std::uint_least64_t timestamp{133485408000000000ull};
    fast_io::win32::nt::file_basic_information update{.LastWriteTime = timestamp};
    fast_io::win32::nt::io_status_block isb{};
    auto const status{fast_io::win32::nt::nt_set_information_file<false>(ordinary_file.native_handle(), &isb, &update,
        sizeof(update), fast_io::win32::nt::file_information_class::FileBasicInformation)};
    require(status == 0u, "default read handle still updates timestamp");
    fast_io::win32::nt::file_basic_information observed{};
    auto const query{fast_io::win32::nt::nt_query_information_file<false>(ordinary_file.native_handle(), &isb, &observed,
        sizeof(observed), fast_io::win32::nt::file_information_class::FileBasicInformation)};
    require(query == 0u && observed.LastWriteTime == timestamp, "timestamp update read back from OS");
    fast_io::native_file minimal_file{writable_path, mode};
    auto const refused{fast_io::win32::nt::nt_set_information_file<false>(minimal_file.native_handle(), &isb, &update,
        sizeof(update), fast_io::win32::nt::file_information_class::FileBasicInformation)};
    require(refused == 0xc0000022u, "explicit read-only handle refuses attribute update");
#endif
    namespace portable = uwvm2::uwvm::debugger::wasip1_portable;
    auto capsule_path{fast_io::u8concat_fast_io(fast_io::mnp::code_cvt(fast_io::mnp::os_c_str(argv[3])))};
    auto group_path{fast_io::u8concat_fast_io(fast_io::mnp::code_cvt(fast_io::mnp::os_c_str(argv[4])))};
    portable::snapshot capsule{};
    require(portable::load_file(portable::text_view{capsule_path.data(), capsule_path.size()}, capsule), "portable capsule imported from read-only input");
    portable::group_snapshot group{};
    std::vector<std::byte> single_wire{}, grouped_wire{};
    require(portable::load_group_file(portable::text_view{group_path.data(), group_path.size()}, group) && group.environments.size() == 1u &&
            portable::encode(capsule, single_wire) && portable::encode(group.environments.front(), grouped_wire) &&
            single_wire == grouped_wire, "portable group imported from read-only input preserves environment");
    fast_io::io::println("fast_io_readonly_attributes PASS checks=", checks);
}
