include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I./src $(GUILE_CFLAGS) -rdynamic
AM_LDFLAGS  += $(GUILE_LIBS)

noinst_PROGRAMS = agent.exe

agent_exe_SOURCES =                 \
	src/main.cpp                \
	src/utils.h                 \
	src/bindings/bindings.cpp   \
	src/bindings/bindings.h     \
	src/bindings/account.h      \
	src/bindings/call.h         \
	src/bindings/conversation.h \
	src/bindings/signal.cpp     \
	src/bindings/signal.h

agent_exe_LDADD = $(top_builddir)/src/libjami.la

MODULES =                                       \
	agent.scm                               \
	examples/active-agent.scm               \
	examples/passive-agent.scm              \
	jami/logger.scm                         \
	jami/signal.scm                         \
	scenarios/bulk-calls/scenario.scm       \
	scenarios/peer-monitor/scenario.scm

GOBJECTS = $(MODULES:%.scm=%.go)

%.go: %.scm
	@echo "agent compile $^"
	@ASAN_OPTIONS=alloc_dealloc_mismatch=0:detect_leaks=0 \
	./pre-inst-env agent compile $< $@

compile: $(GOBJECTS)

CLEANFILES = $(GOBJECTS)
