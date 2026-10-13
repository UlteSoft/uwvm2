// Compile once with the hosted header and once with
// -DUWVM_TEST_IMPORT_FAST_IO and the fresh named-module BMI.
#include <memory>
#include <string>
#include <string_view>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

int main()
{
    auto path{::fast_io::concat_std(::std::string_view{"src"},
                                  ::fast_io::mnp::cond(true, "/"), ::std::string_view{"main.cpp"})};
    ::fast_io::ostring_ref_std output{::std::addressof(path)};
    ::fast_io::io::print(output, ":", ::fast_io::mnp::dec(42u));
    if(path != "src/main.cpp:42") { return 1; }
    auto adapted{::fast_io::io_strlike_ref(::fast_io::io_alias, path)};
    ::fast_io::io::print(adapted, " end");
    if(path != "src/main.cpp:42 end") { return 2; }
    if(::fast_io::concat_std(::std::string_view{"a\0b", 3u}, "c") != ::std::string{"a\0bc", 4u}) { return 3; }
    if(::fast_io::concatln_std("line", ::fast_io::mnp::dec(7u)) != "line7\n") { return 4; }

    auto u8{::fast_io::u8concat_std(u8"module", ::std::u8string_view{u8".wasm"})};
    ::fast_io::u8ostring_ref_std u8out{::std::addressof(u8)};
    ::fast_io::io::print(u8out, u8":", ::fast_io::mnp::dec(9u));
    if(u8 != u8"module.wasm:9" || ::fast_io::u8concatln_std(u8"u8") != u8"u8\n") { return 5; }

    auto u16{::fast_io::u16concat_std(u"unit", ::std::u16string_view{u".cc"})};
    ::fast_io::u16ostring_ref_std u16out{::std::addressof(u16)};
    ::fast_io::io::print(u16out, u":", ::fast_io::mnp::dec(3u));
    if(u16 != u"unit.cc:3" || ::fast_io::u16concatln_std(u"u16") != u"u16\n") { return 6; }

    auto u32{::fast_io::u32concat_std(U"unit", ::std::u32string_view{U".cc"})};
    ::fast_io::u32ostring_ref_std u32out{::std::addressof(u32)};
    ::fast_io::io::print(u32out, U":", ::fast_io::mnp::dec(4u));
    if(u32 != U"unit.cc:4" || ::fast_io::u32concatln_std(U"u32") != U"u32\n") { return 7; }

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
    auto wide{::fast_io::wconcat_std(L"unit", ::std::wstring_view{L".cc"})};
    ::fast_io::wostring_ref_std wideout{::std::addressof(wide)};
    ::fast_io::io::print(wideout, L":", ::fast_io::mnp::dec(5u));
    if(wide != L"unit.cc:5" || ::fast_io::wconcatln_std(L"wide") != L"wide\n") { return 8; }
#endif
    ::fast_io::println(::fast_io::out(), "PASS public standard-string concat/ref");
}
