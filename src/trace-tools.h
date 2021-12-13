#pragma once

#ifdef ENABLE_TRACEPOINT
/*
 * NOTE!  source_location is a C++20 feature.  However, Jami does not compile
 * with C++20.  But we can still use the experimental header to get the C++20
 * stuffs.
 */
#  include <experimental/source_location>
#  define CURRENT_FILENAME() std::experimental::source_location::current().file_name()
#  define CURRENT_LINE()     std::experimental::source_location::current().line()
#else
#  define CURRENT_FILENAME() ""
#  define CURRENT_LINE()     0
#endif
