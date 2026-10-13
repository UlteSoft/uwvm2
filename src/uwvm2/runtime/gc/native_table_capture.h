// PRIVATE SOURCE ONLY: insert in sealed_compact_entry, before public:.
// No additional authority constructor/callback/TLS entry is introduced.
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
        void clear_local_table_capture() noexcept
        {
            // armed_nonce has ALREADY been set to zero, before collection,
            // descriptor/pin retirement, or any possible grow/native reentry.
            view_.table_ready = 0u;
            view_.table_elements = nullptr;
            view_.table_extent = 0uz;
            view_.table_address64 = 0u;
        }
        void capture_current_local_table(::std::uint_least32_t exact_type) noexcept
        {
            using slot = local_defined_table_elem_storage_t;
            static_assert(::std::is_standard_layout_v<slot> && ::std::is_trivially_copyable_v<slot>);
            static_assert(sizeof(slot) == sizeof(gc_reference) && sizeof(slot) == 16uz);
            static_assert(sizeof(slot::storage) == sizeof(::std::uintptr_t) && sizeof(slot::type) == 4uz);
            clear_local_table_capture();
            if(!live_authority() || !authority_ || module_->imported_table_vec_storage.size() != 0uz ||
               module_->local_defined_table_vec_storage.size() != 1uz) { return; }
            auto& table{const_cast<local_defined_table_storage_t&>(
                module_->local_defined_table_vec_storage.index_unchecked(0uz))};
            if(!authority_->local_defined_table(*module_, &table, 0uz) || !table.table_type_ptr ||
               !table.table_type_ptr->has_core_type || runtime_table_family(table) != runtime_table_reference_family::gc)
            { return; }
            auto const expected{table.table_type_ptr->core_type};
            if(expected.kind != gc_type::value_kind::reference) { return; }
            // Cold exact owned-layout relation, including equal canonical IDs
            // at distinct indices. No canonical registry lock on the hot edge.
            // This inline slice only publishes NONNULL wasm_struct carriers;
            // nullable/null/i31/array/ref/foreign values still use old helpers.
            bool accepts{};
            if(expected.heap.is_defined())
            {
                if(expected.heap.code > static_cast<::std::int_least64_t>(UINT32_MAX)) { return; }
                accepts = gc_object_store::canonical_subtype(store_pin_.get(), exact_type,
                    store_pin_.get(), static_cast<::std::uint_least32_t>(expected.heap.code));
            }
            else
            {
                auto const heap{expected.heap.code};
                accepts = heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::any) ||
                    heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::eq) ||
                    heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::struct_);
            }
            if(!accepts) { return; }
            auto const extent{table.elems.size()};
            constexpr auto max_elements{static_cast<::std::size_t>(PTRDIFF_MAX) / sizeof(slot)};
            if(extent > max_elements || (extent != 0uz && table.elems.data() == nullptr)) { return; }
            // [REAL vector allocation base ... initialized extent) is pinned
            // through its canonical module and the genuine entry exclusion.
            // The descriptor/range DOES NOT own this table allocation. All
            // grow/native/callback/EH/debug/multi-entry boundaries first revoke
            // this view; a collector/poll clears it before scans and refreshes
            // from the actual live table after the original allocation_poll.
            view_.table_elements = table.elems.data();
            view_.table_extent = extent;
            view_.table_address64 = runtime_table_is_address64(*module_, 0uz) ? 1u : 0u;
            view_.table_ready = 1u; // plain native-owned fields precede armed RELEASE
        }
#endif
