#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#pragma once
#ifndef UWVM_MODULE
# include "value.h"
# include "native_roots.h"
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
# include "external_handle.h"
#endif
# include "activation.h"
# include "roots.h"
#endif
#else
#pragma once
#ifndef UWVM_MODULE
# include "value.h"
# include "roots.h"
#endif
#endif
