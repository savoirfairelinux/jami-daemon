

noinst_LTLIBRARIES = libeXosip2.la

#
# Other flags to add someday:
#	-Wcast-qual
#	-Wwrite-strings
#	-Wstrict-prototypes
#	-Werror
#
AM_CFLAGS = $(EXOSIP_CFLAGS) @CFLAGS@  
noinst_HEADERS = eXosip2.h  inet_ntop.h  jpipe.h

libeXosip2_la_SOURCES =      \
eXosip.c         eXconf.c    \
eXregister_api.c eXsubscription_api.c    \
eXcall_api.c     eXoptions_api.c         \
eXrefer_api.c    eXmessage_api.c         \
eXinsubscription_api.c  eXpublish_api.c     \
eXtransport.c    \
inet_ntop.c      inet_ntop.h   \
jrequest.c       jresponse.c   \
jcallback.c      jdialog.c               udp.c        \
jcall.c          jreg.c                  eXutils.c    \
jnotify.c        jsubscribe.c            jevents.c    \
misc.c           eXosip2.h     \
jpipe.c          jpipe.h                 jauth.c      \
sdp_offans.c     jpublish.c

libeXosip2_la_LIBADD = -losip2  @EXOSIP_LIBS@

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(includedir)
