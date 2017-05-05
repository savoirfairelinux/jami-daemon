include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libringjni.la

PACKAGE=cx.ring.daemon

BUILT_SOURCES=                      \
	ring_wrapper.cpp

ring_wrapper.cpp: jni_interface.i configurationmanager.i managerimpl.i
	$(SWIG) -v -c++ -java -package $(PACKAGE) -o ring_wrapper.cpp jni_interface.i
	$(PYTHON) JavaJNI2CJNI_Load.py \
	-i RingserviceJNI.java \
	-o ringservice_loader.c \
	-t ringservice.c.template \
	-m Ringservice \
	-p $(PACKAGE)
	echo "Appending ring_wrapper.cpp..."
	cat ringservice_loader.c >> ring_wrapper.cpp

libringjni_la_SOURCES = \
	$(BUILT_SOURCES)

libringjni_la_CXXFLAGS = -I../ \
			-I$(top_srcdir)/src \
			-I$(top_srcdir)/src/dring \
			-DPREFIX=\"$(prefix)\" \
			-DPROGSHAREDIR=\"${datadir}/ring\"

CLEANFILES= \
	$(BUILT_SOURCES) \
	cx.ring.Ring.service
