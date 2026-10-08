// Private members of the original section manager, under the independent
// exact-1 candidate gate. These observations do not grant code permission.
private:
    struct private_leaf_fde_range { ::std::uintptr_t begin{}, size{}; };
    ::std::vector<private_leaf_fde_range> private_leaf_fde_ranges_{};
    ::llvm::Triple::ArchType private_leaf_cfi_arch_{::llvm::Triple::UnknownArch};
    bool private_leaf_cfi_enabled_{}, private_leaf_cfi_valid_{};
    void observe_private_leaf_registered_eh_frames(::std::uint8_t const* addr,
        ::std::uint64_t load_addr, ::std::size_t size) noexcept
    {
        if(!private_leaf_cfi_enabled_ || !private_leaf_cfi_valid_) { return; }
#if UWVM2_RUNTIME_LLVM_JIT_SECTION_MEMORY_MANAGER_HAS_DWARF_EH_FRAME && defined(UWVM_CPP_EXCEPTIONS)
        try // Candidate-owned cold parsing only, after original registration.
        {
            constexpr ::std::size_t max_bytes{256uz * 1'024uz * 1'024uz};
            if(addr == nullptr || load_addr == 0u || size == 0uz || size > max_bytes ||
               size > (::std::numeric_limits<::std::uintptr_t>::max)() - reinterpret_cast<::std::uintptr_t>(addr))
            { private_leaf_cfi_valid_ = false; return; }
            // [actual registered relocated native EH section: size bytes] end
            // [safe] The original manager owns this callback span. Parsing
            // borrows it synchronously, never guest integers or external memory.
            ::llvm::DWARFDebugFrame frames{private_leaf_cfi_arch_, true, load_addr};
            auto error{frames.parse(::llvm::DWARFDataExtractor{
                ::llvm::StringRef{reinterpret_cast<char const*>(addr), size},
                ::std::endian::native == ::std::endian::little, static_cast<::std::uint8_t>(sizeof(::std::uintptr_t))})};
            if(error) { ::llvm::consumeError(::std::move(error)); private_leaf_cfi_valid_ = false; return; }
            for(auto const& entry: frames.entries())
            {
                auto const fde{::llvm::dyn_cast<::llvm::dwarf::FDE>(::std::addressof(entry))};
                if(fde == nullptr) { continue; }
                auto const begin{fde->getInitialLocation()}, width{fde->getAddressRange()};
                constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
                if(begin == 0u || begin > limit || width == 0u || width > limit - begin ||
                   private_leaf_fde_ranges_.size() == 4'096uz)
                { private_leaf_cfi_valid_ = false; return; }
                // [decoded actual relocated FDE][checked begin ... end]
                // [safe] Retain only integer code extents, no CFI/native pointer.
                private_leaf_fde_ranges_.push_back({static_cast<::std::uintptr_t>(begin), static_cast<::std::uintptr_t>(width)});
            }
        }
        catch(...) { private_leaf_cfi_valid_ = false; } // Never undo or replace original registration.
#else
        static_cast<void>(addr); static_cast<void>(load_addr); static_cast<void>(size);
        private_leaf_cfi_valid_ = false; // Unobserved registration is not proof.
#endif
    }
public:
    void begin_private_leaf_cfi_observation(::llvm::Triple::ArchType arch) noexcept
    {
        // Called only before this private engine emits/loads code. No ordinary
        // manager opts in, and no parsing is added to guest execution.
        private_leaf_fde_ranges_.clear();
        private_leaf_cfi_arch_ = arch;
        private_leaf_cfi_enabled_ = true;
        private_leaf_cfi_valid_ = arch != ::llvm::Triple::UnknownArch;
    }
    [[nodiscard]] bool private_leaf_has_actual_registered_cfi(
        ::std::uintptr_t begin, ::std::uintptr_t size) const noexcept
    {
        if(!private_leaf_cfi_enabled_ || !private_leaf_cfi_valid_ || finalization_failure_ || begin == 0u || size == 0u ||
           size > (::std::numeric_limits<::std::uintptr_t>::max)() - begin) { return false; }
        for(auto const& fde: private_leaf_fde_ranges_)
        {
            // Require an actual FDE for this exact start and complete loaded
            // symbol extent. Merely seeing any EH section is insufficient.
            if(fde.begin == begin && fde.size >= size) { return true; }
        }
        return false;
    }
