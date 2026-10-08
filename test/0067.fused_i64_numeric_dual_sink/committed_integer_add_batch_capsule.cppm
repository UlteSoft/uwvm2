module;
#include <cstddef>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2test.committed_integer_add_batch;
import uwvm2.validation.standard.wasm3;
static_assert(sizeof(::uwvm2::validation::standard::wasm3::committed_integer_add_batch<false>) == 1u);
static_assert(requires(::uwvm2::validation::standard::wasm3::committed_integer_add_batch<true>& owned)
{ owned.record_provider(0u, 0u, 1u); owned.record_add(1u); owned.complete(); });
