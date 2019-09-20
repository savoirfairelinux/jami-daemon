# OPENCV
OPENCV_VERSION := 4.1.1
OPENCV_URL := https://github.com/opencv/opencv/archive/$(OPENCV_VERSION).tar.gz

PKGS += opencv
ifeq ($(call need_pkg,"opencv >= 3.2.0"),)
PKGS_FOUND += opencv
endif

OPENCV_MODULES := "highgui,imgproc,imgcodecs"

OPENCV_CMAKECONF := \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DBUILD_SHARED_LIBS=no \
		-DOPENCV_FORCE_3RDPARTY_BUILD=OFF \
		-DBUILD_LIST=$(OPENCV_MODULES) \
		-DBUILD_ZLIB=OFF \
		-DBUILD_TIFF=OFF \
		-DBUILD_JASPER=OFF \
		-DBUILD_JPEG=OFF \
		-DBUILD_PNG=OFF \
		-DBUILD_OPENEXR=OFF \
		-DBUILD_WEBP=OFF \
		-DBUILD_TBB=OFF \
		-DBUILD_IPP_IW=OFF \
		-DBUILD_ITT=OFF \
		-DBUILD_opencv_apps=OFF \
		-DBUILD_opencv_js=OFF \
		-DBUILD_ANDROID_PROJECTS=OFF \
		-DBUILD_ANDROID_EXAMPLES=OFF \
		-DBUILD_DOCS=OFF \
		-DBUILD_EXAMPLES=OFF \
		-DBUILD_PACKAGE=OFF \
		-DBUILD_PERF_TESTS=OFF \
		-DBUILD_TESTS=OFF \
		-DBUILD_WITH_STATIC_CRT=ON \
		-DBUILD_WITH_DYNAMIC_IPP=OFF \
		-DWITH_JPEG=OFF \
		-DWITH_JASPER=OFF \
		-DWITH_WEBP=OFF \
		-DWITH_PNG=OFF \
		-DWITH_TIFF=OFF \
		-DWITH_GTK=OFF \
		-DWITH_GSTREAMER=OFF \
		-DWITH_VTK=OFF \
		-DWITH_CAROTENE=OFF \
		-DWITH_OPENEXR=OFF \
		-DWITH_WIN32UI=OFF \
		-DWITH_V4L=OFF \
		-DWITH_DSHOW=OFF \
		-DWITH_MSMF=OFF \
		-DWITH_OPENCLAMDFFT=OFF \
		-DWITH_OPENCLAMDBLAS=OFF \
		-DWITH_PROTOBUF=OFF \
		-DWITH_QUIRC=OFF \
		-DWITH_IPP=OFF

$(TARBALLS)/opencv-$(OPENCV_VERSION).tar.gz:
	$(call download,$(OPENCV_URL))
.sum-opencv: opencv-$(OPENCV_VERSION).tar.gz
opencv: opencv-$(OPENCV_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)
.opencv: opencv toolchain.cmake .sum-opencv
	cd $< && mkdir -p build
	cd $< && cd build && $(HOSTVARS) $(CMAKE) .. $(OPENCV_CMAKECONF)
	cd $< && cd build && $(MAKE) install
ifdef HAVE_ANDROID
	cp -R $(PREFIX)/sdk/native/jni/include/* $(PREFIX)/include
	cp -R $(PREFIX)/sdk/native/staticlibs/$(ANDROID_ABI)/* $(PREFIX)/lib
	cp -R $(PREFIX)/sdk/native/3rdparty/libs/$(ANDROID_ABI)/* $(PREFIX)/lib
endif
	touch $@
