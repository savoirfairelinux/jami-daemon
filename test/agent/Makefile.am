include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I.. \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

check_PROGRAMS = agent

agent_SOURCES = agent.cpp agent.h utils.cpp utils.h main.cpp

agent_LDADD = $(top_builddir)/src/libring.la -ldl
