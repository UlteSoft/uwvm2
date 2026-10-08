// Actual parsed/initialized/validated Core 3 Wasm-to-LLVM lowering. The linked
// SDK optimizes and executes it. A test-owned debug callback collects while
// the one native guest thread is synchronously stopped, using real static and
// generated frame roots. This is not automatic/multithreaded VM collection.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include <uwvm_int_translate_strict_common.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/gc/impl.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
#include <uwvm2/uwvm/runtime/storage/gc_static_roots.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace strict = ::uwvm2test::uwvm_int_strict;
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace details = compiler::details;
namespace frames = ::uwvm2::runtime::gc;
namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace container = ::uwvm2::utils::container;

namespace
{
    ::std::size_t checks{}, collections{}, reclaimed_total{}, max_depth{};
    ::std::vector<::std::shared_ptr<storage::gc_object_store>> cohort{};
    ::std::vector<storage::wasm_module_storage_t const*> modules{};
    bool force_collection{};
    bool expect_empty_activation{};

    ::uwvm2::runtime::exception::guest_exception make_guest_probe()
    {
        auto tag{::std::make_shared<int>(17)};
        auto value{::uwvm2::runtime::exception::value::make(tag, {})};
        return ::uwvm2::runtime::exception::guest_exception{::std::move(value)};
    }

    void require(bool condition, char const* label) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL precise LLVM frames: ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }

    // Exact five-uintptr_t ABI of the real product debug safe-point bridge.
    // MCJIT's per-engine mapping replaces only that callback in this probe.
    // It does not pretend that the product's debugger supplies a GC handshake.
    void collect_at_debug_point(::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t,
        ::std::uintptr_t, ::std::uintptr_t) noexcept
    {
        if(!force_collection) { return; }
        ::std::vector<storage::gc_reference> roots{};
        auto const append{[&](storage::gc_reference value) noexcept
        { roots.push_back(value); return true; }};
        auto const active{frames::visit_quiescent_frame_roots(frames::current_root_frames(), append)};
        require(active.status == frames::frame_root_status::ok, "complete native frame chain validates");
        require(expect_empty_activation ? active.frames == 0uz : active.frames != 0uz,
            "exactly reference-free numeric activations retain no empty root record");
        max_depth = (::std::max)(max_depth, active.frames);
        auto const static_{storage::visit_quiescent_cohort_static_roots(
            {modules.data(), modules.size()}, append)};
        require(static_.status == storage::gc_static_root_status::ok,
            "all actual initialized module static roots validate");
        ::std::size_t reclaimed{};
        require(storage::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), cohort.size(),
            roots.data(), roots.size(), reclaimed) == storage::gc_object_status::ok,
            "actual closed aggregate collector accepts the complete snapshot");
        ++collections;
        reclaimed_total += reclaimed;
    }

    [[nodiscard]] ::std::string name(container::u8string const& value)
    {
        // [complete bounded UTF-8 name] end
        // [safe                     ] copy its bytes; retain no borrowed view.
        return {reinterpret_cast<char const*>(value.data()), value.size()};
    }
    template<auto Bridge>
    [[nodiscard]] bool bridge(::llvm::Function const& function)
    {
        auto const prefix{details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
        return function.getName().starts_with(::llvm::StringRef{
            reinterpret_cast<char const*>(prefix.data()), prefix.size()});
    }
    void verify_fixed_gc_bridge_identity(::llvm::Module& module, bool enabled, bool optimized)
    {
        auto const i32{::llvm::Type::getInt32Ty(module.getContext())};
        auto const i64{::llvm::Type::getInt64Ty(module.getContext())};
        auto const intptr{::llvm::IntegerType::get(module.getContext(),
            static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
        auto const allocation_type{::llvm::FunctionType::get(i32, {intptr, i32, i32, intptr, intptr}, false)};
        auto const scalar_type{::llvm::FunctionType::get(i64, {intptr, i32, intptr, i32}, false)};
        constexpr char8_t new_tag[]{u8"gc_fixed_0_1"};
        constexpr char8_t unsigned_tag[]{u8"gc_struct_get32_unsigned"};
        constexpr char8_t signed_tag[]{u8"gc_struct_get32_signed"};
        auto const expected_new{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_aggregate_fixed_bridge<0u, 1uz>>(allocation_type,
                container::u8string_view{new_tag, sizeof(new_tag) / sizeof(char8_t) - 1uz})};
        auto const expected_get{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_struct_get32_bridge<false>>(scalar_type,
                container::u8string_view{unsigned_tag, sizeof(unsigned_tag) / sizeof(char8_t) - 1uz})};
        auto const expected_signed{details::get_llvm_runtime_bridge_function_symbol_name<
            details::llvm_jit_gc_struct_get32_bridge<true>>(scalar_type,
                container::u8string_view{signed_tag, sizeof(signed_tag) / sizeof(char8_t) - 1uz})};
        auto const new_name{name(expected_new)};
        auto const get_name{name(expected_get)};
        auto const signed_name{name(expected_signed)};
        require(new_name != get_name && get_name != signed_name && new_name != signed_name,
            "actual fixed allocation and four-argument scalar signed/unsigned semantic identities are distinct");
        auto const new_declaration{module.getFunction(new_name)};
        auto const get_declaration{module.getFunction(get_name)};
        // Both preserved WAT fixtures contain unpacked i32 struct.get, hence
        // the UNSIGNED helper is actually emitted. Merely computing the signed
        // name is not execution coverage of struct.get_s/packed fields.
        auto const signed_declaration{module.getFunction(signed_name)};
        require(new_declaration != nullptr && get_declaration != nullptr &&
            new_declaration != get_declaration && new_declaration->isDeclaration() &&
            get_declaration->isDeclaration() && new_declaration->getFunctionType() == allocation_type &&
            get_declaration->getFunctionType() == scalar_type && signed_declaration == nullptr &&
            get_declaration->getCallingConv() == ::llvm::CallingConv::C,
            "actual emitter uses five-arg allocation and i64-return four-arg scalar C ABI; unused signed declaration absent");
        ::std::size_t new_calls{}, get_calls{};
        for(auto const& function : module)
        {
            for(auto const& block : function) for(auto const& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                auto const callee{call->getCalledFunction()};
                new_calls += callee == new_declaration;
                if(callee == get_declaration)
                {
                    ++get_calls;
                    require(call->arg_size() == 4u && call->getType() == i64 &&
                        call->getArgOperand(0u)->getType() == intptr && call->getArgOperand(1u)->getType() == i32 &&
                        call->getArgOperand(2u)->getType() == intptr && call->getArgOperand(3u)->getType() == i32 &&
                        call->getCallingConv() == ::llvm::CallingConv::C && call->doesNotThrow(),
                        "actual scalar read transports module/kind/token/field in four registers and is nounwind");
                }
            }
        }
        require(new_calls != 0uz && get_calls != 0uz,
            "actual fixed allocation and scalar read declarations have real source-derived call sites");
        ::fast_io::io::println("PASS updated scalar32 GC bridge identities enabled=", enabled ? 1 : 0,
            " optimized=", optimized ? 1 : 0,
            " new=", ::fast_io::mnp::os_c_str(new_name.c_str()),
            " get=", ::fast_io::mnp::os_c_str(get_name.c_str()),
            " signed_not_exercised=", ::fast_io::mnp::os_c_str(signed_name.c_str()),
            " new_calls=", new_calls, " get_calls=", get_calls);
    }
    // Inspect the actual UNOPTIMIZED product LLVM instruction graph. The
    // old runtime collection callbacks remain the execution proof after O3;
    // this extra audit neither manufactures IR nor guesses pointer roots.
    void verify_precise_root_publication(::llvm::Module& module, ::llvm::Function const* numeric,
        bool enabled, bool has_real_eh)
    {
        ::std::size_t records{}, slot_stores{}, publications{}, exceptional_leaves{};
        constexpr auto live_offset{offsetof(frames::root_frame, live_count)};
        constexpr auto carrier_bits{static_cast<unsigned>(sizeof(frames::root_reference) * CHAR_BIT)};
        for(auto& function : module)
        {
            if(function.empty()) { continue; }
            ::llvm::CallBase const* enter{};
            ::llvm::AllocaInst const* record{};
            ::llvm::AllocaInst const* slots{};
            ::llvm::ConstantInt const* capacity{};
            for(auto const& block : function) for(auto const& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr || call->getCalledFunction() == nullptr ||
                   !bridge<frames::uwvm_gc_root_frame_enter_checked_abi>(*call->getCalledFunction())) { continue; }
                require(enabled && ::std::addressof(function) != numeric && enter == nullptr && call->arg_size() == 3u,
                    "one genuine nonempty record construction per reference-owning native function");
                enter = call;
                auto const frame_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(call->getArgOperand(0u))};
                auto const slot_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(call->getArgOperand(1u))};
                capacity = ::llvm::dyn_cast<::llvm::ConstantInt>(call->getArgOperand(2u));
                record = frame_bits == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::AllocaInst>(frame_bits->getPointerOperand());
                slots = slot_bits == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::AllocaInst>(slot_bits->getPointerOperand());
                require(record != nullptr && slots != nullptr && capacity != nullptr && !capacity->isZero() &&
                    capacity->getZExtValue() <= frames::frame_root_details::max_capacity &&
                    record->getAllocatedType()->isIntegerTy(8u) && slots->getAllocatedType()->isIntegerTy(carrier_bits) &&
                    record->getAlign().value() >= alignof(frames::root_frame) &&
                    slots->getAlign().value() >= alignof(frames::root_reference),
                    "actual entry owns fresh aligned byte-record and complete integer carrier slots with final positive capacity");
                auto const frame_extent{::llvm::dyn_cast<::llvm::ConstantInt>(record->getArraySize())};
                auto const slot_extent{::llvm::dyn_cast<::llvm::ConstantInt>(slots->getArraySize())};
                require(frame_extent != nullptr && slot_extent != nullptr &&
                    frame_extent->getZExtValue() == sizeof(frames::root_frame) &&
                    slot_extent->getValue() == capacity->getValue(),
                    "finalized real entry extents agree with native ABI record and maximum typed snapshot");
                ++records;
            }
            ::llvm::DominatorTree dominance{function};
            for(auto const& block : function) for(auto const& instruction : block)
            {
                bool const named_root{instruction.getName().starts_with("gc.root.")};
                require((enabled && ::std::addressof(function) != numeric) || !named_root,
                    "disabled and pure numeric lowering have no retained root allocations/addresses/loads");
                auto const store{::llvm::dyn_cast<::llvm::StoreInst>(::std::addressof(instruction))};
                if(store != nullptr && slots != nullptr &&
                   ::llvm::getUnderlyingObject(store->getPointerOperand()) == slots)
                {
                    auto const address{::llvm::dyn_cast<::llvm::GetElementPtrInst>(store->getPointerOperand())};
                    auto const index{address == nullptr || address->getNumIndices() != 1u ? nullptr :
                        ::llvm::dyn_cast<::llvm::ConstantInt>(address->getOperand(1u))};
                    require(store->getValueOperand()->getType()->isIntegerTy(carrier_bits) &&
                        address != nullptr && address->getPointerOperand() == slots && index != nullptr &&
                        index->getValue().ult(capacity->getValue()) &&
                        store->getAlign().value() >= alignof(frames::root_reference) && dominance.dominates(enter, store),
                        "actual typed root slot stores one full aligned carrier inside final native extent after construction");
                    ++slot_stores;
                }
                auto const gep{store == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::GetElementPtrInst>(store->getPointerOperand())};
                if(gep != nullptr && record != nullptr && gep->getPointerOperand() == record && gep->getNumIndices() == 1u)
                {
                    auto const offset{::llvm::dyn_cast<::llvm::ConstantInt>(gep->getOperand(1u))};
                    if(offset != nullptr && offset->getZExtValue() == live_offset)
                    {
                        auto const count{::llvm::dyn_cast<::llvm::ConstantInt>(store->getValueOperand())};
                        require(count != nullptr && count->getValue().ule(capacity->getValue()) &&
                            dominance.dominates(enter, store),
                            "actual live-count publication is bounded by finalized capacity and dominated by construction");
                        ::std::vector<bool> written(static_cast<::std::size_t>(count->getZExtValue()));
                        // [same LLVM-owned block instructions ... publication]
                        // [safe                                            ]
                        // Each predecessor borrow is valid until this audit ends;
                        // getPrevNode reaches null only at this block's beginning.
                        for(auto const* previous{store->getPrevNode()}; previous != nullptr; previous = previous->getPrevNode())
                        {
                            auto const field{::llvm::dyn_cast<::llvm::StoreInst>(previous)};
                            if(field == nullptr) { continue; }
                            auto const destination{::llvm::dyn_cast<::llvm::GetElementPtrInst>(field->getPointerOperand())};
                            if(destination == nullptr || destination->getNumIndices() != 1u) { continue; }
                            auto const at{::llvm::dyn_cast<::llvm::ConstantInt>(destination->getOperand(1u))};
                            if(at == nullptr) { continue; }
                            if(destination->getPointerOperand() == record && at->getZExtValue() == live_offset) { break; }
                            if(destination->getPointerOperand() == slots && at->getZExtValue() < written.size())
                            { written[static_cast<::std::size_t>(at->getZExtValue())] = true; }
                        }
                        require(::std::all_of(written.begin(), written.end(), [](bool value) noexcept { return value; }),
                            "every actual published live slot was rewritten by this same-block typed snapshot before its count");
                        ++publications;
                    }
                }
                if(enter != nullptr && (::llvm::isa<::llvm::ResumeInst>(instruction) ||
                    ::llvm::isa<::llvm::ReturnInst>(instruction)))
                {
                    bool retired{};
                    // [same LLVM-owned block ... native terminator] block end
                    // [safe                                       ] walk only
                    // live predecessor handles; never cross a block or retain
                    // them through optimization/erasure of this real module.
                    for(auto const* previous{instruction.getPrevNode()}; previous != nullptr; previous = previous->getPrevNode())
                    {
                        auto const cleanup{::llvm::dyn_cast<::llvm::CallBase>(previous)};
                        if(cleanup != nullptr && cleanup->getCalledFunction() != nullptr &&
                           bridge<frames::uwvm_gc_root_frame_leave_checked_abi>(*cleanup->getCalledFunction()))
                        {
                            require(cleanup->arg_size() == 1u, "actual root leave has its exact one-argument ABI");
                            auto const frame_bits{::llvm::dyn_cast<::llvm::PtrToIntInst>(cleanup->getArgOperand(0u))};
                            require(frame_bits != nullptr && frame_bits->getPointerOperand() == record &&
                                cleanup->doesNotThrow(),
                                "actual native exit retires its own allocation despite distinct ptrtoint SSA handles");
                            retired = true; break;
                        }
                    }
                    require(retired, "real normal/tail/exceptional native exit contains checked root retirement");
                    exceptional_leaves += ::llvm::isa<::llvm::ResumeInst>(instruction);
                }
            }
        }
        require(enabled ? (records != 0uz && slot_stores != 0uz && publications != 0uz) :
            (records == 0uz && slot_stores == 0uz && publications == 0uz),
            "actual graph has complete typed stores/publication only for enabled nonempty records");
        require(!enabled || !has_real_eh || exceptional_leaves != 0uz,
            "actual GC+EH fixture has native exceptional cleanup witnesses");
    }
    void write_ir(::llvm::Module const& module, char const* directory, char const* file_name)
    {
        container::u8string text{};
        details::raw_uwvm_string_ostream stream{text};
        module.print(stream, nullptr); stream.flush();
        auto const path{container::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/",
            ::fast_io::mnp::os_c_str(file_name))};
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::out};
        // [owned complete IR bytes] end
        // [safe                   ] form one-past only within that allocation.
        auto const begin{reinterpret_cast<::std::byte const*>(text.data())};
        ::fast_io::operations::write_all_bytes(file, begin, begin + text.size());
    }
    void optimize(::llvm::Module& module, ::llvm::TargetMachine* machine)
    {
        ::llvm::LoopAnalysisManager loop{};
        ::llvm::FunctionAnalysisManager function{};
        ::llvm::CGSCCAnalysisManager cgscc{};
        ::llvm::ModuleAnalysisManager analysis{};
        ::llvm::PassBuilder passes{machine};
        passes.registerLoopAnalyses(loop); passes.registerFunctionAnalyses(function);
        passes.registerCGSCCAnalyses(cgscc); passes.registerModuleAnalyses(analysis);
        passes.crossRegisterProxies(loop, function, cgscc, analysis);
        auto pipeline{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3)};
        pipeline.run(module, analysis);
        require(!::llvm::verifyModule(module, ::std::addressof(::llvm::errs())), "same SDK verifies optimized IR");
    }

    // Observe the exact object MCJIT compiles and executes. Never satisfy an
    // engine request with a cached object, a separately generated object or a
    // hand-written IR stub; the runner disassembles these actual bytes.
    class native_object_witness final : public ::llvm::ObjectCache
    {
        container::string path_{};
    public:
        ::std::size_t objects{};
        native_object_witness(char const* directory, bool enabled)
            : path_{container::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/",
                ::fast_io::mnp::os_c_str(enabled ? "roots.native.o" : "no-roots.native.o"))} {}
        void notifyObjectCompiled(::llvm::Module const*, ::llvm::MemoryBufferRef object) override
        {
            require(objects++ == 0uz && object.getBufferSize() != 0uz,
                "exactly one newly compiled actual native Wasm object is captured");
            ::fast_io::native_file file{::fast_io::mnp::os_c_str(path_.c_str()), ::fast_io::open_mode::out};
            // [complete MCJIT-owned object bytes] one-past
            // [safe                            ] copy before this callback's
            // borrowed memory buffer expires; do not retain its native address.
            auto const begin{reinterpret_cast<::std::byte const*>(object.getBufferStart())};
            ::fast_io::operations::write_all_bytes(file, begin, begin + object.getBufferSize());
        }
        ::std::unique_ptr<::llvm::MemoryBuffer> getObject(::llvm::Module const*) override { return {}; }
    };
}

int main(int argc, char** argv)
{
    require(argc == 3, "arguments are exact compiled Wasm fixture and output directory");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),
        "actual linked SDK native target initialized");
    ::llvm::EngineBuilder target{};
    target.setOptLevel(::llvm::CodeGenOptLevel::Aggressive).setCodeModel(::llvm::CodeModel::Large);
    ::std::unique_ptr<::llvm::TargetMachine> machine{target.selectTarget()};
    require(machine != nullptr, "actual SDK provides a native target");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    strict::byte_vec wasm{};
    wasm.resize(file.size());
    // [complete file-loader bytes] -> [same-size owned Wasm bytes]
    // [safe                      ] parser/initializer borrows the owned copy
    // for this complete test; no pointer survives its final module teardown.
    if(!wasm.empty()) { ::std::memcpy(wasm.data(), file.data(), wasm.size()); }
    auto features{strict::make_wasm1p1_feature_parameter()};
    auto& core{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    core.disable_gc = false;
    core.disable_tail_call = false;
    core.disable_function_references = false;
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"gc-frame-actual-wasm", {}, features)};
    require(prepared.mod != nullptr && prepared.mod->gc_store && prepared.mod->gc_store->valid(),
        "actual parser and initializer provide a valid Core 3 module/store");
    for(auto const& item : storage::wasm_module_runtime_storage)
    {
        // [actual module map] prepared_runtime keeps every native record live;
        // cohort strong pins protect all stores, including empty-type modules.
        //  ^^ retain only native record addresses, never guest-provided pointers.
        modules.push_back(::std::addressof(item.second));
        if(item.second.gc_store) { cohort.push_back(item.second.gc_store); }
    }
    require(!cohort.empty(), "complete real store cohort retained");

    compiler::compile_option options{};
    options.validator_feature_parameter = ::std::addressof(features);
    options.verify_llvm_jit_ir = true;
    options.emit_call_stack_frames = false;
    options.emit_unwind_call_stack_frames = true;
    options.native_exception_target_machine = machine.get();
    options.compilation_mode = compiler::llvm_jit_compilation_mode::full;
    options.emit_debug_safe_points = true;
    options.debug_safe_point_granularity = compiler::llvm_jit_debug_safe_point_granularity::instruction;
    for(bool enabled : {false, true})
    {
        options.emit_precise_gc_root_frames = enabled;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
        require(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok &&
            compiled.llvm_jit_module.emitted && compiled.llvm_jit_module.llvm_module != nullptr,
            "real product validator/emitter accepts the actual Core 3 Wasm");
        auto const ir{compiled.llvm_jit_module.llvm_module.get()};
        require(!::llvm::verifyModule(*ir, ::std::addressof(::llvm::errs())), "actual unoptimized Wasm IR verifies");
        ::std::size_t enters{}, leaves{}, musttails{};
        ::llvm::Function* callback{};
        auto const numeric_name{details::get_llvm_wasm_function_name(*prepared.mod, 8uz)};
        auto const numeric{ir->getFunction(name(numeric_name))};
        require(numeric != nullptr && !numeric->empty(), "actual pure-numeric Wasm typed function exists");
        for(auto& function : *ir)
        {
            if(bridge<::uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge>(function))
            {
                // [live LLVM module-owned declaration] engine retains its owner.
                //  ^^ callback is borrowed only through final native execution.
                require(callback == nullptr, "exactly one real debug bridge declaration");
                callback = ::std::addressof(function);
            }
            for(auto& block : function) for(auto& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                if(auto const target_{call->getCalledFunction()}; target_ != nullptr)
                {
                    if(bridge<frames::uwvm_gc_root_frame_enter_checked_abi>(*target_))
                    {
                        ++enters;
                        require(::std::addressof(function) != numeric, "pure numeric entry owns no empty root ABI call");
                        require(::std::addressof(block) == ::std::addressof(function.getEntryBlock()),
                            "record placement construction is in true native entry only");
                        require(call->doesNotThrow(), "root entry cannot throw through an unconstructed frame");
                    }
                    if(bridge<frames::uwvm_gc_root_frame_leave_checked_abi>(*target_))
                    {
                        ++leaves;
                        require(::std::addressof(function) != numeric, "pure numeric return owns no empty root ABI call");
                    }
                }
                if(auto const direct{::llvm::dyn_cast<::llvm::CallInst>(call)};
                    direct != nullptr && direct->isMustTailCall())
                {
                    ++musttails;
                    require(::llvm::isa<::llvm::ReturnInst>(direct->getNextNode()), "actual tail calls remain musttail plus ret");
                }
            }
        }
        require(callback != nullptr && musttails >= 2uz, "actual instruction points and cross-function musttail exist");
        require(enabled ? (enters != 0uz && leaves >= enters) : (enters == 0uz && leaves == 0uz),
            "default lowering adds no root calls; enabled lowering owns and retires every record");
        verify_precise_root_publication(*ir, numeric, enabled, false);
        verify_fixed_gc_bridge_identity(*ir, enabled, false);
        write_ir(*ir, argv[2], enabled ? "roots.ll" : "no-roots.ll");
        optimize(*ir, machine.get());
        verify_fixed_gc_bridge_identity(*ir, enabled, true);
        write_ir(*ir, argv[2], enabled ? "roots.o3.ll" : "no-roots.o3.ll");
        // O3 may discard unused ABI declarations. Obtain genuine current
        // module handles after optimization; never retain a removed GV pointer.
        auto const imports{::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(
            *ir, *machine)};
        auto const catches{::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(*ir)};
        ::std::string engine_error{};
        // [exclusive actual generated LLVM module] -> [one native engine]
        // [safe                                  ] context holder remains alive
        // outside engine destruction; no instruction/module alias is retired.
        ::std::unique_ptr<::llvm::Module> execution{compiled.llvm_jit_module.llvm_module.release()};
        ::llvm::EngineBuilder builder{::std::move(execution)};
        builder.setErrorStr(::std::addressof(engine_error)).setEngineKind(::llvm::EngineKind::JIT)
            .setOptLevel(::llvm::CodeGenOptLevel::Aggressive).setCodeModel(::llvm::CodeModel::Large);
        native_object_witness object_witness{argv[2], enabled};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{builder.create()};
        require(engine != nullptr, "real same-SDK MCJIT engine created");
        require(::uwvm2::runtime::lib::details::native_exception_host::bind<
            ::uwvm2::runtime::exception::guest_exception>(*engine, imports, catches, make_guest_probe),
            "actual native exception ABI bindings match this generated module/engine");
        auto const callback_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(collect_at_debug_point))};
        engine->addGlobalMapping(callback, reinterpret_cast<void*>(callback_address));
        engine->setObjectCache(::std::addressof(object_witness));
        engine->finalizeObject();
        require(object_witness.objects == 1uz, "actual executed native machine object is retained for disassembly");
        force_collection = enabled;
        // Function indices follow the exact source fixture. References are
        // constructed in Wasm; native callers never manufacture their carriers.
        for(auto const [index, expected] : ::std::array<::std::array<unsigned, 2uz>, 6uz>{{
            {1u, 37u}, {3u, 41u}, {5u, 43u}, {6u, 4u}, {7u, 47u}, {8u, 17u}}})
        {
            expect_empty_activation = index == 8u;
            auto const symbol{details::get_llvm_wasm_raw_function_name(*prepared.mod, index)};
            auto const address{engine->getFunctionAddress(name(symbol))};
            require(address != 0u, "MCJIT publishes the actual generated raw entry");
            using raw_entry = void(*)(::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t,
                ::std::uintptr_t, ::std::uintptr_t);
            static_assert(sizeof(raw_entry) == sizeof(address));
            raw_entry entry{};
            // [one real native entry address] -> [same-size function pointer]
            // [safe                        ] copy its representation, never
            // dereference an untrusted or object-typed function address.
            ::std::memcpy(::std::addressof(entry), ::std::addressof(address), sizeof(entry));
            ::std::uint32_t result{};
            require(frames::current_root_frames() == nullptr, "no native root leaked before guest entry");
            entry(0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(result)), sizeof(result), 0u, 0u);
            require(result == expected, "actual Core 3 guest self-check result survives all forced collections");
            require(frames::current_root_frames() == nullptr, "normal and tail returns restore empty native TLS chain");
        }
        force_collection = false;
        expect_empty_activation = false;
    }
    require(collections > 20uz && reclaimed_total != 0uz && max_depth >= 2uz,
        "actual repeated collections include nested native frames and reclaim unreachable aggregates");
    ::fast_io::io::println("PASS actual Core 3 LLVM frames: checks=", checks,
        " collections=", collections, " reclaimed=", reclaimed_total, " max_native_depth=", max_depth,
        "; actual parser/initializer/validator/O3/MCJIT; single-thread test callbacks; autoVMGC=false");
}
