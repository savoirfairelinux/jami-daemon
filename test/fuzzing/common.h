#pragma once

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include <dlfcn.h>

static inline bool
streq(const char* A, const char* B)
{
        return 0 == strcmp(A, B);
}

#define BEGIN_WRAPPER_PRIMITIVE(SYM, RET, NAME, ARGS...)                \
        RET NAME(ARGS)                                                  \
        {                                                               \
        RET (*this_func)(ARGS) = nullptr;                               \
                                                                        \
        if (nullptr == this_func) {                                     \
                this_func = (typeof(this_func)) dlsym(RTLD_NEXT, #SYM); \
                assert(this_func);                                      \
        }

#define BEGIN_METHOD_WRAPPER(SYM, RET, NAME, ARGS...)                   \
        RET NAME(ARGS)                                                  \
        {                                                               \
        RET (*this_func)(typeof(this), ##ARGS) = nullptr;               \
                                                                        \
        if (nullptr == this_func) {                                     \
                this_func = (typeof(this_func)) dlsym(RTLD_NEXT, #SYM); \
                assert(this_func);                                      \
        }

#define BEGIN_WRAPPER(RET, NAME, ARGS...) BEGIN_WRAPPER_PRIMITIVE(NAME, RET, NAME, ARGS)
#define END_WRAPPER(ARGS...) } static_assert(true, "")

#define __weak __attribute__((weak))
#define __used __attribute__((used))

#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))
