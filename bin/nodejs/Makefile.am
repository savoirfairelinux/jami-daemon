include $(top_srcdir)/globals.mk

BUILT_SOURCES=                      \
	ring_wrapper.cpp

ring_wrapper.cpp: jni_interface.i configurationmanager.i managerimpl.i
	$(SWIG) -v -c++ -javascript -node -o ring_wrapper.cpp jni_interface.i

CLEANFILES= \
	$(BUILT_SOURCES)
