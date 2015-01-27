# Global variables
src = $(top_srcdir)

ASTYLERC="$(top_srcdir)/../astylerc"
indent="/usr/bin/astyle"

sfllibdir=$(DESTDIR)$(libdir)/ring
sflplugindir=$(sfllibdir)/plugins

# Preprocessor flags
AM_CPPFLAGS =		$(DEPS_CFLAGS)										\
					$(LIBSEXY_CFLAGS)									\
					-I$(src)/src										\
					-I$(src)/src/config									\
					-I$(src)/src/dbus									\
					-I$(src)/src/contacts								\
					-I$(src)/src/addressbook							\
					-DDATA_DIR=\""$(prefix)/share/ring"\"			\
					-DICONS_DIR=\""$(prefix)/share/ring"\"			\
					-DCODECS_DIR=\""$(prefix)/lib/ring/codecs"\"	\
					-DPREFIX=\"$(prefix)\"								\
					-DENABLE_TRACE										\
					-DPREFIX=\""$(prefix)"\"							\
					-DSYSCONFDIR=\""$(sysconfdir)"\"					\
					-DDATADIR=\""$(datadir)"\"							\
					-DLIBDIR=\""$(libdir)"\"							\
					-DLOCALEDIR=\""$(localedir)"\"							\
					-DRING_UIDIR=\""$(datadir)/ring/ui"\" \
					-DPLUGINS_DIR=\""$(sflplugindir)"\"
indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
		find $(top_srcdir)/src/ -regex ".*\.\(h\|c\)" -exec $(indent) --options=$(ASTYLERC) {} \; ; \
	fi
