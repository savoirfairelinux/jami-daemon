if HAVE_WIN32
bin_PROGRAMS = ringcli
ringcli_SOURCES = winmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

if HAVE_OSX
bin_PROGRAMS = ringcli
ringcli_SOURCES = osxmain.cpp
ringcli_CXXFLAGS = -I$(top_srcdir)/src \
                   -I$(top_srcdir)/src/dring \
                   -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"
ringcli_LDADD = $(top_builddir)/src/libring.la
endif

if RING_DBUS
SUBDIRS=dbus

sbin_PROGRAMS = dring

dring_SOURCES = main.cpp

dring_CXXFLAGS= -I$(top_srcdir)/src ${DBUSCPP_CFLAGS} \
                -I$(top_srcdir)/src/dring \
                -DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\"

dring_LDADD = dbus/libclient_dbus.la ${DBUSCPP_LIBS} $(top_builddir)/src/libring.la
endif

if RING_RESTCYTHON
SUBDIRS=restcython

all:
	cp restcython/dring .
	chmod +x dring

install:
	mkdir -p /opt/dring_cython/
	./restcython/install.sh restcython /opt/dring_cython/
	sudo cp /opt/dring_cython/restcython/dring /opt/dring_cython/dring
	chmod +x /opt/dring_cython/dring
	ln -s /opt/dring_cython/dring /usr/bin/dring

clean:
	rm -f dring

endif
