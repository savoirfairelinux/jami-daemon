# OPENCV_CONTRIB
OPENCV_CONTRIB_VERSION := 4.6.0
OPENCV_CONTRIB_URL := https://github.com/opencv/opencv_contrib/archive/$(OPENCV_CONTRIB_VERSION).tar.gz

$(TARBALLS)/opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz:
	$(call download,$(OPENCV_CONTRIB_URL))
.sum-opencv_contrib: opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz
opencv_contrib: opencv_contrib-$(OPENCV_CONTRIB_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.opencv_contrib: opencv_contrib .sum-opencv_contrib
	touch $@
