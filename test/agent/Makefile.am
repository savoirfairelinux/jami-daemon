include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I.. $(GUILE_CFLAGS) -rdynamic
AM_LDFLAGS  += $(GUILE_LIBS)

noinst_PROGRAMS = agent

agent_SOURCES =      \
	agent.cpp    \
	agent.h      \
	bindings.cpp \
	bindings.h   \
	main.cpp

agent_LDADD = $(top_builddir)/src/libring.la
