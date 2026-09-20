#pragma once

// Locale width intentionally has no independent reserve/define customization.
//
// `lc_status_print.h` recursively rebuilds every semantic width node around locale-bound leaf objects. The ordinary
// semantic width engine then owns precise/bounded measurement, left/middle/right/internal placement, repeated-fill
// policy, stack-versus-dynamic storage, output-buffer commits, and streaming fallbacks. Keeping a second implementation
// here previously depended on removed helpers such as `handle_common_ch`; more importantly, it let locale and ordinary
// width choose different allocation and syscall strategies for the same output object.
//
// The rebinding proof is structural: a bridge stores only pointers to the locale aggregate and to a normalized argument,
// both of which outlive the synchronous semantic emission. Width and condition nodes rebuilt around that bridge preserve
// their original predicate, placement, width, and fill character. `print_define_internal_shift` is separately forwarded
// by the bridge, so internal placement also observes the same sign/prefix boundary as the locale leaf customization.
