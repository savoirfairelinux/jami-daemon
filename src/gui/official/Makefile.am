noinst_PROGRAMS = test

BUILT_SOURCES = 

test_SOURCES = \
	$(BUILT_SOURCES) \
	account.cpp answerreceiver.cpp call.cpp  main.cpp  request.cpp  requesterimpl.cpp  session.cpp  sessionio.cpp  


CLEANFILES = \
	$(BUILT_SOURCES)

test_LDFLAGS = $(QT_LDFLAGS) $(X_LDFLAGS) -static
test_LDADD =  ../../../utilspp/libutilspp.la -lpthread $(LIBQT)
KDE_CXXFLAGS = $(USE_EXCEPTIONS)
AM_CPPFLAGS = $(QT_INCLUDES) $(X_INCLUDES) -I$(top_srcdir)/src




