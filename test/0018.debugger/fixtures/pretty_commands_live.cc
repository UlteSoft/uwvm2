// uwvm-dbg-pretty-commands-live-v1
// Compile unchanged with the actual WASI libc++ producer: -std=c++20 -O0 -g
// -fno-exceptions -fno-omit-frame-pointer. No synthetic DWARF or ABI layout.
#include <cstddef>
#include <string>
#include <vector>

namespace command_live
{
    template<class T> struct CommandSequence { T* data; std::size_t size,cap; };
    struct CommandText { char const* textdata; std::size_t size; };
    struct CommandPointer { int* ptr; };
    struct CommandOptional { bool engaged; int value; };
}

volatile int pretty_commands_live_sink;

extern "C" [[gnu::noinline]] int pretty_commands_live_probe(int seed)
{
    std::vector<int> builtin_values{seed,seed+1,seed+2,seed+3,seed+4};
    std::string builtin_text{"builtin command text"};
    int backing[5]{41,42,43,44,45};
    char const message[]{"custom bounded text"};
    command_live::CommandSequence<int> custom_sequence{backing,5u,5u};
    command_live::CommandText custom_text{message,sizeof(message)-1u};
    command_live::CommandPointer custom_pointer{backing+2u};
    command_live::CommandOptional present{true,73},absent{false,99};
    int sentinel{seed+1000};
    int progress{0};
    asm volatile("" : : : "memory");
    pretty_commands_live_sink = sentinel + builtin_values[0]; // PRETTY_COMMANDS_LIVE_STOP
    ++progress;
    asm volatile("" : : : "memory");
    return seed!=7 || sentinel!=1007 || progress!=1 || builtin_values.size()!=5u ||
        builtin_values[0]!=7 || builtin_values[4]!=11 || builtin_text!="builtin command text" ||
        custom_sequence.data!=backing || custom_sequence.size!=5u || custom_sequence.cap!=5u ||
        backing[0]!=41 || backing[1]!=42 || backing[2]!=43 || backing[3]!=44 || backing[4]!=45 ||
        custom_text.textdata!=message || custom_text.size!=19u || message[0]!='c' || message[18]!='t' ||
        *custom_pointer.ptr!=43 || !present.engaged || present.value!=73 || absent.engaged || absent.value!=99;
}

int main() { return pretty_commands_live_probe(7); }
