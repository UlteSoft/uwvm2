// PRIVATE actual validated-stream graph finalization. This is cold compiler
// work; ordinary memory and numeric guest instructions gain no runtime guard.
// Included inside details after single_func_emit.h, never by production code.
[[nodiscard]] inline bool pending_numeric_metadata_integer(::llvm::MDNode const* node,
    unsigned operand, ::std::uint_least64_t& value) noexcept
{
    if(node == nullptr || operand >= node->getNumOperands()) { return false; }
    auto const metadata{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(node->getOperand(operand).get())};
    auto const constant{metadata == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::ConstantInt>(metadata->getValue())};
    if(constant == nullptr || constant->getBitWidth() > 64u) { return false; }
    value = constant->getZExtValue();
    return true;
}

[[nodiscard]] inline bool finalize_pending_numeric_effects(::llvm::Module& module,
    ::uwvm2::runtime::exception::pending_experiment::numeric_entry::compile_plan const& plan,
    bool fold_proven_empty_calls) noexcept
{
    namespace analysis = ::uwvm2::runtime::compiler::shared::wasm_exception_effect;
    if(!plan.shape_valid_for_compilation() || !plan.module->imported_function_vec_storage.empty()) { return false; }
    auto const count{plan.function_count};
    constexpr auto bound{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
    if(count == 0uz || count > bound / sizeof(::llvm::Function*) ||
       count > bound / sizeof(analysis::function)) { return false; }
    ::std::vector<::llvm::Function*> bodies(count);
    ::std::vector<analysis::function> functions(count);
    ::std::vector<analysis::call> calls{};
    struct presence_site { ::llvm::LoadInst* instruction; ::std::size_t target; };
    ::std::vector<presence_site> presence{};

    // All function indices are assigned by actual validated emission, not
    // parsed from strings/custom sections or reconstructed from a byte scan.
    for(auto& function: module)
    {
        auto const identity{function.getMetadata("uwvm2.pending.core.index")};
        if(identity == nullptr) { continue; }
        ::std::uint_least64_t index{};
        if(identity->getNumOperands() != 1u || !pending_numeric_metadata_integer(identity, 0u, index) ||
           index >= count || function.empty() || bodies[static_cast<::std::size_t>(index)] != nullptr)
        { return false; }
        // [count owned graph slots][index proved in range]
        // [safe                                           ] module ownership
        // keeps this native IR function live until graph/folding completes.
        bodies[static_cast<::std::size_t>(index)] = ::std::addressof(function);
        // Guest escape and C++ unwind are INDEPENDENT effects. Keep native
        // unwind conservatively set, even for numeric leaves. This pass never
        // adds LLVM nounwind, removes an invoke, or bypasses foreign cleanup.
        auto local{analysis::effect::native_unwind};
        if(auto const guest{function.getMetadata("uwvm2.pending.local.guest")}; guest != nullptr)
        {
            ::std::uint_least64_t escaping{};
            if(guest->getNumOperands() != 1u || !pending_numeric_metadata_integer(guest, 0u, escaping) || escaping != 1u)
            { return false; }
            local = analysis::effect::both;
        }
        functions[static_cast<::std::size_t>(index)].local = local;
    }
    // A declaration/omitted body/import is NEVER inferred pure. The private
    // numeric island requires the complete same-generation module; an open
    // graph is refused before any check or metadata is changed.
    for(auto const body: bodies) { if(body == nullptr) { return false; } }

    for(::std::size_t index{}; index != count; ++index)
    {
        auto const body{bodies[index]};
        if(body->arg_empty() || !body->getArg(0u)->getType()->isIntegerTy(sizeof(::std::uintptr_t) * CHAR_BIT))
        { return false; }
        for(auto& block: *body)
        {
            for(auto& instruction: block)
            {
                if(auto const check{instruction.getMetadata("uwvm2.pending.presence.target")}; check != nullptr)
                {
                    ::std::uint_least64_t called{};
                    auto const load{::llvm::dyn_cast<::llvm::LoadInst>(::std::addressof(instruction))};
                    auto const pointer{load == nullptr ? nullptr :
                        ::llvm::dyn_cast<::llvm::IntToPtrInst>(load->getPointerOperand())};
                    // Do not accept r1 checked leaf calls, arbitrary memory,
                    // forged metadata, atomic/volatile reads or another hidden
                    // context. R2's actual owned phase is a standard-layout
                    // native WORD at offset zero in this same sealed core.
                    if(check->getNumOperands() != 1u || !pending_numeric_metadata_integer(check, 0u, called) ||
                       called >= count || load == nullptr || pointer == nullptr ||
                       pointer->getOperand(0u) != body->getArg(0u) ||
                       !load->getType()->isIntegerTy(sizeof(::std::uintptr_t) * CHAR_BIT) ||
                       load->isAtomic() || load->isVolatile() ||
                       load->getAlign() != ::llvm::Align{alignof(::std::uintptr_t)} ||
                       pointer->getParent() != ::std::addressof(block) || pointer->getNextNode() != load ||
                       presence.size() == bound / sizeof(presence_site)) { return false; }
                    // COLD anchoring proof: target metadata alone is not a
                    // call-effect witness. This pointer/load pair must be the
                    // immediate normal continuation of the actual same-header
                    // core call the validated emitter just recorded. A metadata
                    // substitution to a NoGuestEscape callee, an intervening
                    // instruction/call or foreign hidden argument fails closed.
                    ::llvm::CallBase const* origin{};
                    if(auto const previous{pointer->getPrevNode()}; previous != nullptr)
                    {
                        auto const direct{::llvm::dyn_cast<::llvm::CallInst>(previous)};
                        if(direct == nullptr || direct->isMustTailCall()) { return false; }
                        origin = direct;
                    }
                    else
                    {
                        auto const predecessor{block.getSinglePredecessor()};
                        // [same compiler-owned CFG predecessor][its instruction list]
                        // [safe ] prove nonempty and a terminator BEFORE borrowing
                        // back(); LLVM 23 getTerminator() asserts on an open block.
                        auto const invoke{predecessor == nullptr || predecessor->empty() || !predecessor->back().isTerminator() ? nullptr :
                            ::llvm::dyn_cast<::llvm::InvokeInst>(::std::addressof(predecessor->back()))};
                        if(invoke == nullptr || invoke->getNormalDest() != ::std::addressof(block)) { return false; }
                        origin = invoke;
                    }
                    auto const target{origin->getCalledFunction()};
                    auto const identity{target == nullptr ? nullptr : target->getMetadata("uwvm2.pending.core.index")};
                    auto const edge{origin->getMetadata("uwvm2.pending.core.call")};
                    ::std::uint_least64_t declared{}, recorded{}, escaping{};
                    if(origin->getFunction() != body || target != bodies[static_cast<::std::size_t>(called)] ||
                       identity == nullptr || identity->getNumOperands() != 1u || edge == nullptr || edge->getNumOperands() != 2u ||
                       !pending_numeric_metadata_integer(identity, 0u, declared) ||
                       !pending_numeric_metadata_integer(edge, 0u, recorded) ||
                       !pending_numeric_metadata_integer(edge, 1u, escaping) ||
                       declared != called || recorded != called || escaping > 1u ||
                       origin->arg_empty() || origin->getArgOperand(0u) != body->getArg(0u)) { return false; }
                    presence.push_back({load, static_cast<::std::size_t>(called)});
                }
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                auto const target{call->getCalledFunction()};
                auto const target_identity{target == nullptr ? nullptr : target->getMetadata("uwvm2.pending.core.index")};
                auto const edge{call->getMetadata("uwvm2.pending.core.call")};
                if(target_identity != nullptr || edge != nullptr)
                {
                    ::std::uint_least64_t declared{}, called{}, escaping{};
                    if(edge == nullptr || target_identity == nullptr || edge->getNumOperands() != 2u ||
                       !pending_numeric_metadata_integer(target_identity, 0u, declared) ||
                       !pending_numeric_metadata_integer(edge, 0u, called) ||
                       !pending_numeric_metadata_integer(edge, 1u, escaping) ||
                       called >= count || declared != called || escaping > 1u ||
                       bodies[static_cast<::std::size_t>(called)] != target ||
                       call->arg_empty() || call->getArgOperand(0u) != body->getArg(0u) ||
                       calls.size() == bound / sizeof(analysis::call)) { return false; }
                    if(auto const transfer{::llvm::dyn_cast<::llvm::CallInst>(call)};
                       transfer != nullptr && transfer->isMustTailCall() && escaping != 1u) { return false; }
                    calls.push_back({index, static_cast<::std::size_t>(called),
                        escaping != 0u ? analysis::effect::both : analysis::effect::native_unwind});
                }

            }
        }
    }
    auto const effects{analysis::analyze(functions, calls)};
    if(effects.state != analysis::status::ok || effects.functions.size() != count) { return false; }
    auto& context{module.getContext()};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const result{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
            ::llvm::Type::getInt8Ty(context), analysis::bits(effects.functions[index])))};
        bodies[index]->setMetadata("uwvm2.pending.effects", ::llvm::MDNode::get(context, {result}));
    }
    ::std::size_t folded{};
    if(fold_proven_empty_calls)
    {
        for(auto const& site: presence)
        {
            if((analysis::bits(effects.functions[site.target]) & analysis::bits(analysis::effect::guest_escape)) != 0u)
            { continue; }
            // [actual same-generation numeric core][validated complete graph]
            // [safe                                                       ]
            // Entry owner constructs empty pending state; every core forwards
            // that same owned header FIRST. Normal Wasm calls are reached only with empty
            // pending, catches clear before continuing, and exceptional returns
            // never consume results. This callee cannot create escaping guest
            // state. Fold ONLY this proved standard-layout phase LOAD; retain
            // every native invoke/unwind edge and all scalar/multi-result
            // ownership repair. No native/C++ function gains nounwind.
            site.instruction->replaceAllUsesWith(::llvm::ConstantInt::get(site.instruction->getType(), 0u));
            // [module-owned instruction][no remaining uses]
            // [safe                                    ] end its native IR
            // lifetime only after replacement; no later site names it again.
            site.instruction->eraseFromParent();
            ++folded;
        }
    }
    auto const folded_count{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
        ::llvm::Type::getInt64Ty(context), folded))};
    module.getOrInsertNamedMetadata("uwvm2.pending.folded_presence")->addOperand(
        ::llvm::MDNode::get(context, {folded_count}));
    return true;
}
