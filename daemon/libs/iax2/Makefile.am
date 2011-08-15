AM_CFLAGS = -Wall -O2
AM_CFLAGS += -g -Wall -Wstrict-prototypes -I .
AM_CFLAGS += -DLIBIAX
AM_CFLAGS += -fsigned-char
# -DDEBUG_SUPPORT (doesn't compile with it)
# -DDEBUG_DEFAULT 
AM_CFLAGS += $(UCFLAGS)

noinst_LTLIBRARIES = libiax2.la
libiax2_la_SOURCES = iax2-parser.c iax.c md5.c jitterbuf.c

