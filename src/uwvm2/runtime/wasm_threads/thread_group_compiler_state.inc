// Included INSIDE ThreadedGroup's private state. Fixed N+1 job slots and an
// intrusive FIFO; no vector growth, detached thread, callback or future heap.
        enum class compiler_job_phase : unsigned { empty, queued, running, completed };
        struct compiler_slot
        {
            compiler_job_phase phase{compiler_job_phase::empty};
            ::std::uint64_t sequence{};
            ::std::size_t next{SIZE_MAX}, charged_bytes{};
            bool abandoned{};
            bool ticket_live{};
            SpawnReservation reservation;
            ::std::unique_ptr<CompilerWork> work;
            compiler_result result{};
            launch_result actual_launch{};
        };
        ::std::unique_ptr<compiler_slot[]> compiler_jobs_;
        ::std::size_t compiler_head_{SIZE_MAX}, compiler_tail_{SIZE_MAX};
        ::std::uint64_t next_compiler_sequence_{1u};
        ::std::size_t active_compiler_tickets_{};
        compiler_slot* compiler_current_{};
        ::pthread_t compiler_thread_{};
        native_stack compiler_stack_;
        ::std::shared_ptr<ThreadedGroup> compiler_native_retention_;
        bool compiler_started_{}, compiler_starting_{}, compiler_created_{}, compiler_joined_{}, compiler_closing_{}, compiler_body_returned_{};
        bool compiler_stack_charged_{};
        bool runtime_control_charged_{};
        ::std::size_t page_bytes_{}, native_stack_reserved_{}, fixed_storage_bytes_{}, compiler_payload_reserved_{};
