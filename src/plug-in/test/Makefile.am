include $(top_srcdir)/globals.mak

PLUGIN_LIB = libplugintest.so
libplugintest_so_SOURCES = pluginTest.cpp
libplugintest_so_CXXFLAGS = -fPIC -g -Wall
libplugintest_so_LDFLAGS = --shared -lc
INSTALL_PLUGIN_RULE = install-libplugintest_so

noinst_PROGRAMS = libplugintest.so

install-exec-local:  install-libplugintest_so
uninstall-local:  uninstall-libplugintest_so

install-libplugintest_so: libplugintest.so
	mkdir -p $(sflplugindir)
	$(INSTALL_PROGRAM) libplugintest.so $(sflplugindir)

uninstall-libplugintest_so:
	rm -f $(sflplugindir)/libplugintest.so
