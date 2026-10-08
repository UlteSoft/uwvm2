// Cold integer DATA ownership only. Actual engine/CFI qualification belongs to
// the genuine MCJIT/controller/runtime fixtures, never these synthetic rows.
#include <uwvm2/runtime/lib/uwvm_runtime_checked_tiered_aliases.h>
#include <fast_io.h>
namespace
{
    void require(bool value)
    { if(!value) { ::fast_io::print(::fast_io::err(), "checked tiered alias DATA failure\n"); ::fast_io::fast_terminate(); } }
}
int main()
{
    using owner = ::uwvm2::runtime::lib::details::checked_tiered_native_aliases;
    owner empty{};
    require(empty.complete() && empty.entries().empty() && empty.prepare(0uz));
    require(!empty.append(0uz, 1u, false, false));
    owner oversized{};
    require(!oversized.prepare(owner::maximum_entries + 1uz) && oversized.complete() && oversized.entries().empty());
    owner actual{};
    require(actual.prepare(2uz) && !actual.complete() && actual.entries().empty());
    require(!actual.append(0uz, 0u, false, false));
    require(actual.append(0uz, 16u, false, false) && !actual.complete() && actual.entries().empty());
    require(!actual.prepare(2uz));
    require(actual.append(0uz, 32u, true, true) && actual.complete() && actual.entries().size() == 2uz);
    require(!actual.append(0uz, 48u, false, false));
    auto const rows{actual.entries()};
    require(rows[0uz].address == 16u && !rows[0uz].raw_entry && !rows[0uz].wrapper_entry &&
            rows[1uz].address == 32u && rows[1uz].raw_entry && rows[1uz].wrapper_entry);
    ::fast_io::print(::fast_io::out(), "CHECKED_TIERED_ALIAS_DATA bounded=1 ownership=1 native_authority=0\n");
}
