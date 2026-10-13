// Included in compile_all_from_uwvm only under the exact-1 observer gate.
// Cold observations only: no executable permission, LLVM metadata or guest ABI.
namespace native_eh_leaf_observer
{
    namespace effect = ::uwvm2::runtime::compiler::shared::wasm_exception_private_leaf_effect;
    using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
    using tag_type = ::uwvm2::uwvm::runtime::storage::local_defined_tag_storage_t;
    enum class decline : unsigned char { none, scope, owner, quota, allocation, event, emission, incomplete };
    struct limits
    {
        ::std::size_t instructions{1'000'000uz}, calls{4'096uz}, retained_handlers{65'536uz};
    };
    struct call_witness
    {
        ::std::size_t expression_offset{}, event_ordinal{}, observation_callsite_index{}, actual_public_target_index{};
        bool fragment_ordinary_call_observed{};
    };
    class module_attempt final
    {
        source_type::owner source_{};
        module_type const* module_{};
        effect::observation_domain::owner domain_{};
        ::std::array<tag_type const*, 64uz> records_{};
        ::std::array<decltype(tag_type{}.exception_identity), 64uz> instance_owners_{};
        ::std::array<effect::linked_tag_identity, 64uz> tags_{};
        ::std::size_t tag_count_{}, function_count_{};
        limits limits_{};
        module_attempt(source_type::owner source, module_type const& module,
            effect::observation_domain::owner domain, limits budget) noexcept
            : source_{::std::move(source)}, module_{::std::addressof(module)}, domain_{::std::move(domain)},
              function_count_{module.local_defined_function_vec_storage.size()}, limits_{budget} {}
        [[nodiscard]] static bool claim(::std::atomic_size_t& used, ::std::size_t count, ::std::size_t limit) noexcept
        {
            auto prior{used.load(::std::memory_order_relaxed)};
            for(;;)
            {
                if(prior > limit || count > limit - prior) { return false; }
                if(used.compare_exchange_weak(prior, prior + count, ::std::memory_order_relaxed)) { return true; }
            }
        }
    public:
        using owner = ::std::shared_ptr<module_attempt>;
        ::std::atomic_size_t instructions{}, calls{}, retained_handlers{}, declined_functions{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        bool stage_private_leaf{}; // Request only, immutable once workers start.
        ::std::atomic_bool private_call_metadata_failed{};
#endif
        [[nodiscard]] static owner create(source_type::owner const& source, module_type const& module,
            ::std::size_t module_id, bool scope, limits budget) noexcept
        {
            // [real typed source owner][its actual initialized registry member]
            // [safe] Ownership/control block and actual module identity precede
            // every borrow. No full-validation epoch is read or fabricated.
            if(!scope || !source_type::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
               source->initialized_main_module() != ::std::addressof(module) ||
               source->bound_initialized_main_module_id() != module_id ||
               module.local_defined_function_vec_storage.empty() || module.local_defined_function_vec_storage.size() > 256uz ||
               module.local_defined_tag_vec_storage.size() > 64uz || !module.imported_function_vec_storage.empty() ||
               !module.imported_tag_vec_storage.empty() || !module.imported_memory_vec_storage.empty() ||
               !module.imported_table_vec_storage.empty() || !module.imported_global_vec_storage.empty() ||
               !module.local_defined_memory_vec_storage.empty() || !module.local_defined_table_vec_storage.empty() ||
               !module.local_defined_global_vec_storage.empty()) { return {}; }
            budget.instructions = budget.instructions < 1'000'000uz ? budget.instructions : 1'000'000uz;
            budget.calls = budget.calls < 4'096uz ? budget.calls : 4'096uz;
            budget.retained_handlers = budget.retained_handlers < 65'536uz ? budget.retained_handlers : 65'536uz;
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
                auto domain{effect::observation_domain::create(module_id, module.local_defined_function_vec_storage.size())};
                if(!domain) { return {}; }
                owner result{new module_attempt{source, module, ::std::move(domain), budget}};
                ::std::size_t expression_bytes{};
                for(auto const& function: module.local_defined_function_vec_storage)
                {
                    if(function.wasm_code_ptr == nullptr) { return {}; }
                    // [actual retained parser body metadata] checked expression extent.
                    // [safe] Integer lengths only for quota; no new pointer is
                    // advanced, dereferenced or used to authorize an alien span.
                    auto const begin{reinterpret_cast<::std::uintptr_t>(function.wasm_code_ptr->body.expr_begin)};
                    auto const end{reinterpret_cast<::std::uintptr_t>(function.wasm_code_ptr->body.code_end)};
                    constexpr auto cap{8uz * 1'024uz * 1'024uz};
                    if(begin == 0u || end <= begin || end - begin > cap - expression_bytes) { return {}; }
                    expression_bytes += end - begin;
                }
                for(::std::size_t index{}; index != module.local_defined_tag_vec_storage.size(); ++index)
                {
                    // [actual local tag records][index < size <= 64] end
                    // [safe] Copy the initialized instance's exact owner and
                    // record address. No unproved candidate tag is dereferenced.
                    auto const& record{module.local_defined_tag_vec_storage.index_unchecked(index)};
                    if(!record.exception_identity) { return {}; }
                    auto const identity{effect::linked_tag_identity::observe_actual_linked_instance(record.exception_identity)};
                    if(!identity) { return {}; }
                    result->records_[index] = ::std::addressof(record);
                    result->instance_owners_[index] = record.exception_identity;
                    result->tags_[index] = *identity;
                }
                result->tag_count_ = module.local_defined_tag_vec_storage.size();
                return result;
            }
            catch(...) { return {}; } // Only these observation allocations are inside this boundary.
#else
            return {}; // No recoverable observer allocation contract on this configuration.
#endif
        }
        [[nodiscard]] module_type const* module() const noexcept { return module_; }
        [[nodiscard]] source_type::owner const& source() const noexcept { return source_; }
        [[nodiscard]] effect::observation_domain::owner const& domain() const noexcept { return domain_; }
        [[nodiscard]] ::std::size_t function_count() const noexcept { return function_count_; }
        [[nodiscard]] bool claim_instruction() noexcept { return claim(instructions, 1uz, limits_.instructions); }
        [[nodiscard]] bool claim_call(::std::size_t handlers) noexcept
        { return claim(calls, 1uz, limits_.calls) && claim(retained_handlers, handlers, limits_.retained_handlers); }
        [[nodiscard]] ::std::optional<effect::linked_tag_identity> tag(tag_type const* candidate) const noexcept
        {
            for(::std::size_t index{}; index != tag_count_; ++index)
            {
                // [retained exact local records][index < tag_count <= 64] end
                // [safe] Address equality proves genuine element membership.
                // Only after equality is the current real instance owner read.
                if(records_[index] == candidate)
                {
                    auto const& current{candidate->exception_identity};
                    auto const& actual{instance_owners_[index]};
                    if(!current || current.get() != actual.get() || current.owner_before(actual) || actual.owner_before(current)) { return {}; }
                    return tags_[index];
                }
            }
            return {};
        }
    };
    struct function_result
    {
        module_attempt::owner attempt{};
        ::std::optional<effect::function_observation> observation{};
        ::std::vector<call_witness> witnesses{};
        ::std::size_t instruction_count{};
        decline reason{decline::incomplete};
    };
    struct module_result
    {
        module_attempt::owner attempt{};
        bool complete{}; // Event/source lifetime closure, NEVER executable permission.
        ::std::size_t completed_functions{}, consumed_effect_edges{}, fragment_calls{};
        decline reason{decline::incomplete};
    };
    class function_state final
    {
        enum class event_kind : unsigned char { scalar, unsupported, handlers, throw_, call };
        module_attempt::owner attempt_{};
        ::std::size_t function_index_{}, expression_size_{}, current_begin_{}, count_{}, call_count_{};
        ::std::optional<effect::function_recorder> recorder_{};
        ::std::optional<effect::function_observation> sealed_{};
        ::std::vector<call_witness> witnesses_{};
        ::std::array<effect::handler_observation, 256uz> handlers_{};
        ::std::size_t handler_count_{}, target_{effect::unknown_function};
        effect::linked_tag_identity throw_tag_{};
        effect::payload_class payload_{effect::payload_class::unknown};
        event_kind kind_{event_kind::unsupported};
        ::llvm::CallBase* current_call_{}; // Same-operation borrow only, cleared before the next instruction.
        ::llvm::Function* current_callee_{};
        ::llvm::Function* current_caller_{};
        unsigned opcode_{};
        decline reason_{decline::none};
        function_state(module_attempt::owner attempt, ::std::size_t index, ::std::size_t bytes) noexcept
            : attempt_{::std::move(attempt)}, function_index_{index}, expression_size_{bytes} {}
        void clear_packet() noexcept
        {
            for(::std::size_t index{}; index != handler_count_; ++index) { handlers_[index] = {}; }
            handler_count_ = 0uz; throw_tag_ = {}; target_ = effect::unknown_function;
            // [LLVM fragment owns the temporary call/function objects]
            // [safe] Drop only borrowed pointers; none survives this operation.
            current_call_ = nullptr; current_callee_ = nullptr; current_caller_ = nullptr;
        }
        template<typename Handler> [[nodiscard]] bool append_handler(Handler const& handler, ::std::size_t frames) noexcept
        {
            if(handler_count_ == handlers_.size() || handler.target_frame >= frames) { stop(decline::quota); return false; }
            effect::handler_observation output{};
            if(handler.identity == nullptr)
            { output.form = handler.with_reference ? effect::catch_form::all_reference : effect::catch_form::all; }
            else
            {
                auto tag{attempt_->tag(handler.identity)};
                if(!tag) { stop(decline::owner); return false; }
                output.tag = ::std::move(*tag);
                output.form = handler.with_reference ? effect::catch_form::tagged_reference : effect::catch_form::tagged;
            }
            handlers_[handler_count_++] = ::std::move(output);
            return true;
        }
        template<typename Frames> [[nodiscard]] bool snapshot(Frames const& frames) noexcept
        {
            if(frames.size() > 128uz) { stop(decline::quota); return false; }
            for(::std::size_t remaining{frames.size()}; remaining != 0uz; --remaining)
            {
                // [live validator frames][remaining-1 < size] end
                // [safe] Borrow inner frame first; preserve each clause order.
                // Copies retain owners, never a pointer into movable vectors.
                auto const& frame{frames.index_unchecked(remaining - 1uz)};
                for(auto const& handler: frame.exception_handlers)
                { if(!append_handler(handler, remaining - 1uz)) { return false; } }
            }
            return true;
        }
        [[nodiscard]] bool ordinary_witness() const noexcept
        {
            if(current_call_ == nullptr || current_callee_ == nullptr || current_caller_ == nullptr || opcode_ != 0x10u ||
               current_call_->getParent() == nullptr || current_call_->getParent()->getParent() != current_caller_ ||
               current_caller_->getParent() == nullptr || current_callee_->getParent() != current_caller_->getParent() ||
               current_call_->getCalledFunction() != current_callee_ ||
               current_call_->getFunctionType() != current_callee_->getFunctionType() ||
               current_call_->getCallingConv() != current_callee_->getCallingConv() ||
               current_call_->arg_size() != current_callee_->arg_size()) { return false; }
            for(unsigned index{}; index != current_call_->arg_size(); ++index)
            {
                if(current_call_->getArgOperand(index)->getType() != current_call_->getFunctionType()->getParamType(index)) { return false; }
            }
            return true;
        }
    public:
        template<typename Local> [[nodiscard]] static ::std::shared_ptr<function_state> create(module_attempt::owner const& attempt,
            Local const& local) noexcept
        {
            if(!attempt || local.runtime_module_ptr != attempt->module() || local.function_index >= attempt->function_count() ||
               local.code_begin == nullptr || local.code_end == nullptr) { return {}; }
            // [actual source-owned local function][checked public/local index]
            // [safe] The admitted module has no imports. All code/type borrows
            // are compared with this real initialized element before offsets.
            auto const& actual{attempt->module()->local_defined_function_vec_storage.index_unchecked(local.function_index)};
            if(actual.function_type_ptr != local.function_type_ptr || actual.wasm_code_ptr != local.wasm_code_ptr ||
               actual.wasm_code_ptr == nullptr ||
               reinterpret_cast<::std::byte const*>(actual.wasm_code_ptr->body.expr_begin) != local.code_begin ||
               reinterpret_cast<::std::byte const*>(actual.wasm_code_ptr->body.code_end) != local.code_end) { return {}; }
            if(local.code_begin >= local.code_end) { return {}; }
            // [actual parser expression begin ... end] same retained byte span.
            // [safe] Difference only; neither endpoint is advanced/dereferenced.
            auto const size{static_cast<::std::size_t>(local.code_end - local.code_begin)};
            if(size > 8uz * 1'024uz * 1'024uz) { return {}; }
#ifdef UWVM_CPP_EXCEPTIONS
            try { return ::std::shared_ptr<function_state>{new function_state{attempt, local.function_index, size}}; }
            catch(...) { return {}; }
#else
            return {};
#endif
        }
        void stop(decline reason) noexcept
        {
            if(reason_ == decline::none) { ++attempt_->declined_functions; reason_ = reason; }
            recorder_.reset(); sealed_.reset(); witnesses_.clear(); clear_packet();
        }
        [[nodiscard]] bool active() const noexcept { return reason_ == decline::none && recorder_.has_value(); }
        void begin(bool numeric, bool actual_native) noexcept
        {
            if(!actual_native) { stop(decline::scope); return; }
            recorder_ = effect::function_recorder::begin(attempt_->domain(), function_index_, expression_size_,
                numeric ? effect::signature_class::numeric : effect::signature_class::reference_or_vector);
            if(!recorder_) { stop(decline::event); }
        }
        void start(unsigned opcode, ::std::size_t begin, ::std::size_t depth) noexcept
        {
            if(!active()) { return; }
            clear_packet(); current_begin_ = begin; opcode_ = opcode;
            if(depth > 128uz) { stop(decline::quota); return; }
            // Deliberately narrow SSA/control whitelist. Division/conversion
            // traps, math/bit-count intrinsics and all extended/helper opcodes
            // decline the leaf until their precise native effect is reviewed.
            auto const scalar{(opcode >= 0x01u && opcode <= 0x05u) || (opcode >= 0x0bu && opcode <= 0x0fu) ||
                opcode == 0x1au || opcode == 0x1bu || (opcode >= 0x20u && opcode <= 0x22u) ||
                (opcode >= 0x41u && opcode <= 0x66u) || (opcode >= 0x6au && opcode <= 0x6cu) ||
                (opcode >= 0x71u && opcode <= 0x78u) || (opcode >= 0x7cu && opcode <= 0x7eu) ||
                (opcode >= 0x83u && opcode <= 0x8au) || opcode == 0xa7u || opcode == 0xacu || opcode == 0xadu ||
                (opcode >= 0xbcu && opcode <= 0xc4u)};
            kind_ = scalar ? event_kind::scalar : event_kind::unsupported;
        }
        void non_numeric_control(bool numeric) noexcept { if(active() && !numeric) { kind_ = event_kind::unsupported; } }
        template<typename Handlers> void prepare_handlers(Handlers const& handlers, ::std::size_t outer_frames, bool numeric) noexcept
        {
            if(!active()) { return; }
            kind_ = numeric ? event_kind::handlers : event_kind::unsupported;
            for(auto const& handler: handlers) { if(!append_handler(handler, outer_frames)) { return; } }
        }
        template<typename Frames> void prepare_throw(tag_type const* tag, bool numeric, Frames const& frames) noexcept
        {
            if(!active()) { return; }
            auto actual{attempt_->tag(tag)};
            if(!actual) { stop(decline::owner); return; }
            throw_tag_ = ::std::move(*actual); payload_ = numeric ? effect::payload_class::numeric : effect::payload_class::reference_or_vector;
            kind_ = event_kind::throw_; static_cast<void>(snapshot(frames));
        }
        template<typename Frames> void prepare_call(::std::size_t target, Frames const& frames) noexcept
        {
            if(!active()) { return; }
            if(opcode_ != 0x10u || target >= attempt_->function_count()) { stop(decline::event); return; }
            kind_ = event_kind::call; target_ = target; static_cast<void>(snapshot(frames));
        }
        void fragment_call(::llvm::CallBase* call, ::llvm::Function* callee, ::llvm::Function* caller,
            module_type const& actual_module, ::std::size_t target) noexcept
        {
            if(!active() || kind_ != event_kind::call || opcode_ != 0x10u || target_ != target ||
               attempt_->module() != ::std::addressof(actual_module)) { return; }
            if(current_call_ != nullptr) { stop(decline::event); return; }
            // [current actual emitter-owned call/declaration/caller] live fragment.
            // [safe] A temporary same-operation borrow; no allocation/callback or
            // unlock here. Post-success commit checks the completed calling ABI.
            current_call_ = call; current_callee_ = callee; current_caller_ = caller;
        }
        void commit(::std::size_t end, bool outer_end = false) noexcept
        {
            if(!active()) { return; }
            if(end > expression_size_ || current_begin_ >= end || !attempt_->claim_instruction()) { stop(decline::quota); return; }
            effect::validated_byte_range const range{current_begin_, end};
            auto const handlers{::std::span<effect::handler_observation const>{handlers_.data(), handler_count_}};
#ifdef UWVM_CPP_EXCEPTIONS
            try // Only observer record/copy allocations, never decoder/LLVM work.
            {
                bool success{};
                if(outer_end) { success = opcode_ == 0x0bu && recorder_->observe_outer_function_end(range); }
                else switch(kind_)
                {
                    case event_kind::scalar: success = recorder_->observe_scalar_or_control(range); break;
                    case event_kind::unsupported: success = recorder_->observe_disqualifying(range, effect::disqualifying_effect::unknown_opcode); break;
                    case event_kind::handlers: success = recorder_->observe_handlers(range, handlers); break;
                    case event_kind::throw_: success = recorder_->observe_throw(range, throw_tag_, payload_, handlers); break;
                    case event_kind::call:
                    {
                        auto const real{ordinary_witness()};
                        if(real && !attempt_->claim_call(handler_count_)) { stop(decline::quota); return; }
                        success = recorder_->observe_call(range,
                            real ? effect::call_route::ordinary_local_direct : effect::call_route::unknown,
                            real ? target_ : effect::unknown_function, handlers);
                        if(success && real)
                        {
                            witnesses_.push_back({current_begin_, count_, call_count_, target_, true});
                            ++call_count_;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
                            if(attempt_->stage_private_leaf && !native_eh_private_leaf::attach_actual_call_metadata(*current_call_,
                                {function_index_, current_begin_, count_, call_count_ - 1uz, target_}))
                            { attempt_->private_call_metadata_failed.store(true, ::std::memory_order_relaxed); }
#endif
                        }
                        break;
                    }
                }
                if(!success) { stop(decline::event); return; }
                ++count_;
                clear_packet();
                if(outer_end)
                {
                    sealed_ = recorder_->seal_observation(); recorder_.reset();
                    if(!sealed_) { stop(decline::incomplete); }
                }
            }
            catch(...) { stop(decline::allocation); }
#endif
        }
        [[nodiscard]] ::std::shared_ptr<function_result const> finish() noexcept
        {
            if(!sealed_) { stop(decline::incomplete); }
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
                return ::std::make_shared<function_result>(function_result{
                    attempt_, ::std::move(sealed_), ::std::move(witnesses_), count_, reason_});
            }
            catch(...) { stop(decline::allocation); return {}; }
#else
            return {};
#endif
        }
    };
    template<typename Range> [[nodiscard]] bool numeric_range(Range const& range) noexcept
    {
        // [validated signature begin ... end] native type allocation.
        // [safe] value = begin borrows its first element (or one-past when empty).
        // Each ++value below stays in the same span and may become end only.
        for(auto value{range.begin}; value != range.end; ++value)
        {
            // [already validated signature range][value < end] end
            // [safe] One existing native type entry; advance to at most end.
            auto const code{static_cast<unsigned>(*value)};
            if(code != 0x7fu && code != 0x7eu && code != 0x7du && code != 0x7cu) { return false; }
        }
        return true;
    }
    template<typename Core> [[nodiscard]] bool numeric_core(Core const& value) noexcept
    {
        namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
        return value.kind == type::value_kind::i32 || value.kind == type::value_kind::i64 ||
            value.kind == type::value_kind::f32 || value.kind == type::value_kind::f64;
    }
    template<typename Signature, typename Types> [[nodiscard]] bool numeric_block(Signature const& signature, Types const& types) noexcept
    {
        if(!numeric_range(signature.params) || !numeric_range(signature.results) ||
           (signature.has_singleton_result_core_type && !numeric_core(signature.singleton_result_core_type))) { return false; }
        if(types.owned_signatures.size() == types.types.size() && signature.type_index < types.owned_signatures.size())
        {
            auto const& rich{types.owned_signatures.index_unchecked(signature.type_index)};
            for(auto const& value: rich.parameters) { if(!numeric_core(value)) { return false; } }
            for(auto const& value: rich.results) { if(!numeric_core(value)) { return false; } }
        }
        return true;
    }
    struct restore_attempt final
    {
        module_attempt::owner& slot;
        module_attempt::owner previous;
        explicit restore_attempt(module_attempt::owner& input) noexcept : slot{input}, previous{::std::move(input)} {}
        ~restore_attempt()
        {
            // [joined compiler tasks][restored original option lifetime edge]
            // [safe] No worker can retain a borrow into this option anymore.
            slot = ::std::move(previous);
        }
        restore_attempt(restore_attempt const&) = delete;
    };
    template<typename Storage> void close_module(Storage& storage, module_attempt::owner const& attempt) noexcept
    {
        storage.native_eh_leaf_observations = {};
        auto& result{storage.native_eh_leaf_observations};
        result.attempt = attempt;
        if(!attempt) { result.reason = decline::scope; return; }
        if(!storage.llvm_jit_module.emitted || !storage.llvm_jit_module.llvm_module ||
           storage.local_funcs.size() != attempt->function_count() ||
           !source_type::has_canonical_owner(attempt->source()) ||
           attempt->source()->initialized_main_module() != attempt->module() ||
           attempt->source()->bound_initialized_main_module_id() != attempt->domain()->module_index())
        { result.reason = decline::owner; return; }
        for(::std::size_t index{}; index != storage.local_funcs.size(); ++index)
        {
            // [complete output slots][index < actual local count] end
            // [safe] All workers have joined; no slot/vector can move here.
            auto const& local{storage.local_funcs.index_unchecked(index)};
            if(local.runtime_module_ptr != attempt->module() || local.function_index != index || !local.native_eh_leaf_observation)
            { return; }
            auto const& record{*local.native_eh_leaf_observation};
            if(record.reason != decline::none || record.attempt.get() != attempt.get() ||
               record.attempt.owner_before(attempt) || attempt.owner_before(record.attempt) || !record.observation ||
               !record.observation->event_stream_complete() || record.observation->function_index() != index ||
               record.witnesses.size() != record.observation->direct_callsite_count()) { return; }
            ++result.completed_functions;
        }
        for(auto const& local: storage.local_funcs)
        {
            auto const& caller{*local.native_eh_leaf_observation};
            for(auto const& witness: caller.witnesses)
            {
                if(!witness.fragment_ordinary_call_observed || witness.actual_public_target_index >= storage.local_funcs.size() ||
                   witness.observation_callsite_index >= caller.observation->direct_callsite_count()) { return; }
                auto const& callee{*storage.local_funcs.index_unchecked(witness.actual_public_target_index).native_eh_leaf_observation};
                ++result.fragment_calls;
                if(effect::observe_consumed_direct_call(*caller.observation, witness.observation_callsite_index,
                    *callee.observation) == effect::selection::observed_consumed) { ++result.consumed_effect_edges; }
            }
        }
        result.reason = decline::none;
        result.complete = true; // Observation closure only. No IR redirect/publication.
    }
}
