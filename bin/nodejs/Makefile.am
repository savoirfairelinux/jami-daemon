include $(top_srcdir)/globals.mk

BUILT_SOURCES= \
	ring_wrapper.cpp \
	build/Makefile \
	build/Release/obj.target/dring.node

ring_wrapper.cpp: nodejs_interface.i configurationmanager.i managerimpl.i
	$(SWIG) -v -c++ -javascript -node -o ring_wrapper.cpp nodejs_interface.i

build/Makefile: ring_wrapper.cpp binding.gyp
	node-gyp configure --target=v12.18.2 --arch=x64

build/Release/obj.target/dring.node: build/Makefile ring_wrapper.cpp callback.h
	node-gyp build

CLEANFILES= \
	$(BUILT_SOURCES) \
	build/Release/dring.node