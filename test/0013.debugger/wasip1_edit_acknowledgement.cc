// Detached native formatter packets. No environment or mutation authority.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <fast_io_unit/string.h>
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
int main(int argc,char const**)
{
    if(argc!=1 && argc!=2) { return 91; }
    constexpr ws::action actions[]{ws::action::replace_argument,ws::action::insert_argument,
        ws::action::remove_argument,ws::action::set_environment,ws::action::remove_environment,
        ws::action::reduce_rights,ws::action::create_file,ws::action::duplicate_descriptor,ws::action::close_descriptor};
    unsigned checks{};
    for(unsigned operation{};operation!=9u;++operation)
    {
        for(unsigned variant{};variant!=4u;++variant)
        {
            ws::view data{};data.operation=actions[operation];data.module=0u;
            data.observed_runtime_epoch=UINT64_MAX;data.shared_environment=true;data.total_entries=2u;
            data.result=variant<2u ? ws::status::ok : ws::status::entry_not_found;
            data.mutation_applied=(variant&1u)==0u;data.affected_descriptor=INT32_MAX;
            ::fast_io::string packet{};ws::print(::fast_io::ostring_ref_fast_io{__builtin_addressof(packet)},data);
            auto expected=::fast_io::concat_fast_io("wasip1 module=0 status=",ws::status_text(data.result),
                " epoch=18446744073709551615 applied=",data.mutation_applied?1u:0u,
                " shared-environment=1 total=2\n");
            if(operation>=6u && data.mutation_applied) { expected.append("wasip1-fd affected=2147483647\n"); }
            ++checks;if(packet!=expected) { ::fast_io::io::perrln("edit formatter mismatch ",operation," ",variant);return 1; }
            ::fast_io::string hex{};hex.reserve(packet.size()*2u);constexpr char digits[]{"0123456789abcdef"};
            for(char c:packet) { auto b=static_cast<unsigned char>(c);hex.push_back(digits[b>>4u]);hex.push_back(digits[b&15u]); }
            ::fast_io::io::println("PACKET operation=",operation," variant=",variant," hex=",hex);
        }
    }
    ::fast_io::io::println("wasip1_edit_acknowledgement ",checks," checks passed cases=1 unsupported=0 phase=post");
}
