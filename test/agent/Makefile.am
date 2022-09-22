include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I./src $(GUILE_CFLAGS)
AM_LDFLAGS  += $(GUILE_LIBS)

# This wrapper is necessary because Guile is not compiled with ASAN.  If however
# we're using the agent with -fsanitize=address, and just run guile, then
# everything will goes haywire.  Thus, just compile a minimal wrapper with the
# same flags passed while configuring so that we can overcome this.
noinst_PROGRAMS = guile-wrapper

guile_wrapper_SOURCES = build-aux/guile-wrapper.cpp

lib_LTLIBRARIES = libguile-jami.la

libguile_jami_la_SOURCES =          \
	src/main.cpp                \
	src/utils.h                 \
	src/bindings/jami.h         \
	src/bindings/bindings.cpp   \
	src/bindings/bindings.h     \
	src/bindings/account.h      \
	src/bindings/call.h         \
	src/bindings/conversation.h \
	src/bindings/signal.cpp     \
	src/bindings/signal.h

libguile_jami_la_LIBADD = $(top_builddir)/src/libjami.la

MODULES =                                       \
	agent.scm                               \
	examples/active-agent.scm               \
	examples/passive-agent.scm              \
	jami.scm                                \
	jami/account.scm                        \
	jami/call.scm                           \
	jami/conversation.scm                   \
	jami/logger.scm                         \
	jami/signal.scm                         \
	scenarios/bulk-calls/scenario.scm       \
	scenarios/peer-monitor/scenario.scm

GOBJECTS = $(MODULES:%.scm=%.go)

%.go: %.scm
	@echo '  GUILD ' $^
	@ASAN_OPTIONS=alloc_dealloc_mismatch=0:detect_leaks=0 \
	./pre-inst-env guile-wrapper --no-auto-compile -e main -s build-aux/compile-scheme.scm $^ $@


compile: $(GOBJECTS)

CLEANFILES = $(GOBJECTS)
