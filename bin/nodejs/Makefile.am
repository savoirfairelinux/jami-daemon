include $(top_srcdir)/globals.mk

BUILT_SOURCES= \
	ring_wrapper.cpp \
	build/Makefile \
	build/Release/obj.target/dring.node

ring_wrapper.cpp: jni_interface.i configurationmanager.i managerimpl.i
	$(SWIG) -v -c++ -javascript -node -o ring_wrapper.cpp jni_interface.i

build/Makefile: ring_wrapper.cpp binding.gyp
	node-gyp configure

build/Release/obj.target/dring.node: build/Makefile ring_wrapper.cpp
	node-gyp build

CLEANFILES= \
	$(BUILT_SOURCES)
