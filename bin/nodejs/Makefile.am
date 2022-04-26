include $(top_srcdir)/globals.mk

BUILT_SOURCES= \
	jami_wrapper.cpp \
	build/Makefile \
	build/Release/obj.target/jami.node

jami_wrapper.cpp: nodejs_interface.i configurationmanager.i managerimpl.i callmanager.i
	$(SWIG) -v -c++ -javascript -node -o jami_wrapper.cpp nodejs_interface.i

build/Makefile: jami_wrapper.cpp binding.gyp
	node-gyp configure --target=v16.14.2 --arch=x64

build/Release/obj.target/jami.node: build/Makefile jami_wrapper.cpp callback.h
	node-gyp build

CLEANFILES= \
	$(BUILT_SOURCES) \
	build/Release/jami.node