include $(top_srcdir)/globals.mk

AM_CXXFLAGS += -I$(top_srcdir)/src -I.. $(GUILE_CFLAGS)
AM_LDFLAGS  += $(GUILE_LIBS)

check_PROGRAMS = agent

agent_SOURCES =   \
	agent.cpp \
	agent.h   \
	main.cpp  \
	utils.cpp \
	utils.h

agent_LDADD = $(top_builddir)/src/libring.la
