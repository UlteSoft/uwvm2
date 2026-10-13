// Consume an actual Clang-built existing C/C++ fixture and a named fbreg root.
// The keeper must retain compiler/source/module SHA and official dwarfdump
// showing terminal stack_value. This checks metadata only, never a live read.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
static void check(bool condition, char const* message)
{ if(!condition) { ::fast_io::io::perrln("frame-base-index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc, char const* const* argv)
{
    check(argc == 3, "require actual Clang Wasm path and an existing fbreg variable name");
    ::std::string_view const wanted{argv[2]}; check(!wanted.empty(), "named producer root required");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    check(file.size() >= 8u, "Wasm header bounds");
    // [RAII loader bytes ... eight-byte header ... module_end]
    // [safe                                                  ] unsafe (one-past)
    //  ^^ exact owner extent and minimum size precede all bounded header reads.
    auto const bytes{::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()), file.size()}};
    ::std::array<unsigned char, 8u> const magic{0u, 0x61u, 0x73u, 0x6du, 1u, 0u, 0u, 0u};
    for(::std::size_t i{}; i != magic.size(); ++i)
    { check(::std::to_integer<unsigned char>(bytes[i]) == magic[i], "valid Wasm version1 header"); }
    details::reader module{bytes, 8u}; ::std::vector<section> sections{}; ::std::uint64_t code_size{};
    while(module.cursor != module.bytes.size())
    {
        ::std::uint8_t id{}; ::std::uint32_t size{};
        check(module.byte(id) && module.leb(size) && size <= module.bytes.size() - module.cursor, "bounded section payload");
        // [safe] FastIO decoded the length; range check precedes subspan/advance.
        auto const payload{module.bytes.subspan(module.cursor, size)}; module.cursor += size;
        if(id == 10u) { check(code_size == 0u, "one code section"); code_size = size; }
        if(id != 0u) { continue; }
        details::reader custom{payload}; ::std::uint32_t length{};
        check(custom.leb(length) && length <= payload.size() - custom.cursor, "bounded custom-section name");
        // [custom payload ... cursor ... cursor+length ... end]
        // [safe                                              ] unsafe (one-past)
        //                    ^^ checked length precedes the name pointer derivation.
        ::std::string_view const name{reinterpret_cast<char const*>(payload.data() + custom.cursor), length};
        custom.cursor += length; // checked scalar advance may reach payload_end.
        if(name.starts_with(".debug_") || name == "external_debug_info")
        { sections.push_back({name, payload.subspan(custom.cursor)}); }
    }
    check(code_size != 0u, "actual code section present"); ::std::unique_ptr<metadata_index> parsed{};
    check(metadata_index::parse({sections, code_size, 4u}, parsed) == error::none && parsed, "actual producer embedded DWARF parsed");
    auto const scopes{parsed->scopes()}; ::std::size_t roots{};
    for(auto const& variable : parsed->variables())
    {
        if(variable.name != wanted) { continue; }
        bool fbreg{};
        for(auto const& location : variable.locations)
        { fbreg |= location.plan.kind == plan_kind::frame_relative_offset && location.plan.reason == unavailable_reason::none; }
        if(!fbreg) { continue; }
        auto physical{variable.scope};
        for(::std::size_t depth{}; physical != no_record; ++depth)
        {
            check(physical < scopes.size() && depth < 32u, "bounded owned parent scope chain");
            // [owned source scopes ... checked physical index] end
            // [safe                                          ] metadata borrow only;
            //  ^^ no caller/native-frame address or local carrier is available here.
            auto const& scope{scopes[physical]};
            if(scope.kind == scope_kind::subprogram) { break; }
            physical = scope.parent;
        }
        check(physical != no_record && scopes[physical].concrete, "actual concrete physical scope of fbreg root");
        bool usable_base{};
        for(auto const& location : scopes[physical].frame_base)
        {
            if(location.plan.kind != plan_kind::wasm_local_frame_base || location.plan.reason != unavailable_reason::none) { continue; }
            check(location.plan.address_bytes == 4u, "existing fixture Wasm32 frame-base carrier width");
            usable_base = true;
            ::fast_io::io::println("actual-frame-base root=", ::fast_io::mnp::code_cvt(wanted),
                " local=", ::fast_io::mnp::dec(location.plan.local_index), " kind=wasm_local_frame_base");
        }
        check(usable_base, "actual Clang frame_base must retain the consumer's frame-base role");
        ++roots;
    }
    check(roots != 0u, "actual named fbreg root required; unsupported/composite location is not a passing fallback");
    ::fast_io::io::println("PASS actual Clang fbreg/frame-base metadata role only; runtime guest-memory qualification separate");
}
