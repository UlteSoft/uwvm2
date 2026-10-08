#include <type_traits>
#include <uwvm2/utils/control/protected_assets.h>
using registry = ::uwvm2::utils::control::protected_asset_registry;
using token = ::uwvm2::utils::control::protected_asset_token;
static_assert(!::std::is_default_constructible_v<registry>);
static_assert(!::std::is_constructible_v<registry, int>);
static_assert(!::std::is_copy_constructible_v<registry>);
static_assert(!::std::is_move_constructible_v<registry>);
static_assert(!::std::is_constructible_v<token, int>);
static_assert(!::std::is_constructible_v<::uwvm2::utils::control::protected_asset_details::entry,
    ::fast_io::native_file&&, ::uwvm2::utils::control::protected_asset_identity,
    ::uwvm2::utils::control::protected_asset_role, ::std::uint64_t>);
static_assert(!::std::is_constructible_v<token, ::uwvm2::utils::control::protected_asset_identity>);
int main()
{
    token empty{};
    if(static_cast<bool>(empty)) { ::fast_io::fast_terminate(); }
    ::fast_io::io::println("PASS protected assets inaccessible construction; runtime manager producer still unavailable");
}
