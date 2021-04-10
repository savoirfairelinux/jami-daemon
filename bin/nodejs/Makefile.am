include $(top_srcdir)/globals.mk

BUILT_SOURCES= \
	jami_wrapper.cpp \
	build/Makefile \
	build/Release/obj.target/dring.node

jami_wrapper.cpp: nodejs_interface.i configurationmanager.i managerimpl.i
	$(SWIG) -v -c++ -javascript -node -o jami_wrapper.cpp nodejs_interface.i

build/Makefile: jami_wrapper.cpp binding.gyp
	node-gyp configure --target=v14.16.1 --arch=x64

build/Release/obj.target/dring.node: build/Makefile jami_wrapper.cpp callback.h
	node-gyp build

CLEANFILES= \
	$(BUILT_SOURCES) \
	build/Release/dring.node