// Included INSIDE ThreadedGroup after SpawnReservation and launch_result.
// All submitting factories remain private friends of the actual Runtime bridge.
    public:
        enum class compiler_completion : unsigned { launched, rejected, cancelled, native_failure };
        struct compiler_result
        {
            compiler_completion completion{compiler_completion::rejected};
            launch_result launched{};
            int native_error{};
        };
        enum class compiler_wait_status : unsigned { pending, completed, group_stopped, invalid };
        struct compiler_observation
        {
            compiler_wait_status status{compiler_wait_status::invalid};
            compiler_result result{};
        };

        class CompilerLease final
        {
            friend class ThreadedGroup;
            friend class ::uwvm2::runtime::lib::runtime_threaded_profile_bridge;
            ThreadedGroup* group_{};
            compiler_slot* job_{};
            ::std::uint64_t sequence_{};
            CompilerLease(ThreadedGroup& group, compiler_slot& job, ::std::uint64_t sequence) noexcept
                : group_{&group}, job_{&job}, sequence_{sequence} {}
            // Only Root's actual concrete job may feed this real ticket to the
            // MainActive hook. It cannot manufacture another reservation.
            [[nodiscard]] SpawnReservation& actual_reservation() const noexcept;
        public:
            CompilerLease(CompilerLease const&)=delete;
            CompilerLease& operator=(CompilerLease const&)=delete;
            [[nodiscard]] bool owns_current_native_thread() const noexcept;
            [[nodiscard]] auto const& admission() const noexcept { return *group_->admission_; }
            [[nodiscard]] ::std::shared_ptr<ThreadedSpawnAdmission const> retained_admission() const noexcept
            { return group_->admission_; }
            [[nodiscard]] ::std::stop_token cancellation() const noexcept { return group_->cancellation_.get_token(); }
            [[nodiscard]] ::std::size_t native_stack_usable_bytes() const noexcept;
            // Distinct from this compiler pthread's stack: actual reserved and
            // already mapped Guest slot that this exact job may launch.
            [[nodiscard]] ::std::size_t guest_native_stack_usable_bytes() const noexcept;
            [[nodiscard]] launch_result launch(::std::unique_ptr<ChildWork>, ::std::uint32_t argument) noexcept;
        };

        class CompilerWork
        {
        protected:
            CompilerWork() noexcept=default;
        public:
            virtual ~CompilerWork()=default;
            [[nodiscard]] virtual ::std::shared_ptr<ThreadedSpawnAdmission const> retained_admission() const noexcept=0;
            // Trusted concrete Root job reports sizeof(actual job) plus every
            // owned submission-buffer capacity. No unbounded callback container.
            [[nodiscard]] virtual ::std::size_t owned_submission_bytes() const noexcept=0;
            // Construct fresh World + engine and (only if admitted and unstopped)
            // perform MainActive/refresh/actual lease.launch. No default success.
            [[nodiscard]] virtual compiler_result run(CompilerLease&) noexcept=0;
        };

        class CompilerTicket final
        {
            friend class ThreadedGroup;
            ThreadedGroup* group_{};
            ::std::shared_ptr<ThreadedGroup> retained_group_;
            ::std::size_t position_{};
            ::std::uint64_t sequence_{};
            CompilerTicket(::std::shared_ptr<ThreadedGroup> group, ::std::size_t position, ::std::uint64_t sequence) noexcept
                : group_{group.get()}, retained_group_{::std::move(group)}, position_{position}, sequence_{sequence} {}
        public:
            CompilerTicket() noexcept=default;
            CompilerTicket(CompilerTicket const&)=delete;
            CompilerTicket& operator=(CompilerTicket const&)=delete;
            CompilerTicket(CompilerTicket&& other) noexcept
                : group_{::std::exchange(other.group_,nullptr)}, retained_group_{::std::move(other.retained_group_)},
                  position_{other.position_}, sequence_{other.sequence_} {}
            CompilerTicket& operator=(CompilerTicket&& other) noexcept
            {
                if(this!=&other)
                { reset(); group_=::std::exchange(other.group_,nullptr); retained_group_=::std::move(other.retained_group_);
                  position_=other.position_; sequence_=other.sequence_; }
                return *this;
            }
            ~CompilerTicket() { reset(); }
            [[nodiscard]] explicit operator bool() const noexcept { return group_!=nullptr; }
            [[nodiscard]] compiler_observation wait_until(::std::chrono::steady_clock::time_point) const noexcept;
            // Completed metadata is recycled only after this exact nonreused
            // sequence is released. Dropping queued/running observation closes
            // the whole Group but NEVER drops the running job or its reservation.
            void reset() noexcept;
        };
        struct compiler_submission { CompilerTicket ticket; int native_error{}; };

    private:
        struct native_stack
        {
            void* mapping{};
            ::std::size_t bytes{}, guard_bytes{};
            int preparation_error{};
            [[nodiscard]] void* usable_begin() const noexcept
            { return mapping ? static_cast<void*>(static_cast<::std::byte*>(mapping)+guard_bytes) : nullptr; }
            [[nodiscard]] ::std::size_t usable_bytes() const noexcept
            { return mapping && guard_bytes<=bytes/2u ? bytes-guard_bytes*2u : 0u; }
        };
        [[nodiscard]] native_stack allocate_native_stack(::std::size_t) const noexcept;
        [[nodiscard]] static int release_native_stack(native_stack&) noexcept;
        [[nodiscard]] int start_compiler_worker() noexcept;
        [[nodiscard]] bool charge_runtime_control_storage(::std::size_t bytes) noexcept;
        [[nodiscard]] compiler_submission submit_compilation(SpawnReservation&&, ::std::unique_ptr<CompilerWork>) noexcept;
        [[nodiscard]] compiler_observation observe_compilation(::std::size_t, ::std::uint64_t,
            ::std::chrono::steady_clock::time_point) noexcept;
        void abandon_compilation(::std::size_t, ::std::uint64_t) noexcept;
        [[nodiscard]] bool actual_compiler_reservation(SpawnReservation const&) noexcept;
        [[nodiscard]] int prepare_compiler_guest_stack(SpawnReservation const&) noexcept;
        static void* compiler_native_entry(void*) noexcept;
        [[nodiscard]] int retire_compiler_once(::std::size_t& native_pending,
            ::std::size_t& preparing, ::std::size_t& cleanup_pending) noexcept;
        [[nodiscard]] int retire_slot_storage(::std::size_t, ::std::uint32_t expected_tid) noexcept;
