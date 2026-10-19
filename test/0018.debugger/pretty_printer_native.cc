// Real implementation-owned C++ objects for independent GDB/LLDB controls.
#include <string>
#include <string_view>
#include <vector>

::std::vector<int> values;
::std::string title{"native pretty printer"};
::std::string_view view{"length-delimited view"};
volatile int pretty_native_result;

extern "C" [[gnu::noinline]] void pretty_printer_ready()
{
#if defined(__wasm__)
    asm volatile("" : : : "memory");
#else
    asm volatile("" : : "g"(values.data()), "g"(title.data()), "g"(view.data()) : "memory");
#endif
    pretty_native_result = values[0u] + (title.size() != 21u || view.size() != 21u || view[0u] != 'l'); // PRETTY_NATIVE_STOP
}

int main()
{
    values.reserve(32u);
    for(int i{};i!=16;++i) { values.push_back(i*3+1); }
    pretty_printer_ready();
    return values[0u]!=1 || pretty_native_result!=1;
}
