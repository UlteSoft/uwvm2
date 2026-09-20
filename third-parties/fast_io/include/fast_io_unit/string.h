#pragma once

/*
This unit may be entered immediately after an independently scoped core or
DSAL include, whose macro epilogue has already restored the caller's state.
Establishing a balanced local macro/warning scope proves that implementation
attributes do not depend on an incidental outer umbrella; push_macro/pop_macro
nesting also preserves an active outer scope when the unit is aggregated.
*/
#include "../fast_io_dsal/impl/misc/push_warnings.h"
#include "../fast_io_dsal/impl/misc/push_macros.h"

#include "string_impl/impl.h"

#include "../fast_io_dsal/impl/misc/pop_macros.h"
#include "../fast_io_dsal/impl/misc/pop_warnings.h"
