// Private SOURCE candidate. Parsing changes only the cold request enum.
#pragma once
#ifndef UWVM_MODULE
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
# if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
# else
    UWVM_GNU_COLD inline constexpr
# endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_exception_dispatch_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        constexpr auto usage{[]() constexpr noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                u8"uwvm: ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                u8"[error] ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                u8"Usage: ",
                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_llvm_jit_exception_dispatch),
                u8"\n\n");
        }};
        // [parser-owned complete result array][current registered option]
        // [safe] para_curr is in [para_begin,para_end); +1 reaches an element
        // or one-past. Never dereference one-past or an unrelated argv object.
        auto* next{para_curr + 1u};
        if(next == para_end || next->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg)
        { usage(); return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme; }
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using dispatch_t = mode::runtime_llvm_jit_exception_dispatch_t;
        dispatch_t requested{};
        if(next->str == u8"auto") { requested = dispatch_t::auto_policy; }
        else if(next->str == u8"native-unwind") { requested = dispatch_t::native_unwind; }
        else if(next->str == u8"pending-numeric") { requested = dispatch_t::pending_numeric; }
        else { usage(); return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme; }
        // [actual next result element] valid spelling was proved before commit.
        next->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        mode::global_runtime_llvm_jit_exception_dispatch = requested;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
