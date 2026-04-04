#if defined(__clang__)
# if (__clang_major__ >= 22)
// godbolt.org tests: Works on armv8-a clang 22+, x86-64 clang 22+.
#  include <stddefer.h>
#  ifndef __STDC_DEFER_TS25755__
#	error "clang needs -fdefer-ts (and C mode) for defer; add -fdefer-ts to compile flags / .clangd"
#  endif
#  define cf_defer defer
# else
#  error "cf_defer requires Clang 22+ (stddefer / defer)"
# endif
#elif (__GNUC__ >= 4)
// This is supposed to be 3.0+ but in practice (empirically using godbolt.org):
// Works on x86-64 gcc 5.1+, does not work on versions prior.
// Works on ARM64 gcc 4.9.4+, MinGW gcc 11.3+.
// Does not work on every other gcc in the godbolt.org selection.
# define cf_defer _DEFER_COUNTER(__COUNTER__)
# define _DEFER_COUNTER(CNTR) _DEFER_EXPAND(CNTR)
# define _DEFER_EXPAND(CNTR) _DEFER_FUNC(_DEFER_FUNC_##CNTR, _DEFER_VAR_##CNTR)
# define _DEFER_FUNC(F, V)												   \
	auto void F(int*);													  \
	__attribute__((__cleanup__(F), __deprecated__, __unused__))			  \
		int V;															   \
	__attribute__((__always_inline__, __deprecated__, __unused__))		  \
	inline auto void F(__attribute__((__unused__))int* V)
#else
# error "cf_defer -- compiler not supported"
#endif
