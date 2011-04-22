include ../../globals.mak

SUBDIRS = addressbook

noinst_LTLIBRARIES = libcontacts.la

libcontacts_la_SOURCES = \
  searchbar.c \
  calllist.c \
  calltab.c \
  calltree.c \
  history.c \
  addressbook.c \
  conferencelist.c

libcontacts_la_LDFLAGS = @DBUSGLIB_LDFLAGS@ @LIBNOTIFY_LDFLAGS@ \
				@SFLGTK_LDFLAGS@ @SFLGLIB_LDFLAGS@ @WEBKIT_LDFLAGS@ @LIBEBOOK_LDFLAGS@ \
				@LIBEDATASERVER_LDFLAGS@ @LIBEDATASERVERUI_LDFLAGS@ @LIBGNOMEUI_LDFLAGS@ \
				@GNOMEDOCUTILS_LDFLAGS@ 
					  
libcontacts_la_CFLAGS = @DBUSGLIB_CFLAGS@ @LIBNOTIFY_CFLAGS@ \
				@SFLGTK_CFLAGS@ @SFLGLIB_CFLAGS@ @WEBKIT_CFLAGS@ @LIBEBOOK_CFLAGS@ \
				@LIBEDATASERVER_CFLAGS@ @LIBEDATASERVERUI_CFLAGS@ @LIBGNOMEUI_CFLAGS@ \
				@GNOMEDOCUTILS_CFLAGS@

libcontacts_la_LIBADD = ./addressbook/libaddressbook.la
