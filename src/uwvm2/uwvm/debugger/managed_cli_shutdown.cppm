module;
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.uwvm.debugger:managed_cli_shutdown;
import fast_io;
import uwvm2.utils.thread;
import uwvm2.runtime;
import :controller;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "managed_cli_shutdown.h"
