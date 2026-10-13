module;
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#endif
export module uwvm2.runtime.gc.source_exception_publisher;
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
import uwvm2.runtime.exception.value;
import uwvm2.runtime.exception.native_roots;
import uwvm2.runtime.exception.external_handle;
import uwvm2.runtime.exception.activation;
import uwvm2.runtime.gc.entry_admission;
import uwvm2.runtime.gc.instance_phase;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
import uwvm2.runtime.gc.managed_exception_cohort;
#endif
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.uwvm.runtime.storage;
import uwvm2.runtime;
#ifndef UWVM_MODULE
#define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
#define UWVM_MODULE_EXPORT export
#endif
#include "source_exception_publisher.h"
#endif
