module;
#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
export module uwvm2test.parsed_checked_module_capsule;
import fast_io_crypto;
import uwvm2.utils.control;
import uwvm2.validation.standard.wasm3;
import uwvm2.uwvm.runtime.checked_source;
import uwvm2.uwvm.runtime.storage;

export namespace uwvm2test::parsed_checked_module_capsule
{
    namespace validation = ::uwvm2::validation::standard::wasm3;
    namespace checked = ::uwvm2::uwvm::runtime::checked_source;
    static_assert(::std::is_same_v<decltype(::std::declval<checked::parsed_integer_source_plan const&>().function(0uz)),
        validation::retained_integer_function_plan::owner>);
    static_assert(::std::is_same_v<decltype(::std::declval<checked::parsed_integer_source_plan const&>().source()),
        ::uwvm2::uwvm::runtime::full::full_source_instance::owner>);
    static_assert(!::std::is_constructible_v<checked::parsed_integer_source_plan>);
    static_assert(!::std::is_copy_constructible_v<checked::parsed_integer_source_plan>);
    static_assert(!::std::is_constructible_v<validation::retained_integer_function_plan>);
    static_assert(::std::is_same_v<decltype(::std::declval<checked::parsed_integer_source_plan const&>().source_module_identities()),
        ::std::span<checked::parsed_source_module_identity const>>);
    static_assert(::std::is_same_v<decltype(checked::parsed_source_image_status{}.error),
        ::uwvm2::utils::control::owned_image_error>);
    static_assert(::fast_io::sha256_context::digest_size == 32uz);
    static_assert(!::std::is_constructible_v<::uwvm2::utils::control::owned_file_image>);

    // This consumer sees retained operation DATA solely through the aggregate
    // module, and sees const source DATA solely through its new public module.
    // It loads no file, grants no guest permission and qualifies no runtime body.
    [[nodiscard]] inline bool exported_owned_model() noexcept
    {
        validation::retained_integer_operation operation{};
        operation.source_offset = 7uz;
        operation.source_bytes = 2uz;
        operation.stack_before = 3uz;
        operation.stack_after = 2uz;
        operation.complete_payload = true;
        checked::parsed_integer_source_plan::owner owner{};
        return !owner && operation.source_offset == 7uz && operation.source_bytes == 2uz &&
            operation.stack_before == 3uz && operation.stack_after == 2uz && operation.complete_payload;
    }
}
