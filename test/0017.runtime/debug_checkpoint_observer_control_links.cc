// Pure compiler-DATA forward-link checks only. No native execution/capture
// permission is inferred from these layouts. Actual VM coverage is a separate
// ALL-fresh-runtime native test of modern syntax and real stopped values.
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_observer_control_map.h>
#include <fast_io.h>
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool okay,char const* message)
{ if(!okay) { ::fast_io::io::perrln("observer control links: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    cp::function_plan p{}; p.profile=cp::compilation_profile::create_for_trusted_observer();
    p.expression_bytes=64u; p.function_generation=1u;
    llvm_jit_checkpoint_observer_control_map builder{&p};
    auto const function{builder.begin(0u)}; auto const first_block{builder.begin(0u)};
    auto const nested_if{builder.begin(8u)};
    require(function==0u && first_block==1u && nested_if==2u,"real compiler lexical IDs distinguish implicit function from opcode0 block");
    cp::safepoint_layout site{}; site.identifier=1u; site.opcode_offset=12u; site.operand_count=1u; site.saved_parameter_count=1u;
    site.slots={{{cp::types::value_kind::i32},true},{{cp::types::value_kind::i32},true}};
    site.controls={{cp::control_kind::function,0u,63u,0u,0u,0u},
                   {cp::control_kind::block,0u,SIZE_MAX,0u,0u,0u},
                   {cp::control_kind::if_then,8u,SIZE_MAX,0u,0u,1u}};
    site.controls[2u].declared_parameters={{cp::types::value_kind::i32}};
    site.handlers={{true,true,0u,1u,SIZE_MAX,{}}};
    p.sites.push_back(site);
    require(cp::validate_plan(p)==cp::status::invalid_layout && !cp::sealed_function_plan::seal_compiler_metadata(p),"pending ends cannot be sealed or interpreted as runtime layouts");
    require(builder.validate_tentative(p.sites[0u])==cp::status::ok && builder.link_site(0u),"private fused compiler can validate exact typed payload while its own forward ends are pending");
    require(builder.close(nested_if,24u) && p.sites[0u].controls[2u].end_offset==24u && p.sites[0u].controls[1u].end_offset==SIZE_MAX,"actual inner end patches only its own cells");
    require(!builder.close(nested_if,25u),"a second guessed end cannot replace an already exact end");
    require(builder.close(first_block,40u) && p.sites[0u].controls[1u].end_offset==40u && p.sites[0u].handlers[0u].target_offset==40u,"actual target end patches owned handler continuation too");
    require(builder.close(function,63u) && builder.complete() && cp::validate_plan(p)==cp::status::ok,"all source ends closed before seal");
    auto const owned{cp::sealed_function_plan::seal_compiler_metadata(p)};
    require(bool(owned) && owned->get().resume_sites.empty() && owned->get().resume_abi_revision==0u,"observation DATA has no executable continuation");
    auto bad{site}; bad.controls[1u].entry_offset=7u;
    require(builder.validate_tentative(bad)==cp::status::invalid_layout,"unowned pending source scope cannot manufacture a layout");
    ::fast_io::io::println("observer-control-link component PASS; actual native execution=false");
}
