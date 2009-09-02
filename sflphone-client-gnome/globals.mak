# Global variables
src = $(top_srcdir)

# Preprocessor flags
AM_CPPFLAGS =		$(DEPS_CFLAGS)										\
					$(LIBSEXY_CFLAGS)									\
					-I$(src)/src										\
					-I$(src)/src/config									\
					-I$(src)/src/dbus									\
					-I$(src)/src/contacts								\
					-I$(src)/src/addressbook							\
					-DDATA_DIR=\""$(prefix)/share/sflphone"\"			\
					-DICONS_DIR=\""$(prefix)/share/sflphone"\"			\
					-DCODECS_DIR=\""$(prefix)/lib/sflphone/codecs"\"	\
					-DPREFIX=\"$(prefix)\"								\
					-DENABLE_TRACE										\
					-DPREFIX=\""$(prefix)"\"							\
					-DSYSCONFDIR=\""$(sysconfdir)"\"					\
					-DDATADIR=\""$(datadir)"\"							\
					-DLIBDIR=\""$(libdir)"\"

