// Include at TU scope AFTER closing namespace uwvm2::runtime::lib.
#if defined(__linux__) && defined(UWVM_USE_THREAD_LOCAL) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && (defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT))
namespace uwvm2::runtime::wasm_threads
{
    inline import_admission admit_import(::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const& world,
        ::std::size_t module, ::std::size_t imported) noexcept
    { return ::uwvm2::runtime::lib::runtime_threaded_profile_bridge::admit_import_checked(world, module, imported); }
    inline host_dispatch_status dispatch_import(ThreadedGroup::ChildLease& lease,
        ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld const& world,
        ::std::size_t module, ::std::size_t caller, ::std::size_t imported,
        ::std::span<::std::byte> results, ::std::span<::std::byte const> parameters) noexcept
    { return ::uwvm2::runtime::lib::runtime_threaded_profile_bridge::dispatch_import_checked(lease, world, module, caller, imported, results, parameters); }
}
#endif
