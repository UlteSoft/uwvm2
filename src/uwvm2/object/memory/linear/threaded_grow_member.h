// Included INSIDE mmap_memory_t. Only explicit threaded profile calls it.
// A real source-generation execution lease owns memory/mutex until physical join.
#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && !defined(UWVM_MODULE)
        [[nodiscard]] threaded_grow_result grow_threaded_profile(::std::size_t delta_pages,
            ::std::size_t original_maximum_bytes,::std::stop_token cancellation) noexcept
        {
            using phase=threaded_grow_status;threaded_grow_result result{};
            // No pre-existing byte may share a newly protected native page.
            // The first profile accepts only normal 64KiB wasm32 pages and a
            // real shared mmap memory. Small/custom pages are not guessed safe.
            if(memory_begin==nullptr||memory_length_p==nullptr||growing_mutex_p==nullptr||
               custom_page_size_log2!=16u||status!=mmap_memory_status_t::wasm32||!sequentially_consistent_size)
                return result;
            auto const [page_size,known_page]=::uwvm2::object::memory::platform_page::get_platform_page_size();
            if(!known_page||page_size==0u||page_size>65536u||!::std::has_single_bit(page_size))return result;
            // Fast_io's Linux native mutex exposes the actual pthread mutex.
            // Direct trylock distinguishes EBUSY from a broken native object;
            // no unchecked bool grants allocation or execution authority.
            for(;;)
            {
                if(cancellation.stop_requested()){result.status=phase::cancelled;return result;}
                auto const error=::pthread_mutex_trylock(::std::addressof(growing_mutex_p->mutex));
                if(error==0)break;
                if(error!=EBUSY){result.os_error=error;return result;}
                ::sched_yield(); // recheck actual token, never futex-block here
            }
            struct unlock final
            {
                ::pthread_mutex_t* actual;
                ~unlock(){if(::pthread_mutex_unlock(actual)!=0)::std::terminate();}
            } held{::std::addressof(growing_mutex_p->mutex)};
            if(cancellation.stop_requested()){result.status=phase::cancelled;return result;}
            auto const old=memory_length_p->load(::std::memory_order_seq_cst);
            result.old_length=old;result.old_pages=old>>16u;
            auto const base=reinterpret_cast<::std::uintptr_t>(memory_begin);
            if((old&(page_size-1u))!=0u||(base&(page_size-1u))!=0u||old>reservation_limit_bytes||
               old>UINT64_C(4294967296)||base>UINTPTR_MAX-old)
                return result;
            auto maximum=(::std::min)(original_maximum_bytes,reservation_limit_bytes);
            if constexpr(sizeof(::std::size_t)>=8u)maximum=(::std::min)(maximum,static_cast<::std::size_t>(UINT64_C(4294967296)));
            if(old>maximum)return result;
            if(delta_pages>(SIZE_MAX>>16u)){result.status=phase::limit;return result;}
            auto const delta=delta_pages<<16u;
            if(delta>maximum-old){result.status=phase::limit;return result;}
            auto const next=old+delta;result.attempted_length=next;
            if(base>UINTPTR_MAX-next)return result;
            if(delta==0u){result.status=phase::success;return result;}
            // Both ends are exact native page boundaries, so [base+old,base+next)
            // is disjoint from ALL previously valid bytes, including rollback.
            auto* begin=memory_begin+old;
            if(details::threaded_profile_mprotect(begin,delta,PROT_READ|PROT_WRITE)!=0)
            {
                result.os_error=errno;
                // POSIX may partially change permissions before an error.
                // No length publication. All admitted software accesses retain
                // old logical length; Root forbids old-engine/source reuse.
                if(details::threaded_profile_mprotect(begin,delta,PROT_NONE)!=0)
                {
                    result.rollback_error=errno;result.tainted=true;
                    result.status=phase::rollback_failed;return result;
                }
                result.status=phase::commit_failed;return result;
            }
            // A stop racing after commit does not roll back a real Wasm grow
            // effect. Publish once; mandatory post-call probe prevents more work.
            memory_length_p->store(next,::std::memory_order_seq_cst);
            result.status=phase::success;return result;
        }
#endif
