# Global variables
src = $(top_srcdir)

ASTYLERC="$(top_srcdir)/../astylerc"
indent="/usr/bin/astyle"

sfllibdir=$(DESTDIR)$(libdir)/sflphone
sflplugindir=$(sfllibdir)/plugins

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
					-DLIBDIR=\""$(libdir)"\"							\
					-DLOCALEDIR=\""$(localedir)"\"							\
					-DSFLPHONE_UIDIR=\""$(datadir)/sflphone/ui"\" \
					-DPLUGINS_DIR=\""$(sflplugindir)"\"
indent:
	@echo "Indenting code:"
	if [ -f $(ASTYLERC) ] ; then \
		find $(top_srcdir)/src/ -regex ".*\.\(h\|c\)" -exec $(indent) --options=$(ASTYLERC) {} \; ; \
	fi
