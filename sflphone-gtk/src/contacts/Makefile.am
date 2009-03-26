include ../../globals.mak

SUBDIRS = addressbook

noinst_LTLIBRARIES = libcontacts.la

libcontacts_la_SOURCES = \
  searchbar.c \
  calllist.c \
  calltab.c \
  calltree.c \
  history.c \
  addressbook.c

libcontacts_la_LDFLAGS = @DEPS_LDFLAGS@
					  
libcontacts_la_CFLAGS = @DEPS_CFLAGS@ 

libcontacts_la_LIBADD = ./addressbook/libaddressbook.la