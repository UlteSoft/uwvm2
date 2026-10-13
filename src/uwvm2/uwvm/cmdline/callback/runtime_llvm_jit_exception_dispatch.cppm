module;
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.uwvm.cmdline.callback:runtime_llvm_jit_exception_dispatch;
import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.ansies;
import uwvm2.utils.cmdline;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.cmdline.params;
import uwvm2.uwvm.runtime.runtime_mode;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "runtime_llvm_jit_exception_dispatch.h"
