# Run in RAM - Don't fill my disk!
set environment XDG_DATA_HOME=/tmp/jami
set environment XDG_CONFIG_HOME=/tmp/jami
set environment XDG_CACHE_HOME=/tmp/jami

# Get full backtrace on any error detected
set environment UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
set environment ASAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# All TLS conversation to out and stop if log level over critical
set environment SUPERVISOR_TLS_OUT=out
set environment SUPERVISOR_LOG=CRIT

# Break on any error repport by ASAN or if sanitizer die
break __asan::ReportGenericError
break __sanitizer::Die

run
