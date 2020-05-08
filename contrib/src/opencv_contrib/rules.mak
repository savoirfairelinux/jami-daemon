# OPENCV_CONTRIB
OPENCV_CONTRIB_VERSION := 4.1.1
OPENCV_CONTRIB_URL := https://github.com/opencv/opencv_contrib/archive/$(OPENCV_CONTRIB_VERSION).tar.gz
ifeq ($(call need_pkg,"opencv_contrib >= 3.2.0"),)
PKGS_FOUND += opencv_contrib
PKGS += opencv_contrib
endif

$(TARBALLS)/opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz:
	$(call download,$(OPENCV_CONTRIB_URL))
.sum-opencv_contrib: opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz
opencv_contrib: opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.opencv_contrib: opencv_contrib .sum-opencv_contrib
	touch $@
