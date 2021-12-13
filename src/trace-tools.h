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
