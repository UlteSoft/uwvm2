// Real compiler-owned user containers for stopped HOST printer configuration tests.
using size_type = __SIZE_TYPE__;
namespace application
{
    template<class T> struct Sequence { T* data; size_type length; size_type capacity; };
    struct Text { char const* data; size_type length; };
    struct Owner { int* ptr; };
    struct Maybe { bool engaged; int value; };
}
volatile int source_print_configuration_result;
extern "C" [[gnu::noinline]] int pretty_configuration_probe(int seed)
{
    int backing[3]{seed,seed+1,seed+2};
    application::Sequence<int> values{backing,3u,3u};
    application::Text text{"bounded text",12u};
    application::Owner owner{backing+1u};
    application::Maybe present{true,seed+2},absent{false,99};
    asm volatile("" : : : "memory");
    source_print_configuration_result = values.data[0u] + present.value; // PRETTY_CONFIGURATION_STOP
    return values.length != 3u || values.capacity != 3u || values.data[2u] != seed+2 || text.length != 12u ||
        text.data[0u] != 'b' || *owner.ptr != seed+1 || !present.engaged || absent.engaged || present.value != seed+2;
}
int main() { return pretty_configuration_probe(7); }
