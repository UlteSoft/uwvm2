export module uwvm2.runtime.exception;
export import uwvm2.runtime.exception.value;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
export import uwvm2.runtime.exception.native_roots;
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
export import uwvm2.runtime.exception.external_handle;
#endif
export import uwvm2.runtime.exception.activation;
#endif
export import uwvm2.runtime.exception.roots;
