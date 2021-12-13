#pragma once

#ifdef ENABLE_TRACEPOINTS
/*
 * GCC Only.  We use these instead of classic __FILE__ and __LINE__ because in
 * these are evaluated where invoked and not expanded.
 */
#  define CURRENT_FILENAME() __builtin_FILE()
#  define CURRENT_LINE()     __builtin_LINE()
#else
#  define CURRENT_FILENAME() ""
#  define CURRENT_LINE()     0
#endif

#if HAVE_CXXABI_H
#include <cxxabi.h>

template<typename T>
std::string UNMANGLE_TYPE()
{
    int err;
    char *raw;
    std::string ret;

    raw = abi::__cxa_demangle(typeid(T).name(), 0, 0, &err);

    if (0 == err) {
        ret = raw;
    } else {
        ret = typeid(T).name();
    }

    std::free(raw);

    return ret;
}

#else
template<typename T>
std::string UNMANGLE_TYPE()
{
    return typeid(T).name();
}
#endif
