// ONE actual INT fused walk -> original ring + real checked LLVM SSA.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/shared/i32_dual_emission.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <type_traits>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT) || defined(UWVM2TEST_STRICT_NO_INTERPRETER)
#error "Use genuine original INT runner and matching INT+LLVM provider closure."
#endif
namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    namespace dual=::uwvm2::runtime::compiler::shared::i32_dual_emission;
    namespace full=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    using error_code=::uwvm2::validation::error::code_validation_error_code;
#if defined(UWVM2TEST_SELECT_CACHED_RING)
    constexpr auto option{make_tailcall_fully_split_opt<3uz,3uz,8uz,8uz>()};
    static_assert(option.is_tail_call && option.i32_stack_top_begin_pos != SIZE_MAX &&
        option.i64_stack_top_begin_pos != SIZE_MAX && option.i32_stack_top_begin_pos != option.i32_stack_top_end_pos &&
        option.i64_stack_top_begin_pos != option.i64_stack_top_end_pos);
#elif defined(UWVM2TEST_SELECT_MEMORY_TAIL)
    constexpr auto option{k_test_tail_min_opt};
    static_assert(option.is_tail_call && option.i32_stack_top_begin_pos == SIZE_MAX && option.i64_stack_top_begin_pos == SIZE_MAX);
#else
    constexpr auto option{k_test_byref_opt};
    static_assert(!option.is_tail_call && option.i32_stack_top_begin_pos == SIZE_MAX && option.i64_stack_top_begin_pos == SIZE_MAX);
#endif
    template<typename> struct first_argument;
    template<typename R,typename C,typename A> struct first_argument<R(C::*)(A)> { using type=::std::remove_cvref_t<A>; };
    using native_name_type=typename first_argument<decltype(&::llvm::ExecutionEngine::getFunctionAddress)>::type;
    void require(bool valid,char const* text)
    { if(!valid) { ::fast_io::print(::fast_io::err(),"real typed select dual: ",::fast_io::mnp::os_c_str(text),"\n"); ::fast_io::fast_terminate(); } }
    byte_vec load(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path),::fast_io::open_mode::in};
        require(input.size()>=8uz && input.size()<=65536uz,"actual complete source extent");byte_vec result(input.size());
        // [RAII retained source][same-size owned bytes] end; checked sizes BEFORE copy.
        ::fast_io::freestanding::my_memcpy(result.data(),input.data(),input.size());return result;
    }
    auto features(bool gc_off=false)
    {
        auto selected{make_wasm1p1_feature_parameter()};auto& f{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(selected)};
        f.disable_simd=false;f.disable_exceptions=false;f.disable_gc=gc_off;f.disable_function_references=gc_off;return selected;
    }
    template<typename Bits> void append(byte_vec& bytes,Bits value)
    {
        auto const offset{bytes.size()};require(offset<=SIZE_MAX-sizeof(Bits),"sum before allocation");bytes.resize(offset+sizeof(Bits));
        require(sizeof(Bits)<=bytes.size()-offset,"actual complete tight ABI cell BEFORE pointer advance");
        // [owned prefix][new sizeof(Bits) cell] end; exact host ABI memcpy, not serialized LE.
        ::fast_io::freestanding::my_memcpy(bytes.data()+offset,::std::addressof(value),sizeof(Bits));
    }
    template<typename R> R ring_result(dual::checked_module<option> const& artifact,runtime_module_t const& module,
                                     ::std::size_t index,byte_vec const& parameters={})
    {
        require(index<artifact.ring_artifact().local_funcs.size() && index<module.local_defined_function_vec_storage.size(),"actual ring function owner bounds");
        auto result{interpreter_runner<option>::run(artifact.ring_artifact().local_funcs.index_unchecked(index),
            module.local_defined_function_vec_storage.index_unchecked(index),parameters,nullptr,nullptr)};
        require(result.results.size()==sizeof(R),"complete typed original ring result ABI");R value{};
        ::fast_io::freestanding::my_memcpy(::std::addressof(value),result.results.data(),sizeof(R));return value;
    }
    template<typename R,typename... Args> R native_result(dual::checked_module<option>& artifact,runtime_module_t const& module,
                                                         ::std::size_t index,Args... arguments)
    {
        require(artifact.native_ir_available(index),"actual one-walk SSA available");auto fragment{artifact.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder && !artifact.native_ir_available(index),"one real context/module move");
        auto const name{full::details::get_llvm_wasm_function_name(module,static_cast<::std::uint_least32_t>(index))};
        auto* definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        require(definition && !definition->isDeclaration() && definition->arg_size()==sizeof...(Args) &&
            definition->getReturnType()->isIntegerTy(sizeof(R)*8u) && definition->getCallingConv()==full::details::get_llvm_jit_wasm_calling_conv(),"actual result/arity/CC prototype");
        ::std::array<unsigned,sizeof...(Args)> widths{{static_cast<unsigned>(sizeof(Args)*8u)...}};::std::size_t i{};
        for(auto const& argument:definition->args()) { require(i<widths.size() && argument.getType()->isIntegerTy(widths[i]),"each actual mixed argument width");++i; }
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module,true),"real verifier");
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
        require(target && target->createDataLayout()==fragment.llvm_module->getDataLayout(),"actual target DL");
#if LLVM_VERSION_MAJOR >= 21
        require(target->getTargetTriple()==fragment.llvm_module->getTargetTriple(),"actual triple");
#else
        require(target->getTargetTriple().str()==fragment.llvm_module->getTargetTriple(),"actual triple");
#endif
        auto context{::std::move(fragment.llvm_context_holder)};auto ir{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::unique_ptr<::llvm::Module>{ir.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(target.release())};
        require(bool(engine),"real matching owned engine");engine->finalizeObject();
        require(!name.empty() && name.size()<=static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),"symbol extent before end iterator");
        // [actual owned complete symbol] end; extent proved BEFORE end-pointer formation.
        native_name_type native_name{name.data(),name.data()+name.size()};auto const address{engine->getFunctionAddress(native_name)};
        require(address && address<=(::std::numeric_limits<::std::uintptr_t>::max)(),"actual finalized symbol");
        using entry=R(UWVM2TEST_WASM_ABI*)(Args...);return reinterpret_cast<entry>(static_cast<::std::uintptr_t>(address))(arguments...);
    }
    void positive(char const* path,bool gc_off)
    {
        auto bytes{load(path)};auto selected{features(gc_off)};auto module{prepare_runtime_from_wasm(bytes,u8"one-real-typed-select",{},selected)};
        require(module.mod && module.mod->local_defined_function_vec_storage.size()==(gc_off?1uz:10uz),"actual modern declared functions");
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};require(bool(target),"live target owner");
        dual::emission_policy policy{};policy.target=target.get();policy.emit_typed_select=true;
        ::uwvm2::validation::error::code_validation_error_impl error{};auto artifact{dual::compile<option>(*module.mod,{},policy,error,&selected)};
        require(artifact.typed_admission_complete() && error.err_code==error_code::ok,"all actual typed bodies and original ring fixups before seal");
        if(!gc_off)
        {
            require(ring_result<::std::uint32_t>(artifact,*module.mod,0uz)==11u && native_result<::std::uint32_t>(artifact,*module.mod,0uz)==11u,"actual i32 true selection");
            require(ring_result<::std::uint64_t>(artifact,*module.mod,1uz)==42ull && native_result<::std::uint64_t>(artifact,*module.mod,1uz)==42ull,"actual i64 false selection");
            byte_vec narrow{};append(narrow,::std::uint32_t{11u});append(narrow,::std::uint32_t{22u});append(narrow,::std::uint32_t{1u});
            byte_vec mixed{};append(mixed,::std::uint64_t{0xffffffffffffffffull});append(mixed,::std::uint64_t{42ull});append(mixed,::std::uint32_t{0u});
            require(narrow.size()==12uz && ring_result<::std::uint32_t>(artifact,*module.mod,2uz,narrow)==11u &&
                native_result<::std::uint32_t>(artifact,*module.mod,2uz,::std::uint32_t{11u},::std::uint32_t{22u},::std::uint32_t{1u})==11u,"real3 cached i32 slots/prototype");
            require(mixed.size()==20uz && ring_result<::std::uint64_t>(artifact,*module.mod,3uz,mixed)==42ull &&
                native_result<::std::uint64_t>(artifact,*module.mod,3uz,::std::uint64_t{0xffffffffffffffffull},::std::uint64_t{42ull},::std::uint32_t{0u})==42ull,"real20-byte mixed ring params vs typed native ABI");
            for(::std::size_t local{4uz};local!=10uz;++local) { require(!artifact.native_ir_available(local),"reference/SIMD/control no fake native seal"); }
            auto off{policy};off.emit_typed_select=false;auto old{dual::compile<option>(*module.mod,{},off,error,&selected)};
            require(old.typed_admission_complete() && !old.native_ir_available(0uz) && ring_result<::std::uint32_t>(old,*module.mod,0uz)==11u,"false policy retains prior slice and real ring result");
            auto limited{policy};limited.module_operations=0uz;auto quota{dual::compile<option>(*module.mod,{},limited,error,&selected)};
            require(quota.typed_admission_complete() && !quota.native_ir_available(0uz),"zero IR quota not module invalidity");
        }
        else { require(!artifact.native_ir_available(0uz),"legal GC-off exn body only loses native subset"); }
        auto const start_index{gc_off?0uz:9uz};auto result{interpreter_runner<option>::run(artifact.ring_artifact().local_funcs.index_unchecked(start_index),
            module.mod->local_defined_function_vec_storage.index_unchecked(start_index),{},nullptr,nullptr)};
        require(result.results.empty(),"genuine called start executes numeric SIMD GC typed-select or GC-off noexn subtype");
    }
    void negative(char const* path,error_code expected)
    {
        auto bytes{load(path)};auto selected{features()};auto module{prepare_runtime_from_wasm(bytes,u8"real-unused-select",{},selected)};
        require(module.mod && module.mod->local_defined_function_vec_storage.size()==2uz,"unused body follows actual empty start");
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};require(bool(target),"matching actual target");
        dual::emission_policy policy{};policy.target=target.get();policy.emit_typed_select=true;::uwvm2::validation::error::code_validation_error_impl error{};bool rejected{};
        try { auto refused{dual::compile<option>(*module.mod,{},policy,error,&selected)};(void)refused; }
        catch(::fast_io::error const& actual)
        { if(actual.domain!=::fast_io::parse_domain_value || actual.code!=static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }rejected=true; }
        auto const code{module.mod->local_defined_function_vec_storage.index_unchecked(1uz).wasm_code_ptr};require(code,"actual unused body owner");
        auto const begin{reinterpret_cast<::std::byte const*>(code->body.expr_begin)},end{reinterpret_cast<::std::byte const*>(code->body.code_end)};
        require(rejected && error.err_code==expected && error.err_curr>=begin && error.err_curr<end,"precise actual first-walk unused-body diagnostic");
    }
}
int main(int argc,char const* const* argv)
{
    require(argc==9,"positive6unusednegative+GCoff actual source paths");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),"actual matching native provider initialization");
    install_unexpected_traps();positive(argv[1],false);
    negative(argv[2],error_code::select_cond_type_not_i32);negative(argv[3],error_code::select_type_mismatch);
    negative(argv[4],error_code::select_type_mismatch);negative(argv[5],error_code::operand_stack_underflow);
    negative(argv[6],error_code::select_type_mismatch);negative(argv[7],error_code::select_cond_type_not_i32);positive(argv[8],true);
    ::fast_io::print(::fast_io::out(),"REAL_TYPED_SELECT_DUAL actual_numeric=4 modern_functions=6 GCoff=1 ring+native=matched full_tiered_qualified=0\n");
}
