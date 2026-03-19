// --- Unique name machinery -------------------------------------------------

#define _DC_CAT(a, b) a##b
#define _DC_XCAT(a, b) _DC_CAT(a, b)

#define _DC_UNIQUE_ID __COUNTER__
#define _DC_UNIQUE(base) _DC_XCAT(base, _DC_UNIQUE_ID)


// --- Clang-safe defer implementation (no nested functions) -----------------

#if defined(__clang__)

// Generic defer helper
typedef void (*_dc_defer_fn_t)(void *);

struct _dc_defer_helper {
    _dc_defer_fn_t fn;
    void *ctx;
};

static inline void _dc_defer_invoke(struct _dc_defer_helper *h) {
    h->fn(h->ctx);
}

// Wrap user code in a static inline function
#define defer_code(code)                                                        \
    static inline void _DC_UNIQUE(__defer_wrap__)(void *unused) { code; }       \
    struct _dc_defer_helper _DC_UNIQUE(__defer_obj__)                           \
        __attribute__((cleanup(_dc_defer_invoke))) = {                          \
            _DC_UNIQUE(__defer_wrap__), NULL                                    \
        }


// --- GCC version using nested functions ------------------------------------

#else  // GCC

#define defer_code(code)                                                        \
    auto void _DC_UNIQUE(__defer_fn__)(void *p __attribute__((unused)));        \
    void _DC_UNIQUE(__defer_fn__)(void *p __attribute__((unused))) { code; }    \
    __attribute__((cleanup(_DC_UNIQUE(__defer_fn__))))                          \
    int _DC_UNIQUE(__defer_dummy__) = 0

#endif
