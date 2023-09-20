# OPENCV
OPENCV_VERSION := 4.6.0
OPENCV_CONTRIB_VERSION := 4.6.0
OPENCV_URL := https://github.com/opencv/opencv/archive/$(OPENCV_VERSION).tar.gz

DEPS_opencv += opencv_contrib

OPENCV_CMAKECONF := \
		-DWITH_FFMPEG=OFF \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DBUILD_SHARED_LIBS=no \
		-DOPENCV_EXTRA_MODULES_PATH="./../../${HOST}/../opencv_contrib/modules" \
		-DOPENCV_FORCE_3RDPARTY_BUILD=OFF \
		-DENABLE_PRECOMPILED_HEADERS=ON \
		-DBUILD_ZLIB=OFF \
		-DBUILD_TIFF=OFF \
		-DBUILD_JASPER=OFF \
		-DBUILD_JPEG=ON \
		-DBUILD_PNG=ON \
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
		-DWITH_JPEG=ON \
		-DWITH_JASPER=OFF \
		-DWITH_WEBP=OFF \
		-DWITH_PNG=ON \
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
		-DWITH_IPP=OFF \
		-DBUILD_opencv_aruco=OFF \
		-DBUILD_opencv_barcode=OFF \
		-DBUILD_opencv_bgsegm=OFF \
		-DBUILD_opencv_bioinspired=OFF \
		-DBUILD_opencv_calib3d=OFF \
		-DBUILD_opencv_ccalib=OFF \
		-DBUILD_opencv_datasets=OFF \
		-DBUILD_opencv_dnn_objdetect=OFF \
		-DBUILD_opencv_dnn_superres=OFF \
		-DBUILD_opencv_dpm=OFF \
		-DBUILD_opencv_face=OFF \
		-DBUILD_opencv_features2d=OFF \
		-DBUILD_opencv_flann=OFF \
		-DBUILD_opencv_fuzzy=OFF \
		-DBUILD_opencv_gapi=OFF \
		-DBUILD_opencv_hfs=OFF \
		-DBUILD_opencv_highgui=OFF \
		-DBUILD_opencv_img_hash=OFF \
		-DBUILD_opencv_intensity_transform=OFF \
		-DBUILD_opencv_java_bindings_generator=OFF \
		-DBUILD_opencv_js_bindings_generator=OFF \
		-DBUILD_opencv_line_descriptor=OFF \
		-DBUILD_opencv_mcc=OFF \
		-DBUILD_opencv_ml=OFF \
		-DBUILD_opencv_objc_bindings_generator=OFF \
		-DBUILD_opencv_objdetect=OFF \
		-DBUILD_opencv_optflow=OFF \
		-DBUILD_opencv_phase_unwrapping=OFF \
		-DBUILD_opencv_photo=OFF \
		-DBUILD_opencv_plot=OFF \
		-DBUILD_opencv_python_bindings_generator=OFF \
		-DBUILD_opencv_python_tests=OFF \
		-DBUILD_opencv_quality=OFF \
		-DBUILD_opencv_rapid=OFF \
		-DBUILD_opencv_reg=OFF \
		-DBUILD_opencv_rgbd=OFF \
		-DBUILD_opencv_saliency=OFF \
		-DBUILD_opencv_shape=OFF \
		-DBUILD_opencv_stereo=OFF \
		-DBUILD_opencv_stitching=OFF \
		-DBUILD_opencv_structured_light=OFF \
		-DBUILD_opencv_superres=OFF \
		-DBUILD_opencv_surface_matching=OFF \
		-DBUILD_opencv_text=OFF \
		-DBUILD_opencv_tracking=OFF \
		-DBUILD_opencv_video=OFF \
		-DBUILD_opencv_videoio=OFF \
		-DBUILD_opencv_videostab=OFF \
		-DBUILD_opencv_wechat_qrcode=OFF \
		-DBUILD_opencv_xfeatures2d=OFF \
		-DBUILD_opencv_ximgproc=OFF \
		-DBUILD_opencv_xobjdetect=OFF \
		-DBUILD_opencv_xphoto=OFF

ifdef HAVE_MACOSX
OPENCV_CMAKECONF += \
		-DOPENCV_EXTRA_CXX_FLAGS="-D'CV_PAUSE(int)'"
endif

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
