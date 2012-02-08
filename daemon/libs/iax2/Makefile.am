AM_CFLAGS = -Wall -O2
AM_CFLAGS += -g -Wall -Wstrict-prototypes -I $(top_srcdir)/libs/iax2/
AM_CFLAGS += -DLIBIAX
AM_CFLAGS += -fsigned-char
# -DDEBUG_SUPPORT (doesn't compile with it)
# -DDEBUG_DEFAULT
AM_CFLAGS += $(UCFLAGS)

noinst_LTLIBRARIES = libiax2.la
libiax2_la_SOURCES = answer.h busy.h dialtone.h frame.h iax2.h iax2-parser.c \
					 iax2-parser.h iax.c iax-client.h iax.h jitterbuf.c \
					 jitterbuf.h md5.c md5.h
