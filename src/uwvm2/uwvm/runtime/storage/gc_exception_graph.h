// Included in gc_object_store only with EXCEPTION_GC_GRAPH == 1.
// This cold graph implementation does not discover native roots or stop VM
// readers. Its caller must own a complete paused native-owner census, precise
// carriers, canonical cohort pins and the genuine exclusive entry admission.
// The existing aggregate API and production tag/native-escape gates stay closed
// to exceptions until their complete owner-registration integration is proven.

        class exception_graph_state;
public:
        class retired_exception_batch
        {
            friend class gc_object_store;
            friend class exception_graph_state;
            exn_token_entry* head_{};
            ::std::size_t count_{};
        public:
            retired_exception_batch() noexcept = default;
            retired_exception_batch(retired_exception_batch const&) = delete;
            retired_exception_batch& operator=(retired_exception_batch const&) = delete;
            retired_exception_batch(retired_exception_batch&&) = delete;
            retired_exception_batch& operator=(retired_exception_batch&&) = delete;
            ~retired_exception_batch() noexcept { reset_after_native_resume(); }

            [[nodiscard]] inline bool empty() const noexcept { return head_ == nullptr; }
            [[nodiscard]] inline ::std::size_t size() const noexcept { return count_; }

#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            [[nodiscard]] bool matches_source_native_leaf(
                ::std::shared_ptr<::uwvm2::runtime::exception::external_exception_lifetime const> const& source,
                void const* certificate) const noexcept
            {
                if(!source || certificate == nullptr || head_ == nullptr || count_ == 0uz) { return false; }
                ::std::size_t visited{};
                // [private detached node chain] no other mutator owns these nodes.
                auto const* current{head_};
                while(current != nullptr)
                {
                    if(visited == count_ || current->root_count != 0uz ||
                       current->source_native_leaf_certificate != certificate ||
                       current->source_origin.owner_before(source) || source.owner_before(current->source_origin))
                    { return false; }
                    ++visited;
                    // [proved detached node] successor remains owned by batch;
                    // bounded count prevents accepting malformed/unbounded links.
                    current = current->next;
                }
                // Exact same source is strongly held by the actual producer and
                // owner pins; weak-origin comparison never retains foreign owners
                // or invokes a destructor inside classification.
                return visited == count_;
            }
#endif
            // Only after the pause ticket, stopped callback, cohort, root-domain
            // and registry locks have retired. Enclosing canonical generation
            // pins outlive this release. Tag/payload owners may run native
            // destructors; never invoke them during marking or under a VM lock.
            inline void reset_after_native_resume() noexcept
            {
                // [private detached registry nodes] or null
                // ^^ transfer only this batch's native chain to local ownership.
                auto* current{head_};
                head_ = nullptr;
                auto const expected{count_};
                count_ = 0uz;
                ::std::size_t removed{};
                while(current != nullptr)
                {
                    if(removed == expected || current->root_count != 0uz) { ::std::terminate(); }
                    // [detached node][initialized private successor/null]
                    // ^^ save the successor before releasing the immutable value.
                    auto* next{current->next};
                    delete current;
                    ++removed;
                    // [remaining batch] next was read before ending node lifetime.
                    // ^^ advance only to another detached native node or null.
                    current = next;
                }
                if(removed != expected) { ::std::terminate(); }
            }
        };

private:
        class exception_graph_state
        {
            friend class gc_object_store;
            retired_exception_batch* retired_{};
            ::std::span<::uwvm2::runtime::exception::value_ref const> native_{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
            // This noncopyable view is minted only inside the actual owning
            // domain's locked census callback. The root lock and native owner
            // lifetime remain held through this entire graph pass. It is not
            // itself proof of a complete VM pause or canonical store admission.
            ::uwvm2::runtime::exception::native_exception_root_domain::registered_view const* native_view_{};
#endif
            ::std::shared_ptr<gc_object_store> const* stores_{};
            ::std::size_t store_count_{};
            struct recipient_record
            {
                exn_root_node* root{};
                bool seen{};
            };
            ::std::unique_ptr<recipient_record[]> recipients_{};
            ::std::size_t recipient_capacity_{};
            ::std::size_t recipient_size_{};
            ::std::unique_ptr<exn_token_entry*[]> work_{};
            ::std::size_t node_count_{};
            ::std::size_t work_count_{};
            bool locked_{};

            [[nodiscard]] inline gc_object_store const* canonical_issuer(exn_token_entry const& entry) const noexcept
            {
                // The registry records weak_from_this(), never an arbitrary
                // alias. Compare control blocks against already authenticated
                // strong cohort pins BEFORE any weak promotion or owner access.
                // Promoting an outside owner here could destroy it while the
                // cohort lock is held and deadlock its unregister operation.
                for(::std::size_t index{}; index != store_count_; ++index)
                {
                    if(!entry.owner.owner_before(stores_[index]) &&
                       !stores_[index].owner_before(entry.owner)) { return stores_[index].get(); }
                }
                return nullptr;
            }
            [[nodiscard]] inline bool registered_entry(exn_token_entry const* candidate) const noexcept
            {
                // Before marking, work_ is a complete sorted native-entry
                // index. A recipient candidate is compared by pointer identity
                // without dereferencing it. std::less supplies the portable
                // total pointer order; unrelated allocation addresses are never
                // compared with built-in relational operators.
                if(candidate == nullptr || node_count_ == 0uz) { return false; }
                // [0,node_count_) contains every protected registry entry once.
                // ^^ the checked array has already been completely initialized.
                auto const* first{work_.get()};
                // [first,first+node_count_) [one-past]
                // ^^ node_count_ * pointer width was bounded by PTRDIFF_MAX.
                auto const* last{first + node_count_};
                auto const* match{::std::lower_bound(first, last, candidate,
                                                   ::std::less<exn_token_entry const*>{})};
                // lower_bound may return one-past; test it before reading a slot.
                return match != last && *match == candidate;
            }

        public:
            exception_graph_state(retired_exception_batch* retired,
                ::std::span<::uwvm2::runtime::exception::value_ref const> native,
                ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t count
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                , ::uwvm2::runtime::exception::native_exception_root_domain::registered_view const* native_view = nullptr
#endif
                ) noexcept
                : retired_{retired}, native_{native},
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                  native_view_{native_view},
#endif
                  stores_{stores}, store_count_{count} {}
            exception_graph_state(exception_graph_state const&) = delete;
            exception_graph_state& operator=(exception_graph_state const&) = delete;
            ~exception_graph_state() noexcept
            {
                if(locked_) { exn_lock_.clear(::std::memory_order_release); }
                // Work slots own only native pointer values. Actual retired
                // immutable owners remain in the caller's deferred batch.
            }
            [[nodiscard]] inline bool enabled() const noexcept { return retired_ != nullptr; }

            [[nodiscard]] inline gc_object_status prepare_closed_registry() noexcept
            {
                if(!enabled() || !retired_->empty() || retired_->count_ != 0uz)
                { return gc_object_status::invalid_value; }
                constexpr auto maximum{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                                       sizeof(exn_token_entry*)};
                constexpr auto maximum_recipients{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                                                   sizeof(recipient_record)};
                if(native_.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                                         sizeof(::uwvm2::runtime::exception::value_ref) ||
                    (!native_.empty() && native_.data() == nullptr)) { return gc_object_status::invalid_value; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                if(native_view_ != nullptr && (!native_.empty() ||
                   native_view_->size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                                           sizeof(::uwvm2::runtime::exception::value_ref)))
                { return gc_object_status::invalid_value; }
#endif
                // [genuinely closed canonical cohort] lock order is cohort ->
                // exn registry -> one recipient index. Native owners are already
                // stopped and their root-domain census must remain stable.
                while(exn_lock_.test_and_set(::std::memory_order_acquire)) {}
                locked_ = true;
                for(::std::size_t bucket{}; bucket != exn_bucket_count; ++bucket)
                {
                    for(auto* current{exn_buckets_[bucket]}; current != nullptr;)
                    {
                        if(node_count_ == maximum) { return gc_object_status::size_overflow; }
                        if(current->token == exn_token_prefix ||
                           !exn_token_shape(reinterpret_cast<void const*>(current->token)) ||
                           exn_bucket(current->token) != bucket || !current->value ||
                            current->value.use_count() == 0 || current->root_count == 0uz ||
                            canonical_issuer(*current) == nullptr) { return gc_object_status::invalid_reference; }
                        if(current->root_count > maximum_recipients - recipient_capacity_)
                        { return gc_object_status::size_overflow; }
                        recipient_capacity_ += current->root_count;
                        ++node_count_;
                        current->collection_marked = false;
                        current->collection_recipients = 0uz;
                        // [exn-owned initialized chain] lock excludes removal.
                        // ^^ advance only to the initialized successor or null.
                        current = current->next;
                    }
                }
                if(node_count_ != 0uz)
                {
                    // [checked native-node count * pointer width] real array.
                    // Preflight uses it as the sorted membership index; after
                    // preflight the very same slots become the bounded mark
                    // queue. No per-issued-ID side table persists between GCs.
                    work_.reset(new(::std::nothrow) exn_token_entry*[node_count_]);
                    if(!work_) { return gc_object_status::out_of_memory; }
                    ::std::size_t filled{};
                    for(auto* head : exn_buckets_)
                    {
                        for(auto* current{head}; current != nullptr;)
                        {
                            if(filled == node_count_) { return gc_object_status::invalid_reference; }
                            // [0,node_count_) owns the next writable pointer slot.
                            work_[filled] = current;
                            ++filled;
                            // [registry-owned initialized successor/null]
                            // ^^ removal remains excluded by the registry lock.
                            current = current->next;
                        }
                    }
                    if(filled != node_count_) { return gc_object_status::invalid_reference; }
                    // [work_.get(),work_.get()+node_count_) [one-past]
                    // ^^ both endpoints belong to the checked initialized array.
                    ::std::sort(work_.get(), work_.get() + node_count_,
                                ::std::less<exn_token_entry const*>{});
                }
                if(recipient_capacity_ != 0uz)
                {
                    // Counts bound allocation and check index consistency only.
                    // They neither seed marking nor prove an external owner root.
                    recipients_.reset(new(::std::nothrow) recipient_record[recipient_capacity_]);
                    if(!recipients_) { return gc_object_status::out_of_memory; }
                }
                for(::std::size_t index{}; index != store_count_; ++index)
                {
                    auto const& store{*stores_[index]};
                    exn_roots_guard roots_guard{store};
                    auto const slice_start{recipient_size_};
                    ::std::size_t recipient_count{};
                    for(auto* root{store.exn_roots_}; root != nullptr;)
                    {
                        if(recipient_count == node_count_ || recipient_size_ == recipient_capacity_ ||
                           !store.exn_root_buckets_ ||
                            !registered_entry(root->entry)) { return gc_object_status::invalid_reference; }
                        auto& entry{*root->entry};
                        if(entry.collection_recipients == entry.root_count) { return gc_object_status::invalid_reference; }
                        ++entry.collection_recipients;
                        // [0,recipient_capacity_) owns this writable record.
                        recipients_[recipient_size_] = {root, false};
                        ++recipient_size_;
                        ++recipient_count;
                        // [recipient-owned list] node stays live under roots lock.
                        // ^^ move only to its initialized list successor or null.
                        root = root->next;
                    }
                    if(recipient_count > 1uz)
                    {
                        // [slice_start,recipient_size_) is initialized and
                        // nonempty; both offsets are <= checked array capacity.
                        // ^^ form only endpoints within this native array.
                        auto* first{recipients_.get() + slice_start};
                        auto* last{recipients_.get() + recipient_size_};
                        ::std::sort(first, last, [](recipient_record const& left,
                                                  recipient_record const& right) noexcept
                        { return ::std::less<exn_root_node const*>{}(left.root, right.root); });
                    }
                    ::std::size_t indexed_count{};
                    if(store.exn_root_buckets_)
                    {
                        for(::std::size_t bucket{}; bucket != exn_bucket_count; ++bucket)
                        {
                            for(auto* item{(*store.exn_root_buckets_)[bucket]}; item != nullptr;)
                            {
                                if(indexed_count == recipient_count) { return gc_object_status::invalid_reference; }
                                // recipient_count > 0 proves a non-null array;
                                // [slice_start,recipient_size_) is fully live.
                                // ^^ construct endpoints within the proved slice.
                                auto* first{recipients_.get() + slice_start};
                                auto* last{recipients_.get() + recipient_size_};
                                auto* match{::std::lower_bound(first, last, item,
                                    [](recipient_record const& record, exn_root_node const* candidate) noexcept
                                    { return ::std::less<exn_root_node const*>{}(record.root, candidate); })};
                                // Do not read item->entry or item->bucket_next
                                // until this actual list-node identity is proved.
                                if(match == last || match->root != item || match->seen ||
                                   exn_bucket(item->entry->token) != bucket)
                                { return gc_object_status::invalid_reference; }
                                match->seen = true;
                                ++indexed_count;
                                // [proved recipient list node][native bucket link]
                                // ^^ next iteration authenticates the successor
                                // identity before dereferencing any of its fields.
                                item = item->bucket_next;
                            }
                        }
                    }
                    if(indexed_count != recipient_count) { return gc_object_status::invalid_reference; }
                }
                if(recipient_size_ != recipient_capacity_) { return gc_object_status::invalid_reference; }
                for(auto* head : exn_buckets_)
                {
                    for(auto* current{head}; current != nullptr;)
                    {
                        if(current->collection_recipients != current->root_count)
                        { return gc_object_status::invalid_reference; }
                        // Membership counts check index consistency ONLY. They
                        // are never seeds or proof of native/guest reachability.
                        // ^^ registry lock keeps this next member live.
                        current = current->next;
                    }
                }
                for(auto const& value : native_)
                { if(!value || value.use_count() == 0) { return gc_object_status::invalid_value; } }
                // No registered_entry call follows this successful preflight.
                // Marking may now overwrite work_'s membership-index slots.
                recipients_.reset();
                return gc_object_status::ok;
            }
            [[nodiscard]] inline exn_token_entry* find(gc_reference reference) const noexcept
            {
                if(!enabled() || !locked_ ||
                   reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_exn ||
                   !exn_token_shape(reference.storage.ptr)) { return nullptr; }
                // Only the complete opaque issued identity is compared. Never
                // interpret the supplied token as an entry/value/payload address.
                return find_exn_locked(reference.storage.ptr);
            }
            [[nodiscard]] inline gc_object_store const* issuer(gc_reference reference) const noexcept
            {
                auto const* entry{find(reference)};
                return entry == nullptr ? nullptr : canonical_issuer(*entry);
            }
            template<class Visitor>
            [[nodiscard]] inline bool validate_all_values(Visitor&& visitor) const noexcept
            {
                if(!enabled()) { return true; }
                using status = ::uwvm2::runtime::exception::payload_root_status;
                for(auto* head : exn_buckets_)
                {
                    for(auto* current{head}; current != nullptr;)
                    {
                        if(::uwvm2::runtime::exception::visit_immutable_exception_wasm_roots(*current->value, visitor).status != status::ok)
                        { return false; }
                        // [owned immutable value][protected registry successor]
                        // ^^ no payload borrow survives the visitor or next step.
                        current = current->next;
                    }
                }
                return visit_native_values(visitor);
            }
            template<class Visitor>
            [[nodiscard]] inline bool visit_native_values(Visitor&& visitor) const noexcept
            {
                if(!enabled()) { return true; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                if(native_view_ != nullptr)
                {
                    // No copied owning value is created or destroyed while VM
                    // locks are held. Each borrow is strongly retained by the
                    // actual registered lease and its locked domain census.
                    auto const result{native_view_->for_each_value(
                        [&](::uwvm2::runtime::exception::value const& value) noexcept
                    {
                        return ::uwvm2::runtime::exception::visit_immutable_exception_wasm_roots(value, visitor).status ==
                               ::uwvm2::runtime::exception::payload_root_status::ok;
                    })};
                    return result.status == ::uwvm2::runtime::exception::native_exception_visit_status::ok;
                }
#endif
                for(auto const& value : native_)
                {
                    auto const result{::uwvm2::runtime::exception::visit_immutable_exception_wasm_roots(*value, visitor)};
                    if(result.status != ::uwvm2::runtime::exception::payload_root_status::ok) { return false; }
                }
                return true;
            }
            [[nodiscard]] inline bool mark(gc_reference reference) noexcept
            {
                auto* entry{find(reference)};
                if(entry == nullptr) { return false; }
                if(entry->collection_marked) { return true; }
                if(work_count_ == node_count_) { ::std::terminate(); }
                entry->collection_marked = true;
                // [0,node_count_) owns live pointer slots. Initialise the next
                // slot before publishing work_count_; each node is queued once.
                work_[work_count_] = entry;
                ++work_count_;
                return true;
            }
            [[nodiscard]] inline bool has_work() const noexcept { return work_count_ != 0uz; }
            template<class Visitor>
            [[nodiscard]] inline bool pop_and_trace(Visitor&& visitor) noexcept
            {
                if(work_count_ == 0uz) { return false; }
                // [0,work_count_) contains proved registered native entries.
                // ^^ pop a complete pointer before borrowing immutable payload.
                auto const* entry{work_[--work_count_]};
                auto const result{::uwvm2::runtime::exception::visit_immutable_exception_wasm_roots(*entry->value, visitor)};
                return result.status == ::uwvm2::runtime::exception::payload_root_status::ok;
            }
            inline void sweep_registry_after_heap_commit() noexcept
            {
                if(!enabled()) { return; }
                // All graph/root/layout/reservation and recipient-index checks
                // have succeeded. No operation below allocates or can reject;
                // mark bits, not persistent membership, determine liveness.
                for(::std::size_t index{}; index != store_count_; ++index)
                {
                    auto const& store{*stores_[index]};
                    exn_roots_guard roots_guard{store};
                    if(store.exn_root_buckets_)
                    {
                        // Unlink dead bucket edges in one pass BEFORE freeing
                        // recipient nodes from the owning list. Preflight proved
                        // the complete list/bucket bijection under these locks.
                        for(auto& bucket : *store.exn_root_buckets_)
                        {
                            // [real bucket head slot or retained bucket_next]
                            // ^^ link names a live recipient-owned pointer slot.
                            auto** bucket_link{::std::addressof(bucket)};
                            while(*bucket_link != nullptr)
                            {
                                auto* root{*bucket_link};
                                if(root->entry->collection_marked)
                                {
                                    // [retained root][initialized bucket-next slot]
                                    // ^^ advance within a proved live native node.
                                    bucket_link = ::std::addressof(root->bucket_next);
                                }
                                else
                                {
                                    // [predecessor][dead root][bucket successor]
                                    // ^^ retire lookup membership; the owning list
                                    // still pins the node until the next pass.
                                    *bucket_link = root->bucket_next;
                                }
                            }
                        }
                    }
                    // [real recipient list head slot]
                    // ^^ each link names head or a retained live next member.
                    auto** link{::std::addressof(store.exn_roots_)};
                    while(*link != nullptr)
                    {
                        auto* root{*link};
                        if(root->entry->collection_marked)
                        {
                            // [retained recipient node] its next slot remains live.
                            // ^^ advance only to that initialized native slot.
                            link = ::std::addressof(root->next);
                            continue;
                        }
                        if(root->entry->root_count == 0uz) { ::std::terminate(); }
                        // [predecessor][dead recipient][initialized successor]
                        // ^^ remove the list edge before retiring its POD node.
                        *link = root->next;
                        --root->entry->root_count;
                        delete root;
                    }
                    if(store.exn_roots_ == nullptr) { store.exn_root_buckets_.reset(); }
                }
                for(auto& bucket : exn_buckets_)
                {
                    // [real registry bucket head or retained entry next slot]
                    // ^^ link always names a live registry-owned pointer slot.
                    auto** link{::std::addressof(bucket)};
                    while(*link != nullptr)
                    {
                        auto* current{*link};
                        if(current->collection_marked)
                        {
                            current->collection_marked = false;
                            // [retained immutable exception node]
                            // ^^ advance only through its live owned next slot.
                            link = ::std::addressof(current->next);
                            continue;
                        }
                        if(current->root_count != 0uz || retired_->count_ == node_count_) { ::std::terminate(); }
                        // [registry predecessor][dead entry][registered successor]
                        // ^^ remove opaque-token membership before deferring owners.
                        *link = current->next;
                        // [fully detached entry][private caller-owned retired chain]
                        // ^^ reuse next only after all guest/recipient indexes unlink.
                        current->next = retired_->head_;
                        retired_->head_ = current;
                        ++retired_->count_;
                    }
                }
            }
        };
