#include <uwvm2/uwvm/debugger/source_map.h>
#include <array>
#include <string_view>

int main()
{
    struct sample { ::std::string_view path; bool absolute; };
    ::std::array const cases{
        sample{"", false}, sample{"C", false}, sample{"C:", true},
        sample{"c:relative", true}, sample{"Z:/file", true}, sample{"a:\\file", true},
        sample{"0:relative", false}, sample{"[:file", false}, sample{"-:file", false},
        sample{"./file", false}, sample{"/file", true}, sample{"\\file", true},
        sample{::std::string_view{"\0:", 2u}, false},
        sample{::std::string_view{"\xc3\xa9:", 3u}, false},
        sample{::std::string_view{"\x80:", 2u}, false},
    };
    for(auto const& current : cases)
    {
        if(::uwvm2::uwvm::debugger::source_map_details::absolute_path(current.path) != current.absolute)
        { return 1; }
    }
    ::fast_io::println(::fast_io::out(), "PASS fixed ASCII source drive-letter policy");
}
