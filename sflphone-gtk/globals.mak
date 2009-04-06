# Global variables
src = $(top_srcdir)

# Preprocessor flags
AM_CPPFLAGS = $(DEPS_CFLAGS) \
	-I$(src)/src \
	-I$(src)/src/config \
	-I$(src)/src/dbus \
	-I$(src)/src/contacts \
	-I$(src)/src/addressbook \
	-DICONS_DIR=\""$(prefix)/share/sflphone"\" \
	-DCODECS_DIR=\""$(prefix)/lib/sflphone/codecs"\" 
	-DPREFIX=\"$(prefix)\" \
	$(ZEROCONFFLAGS) \
	-DENABLE_TRACE
