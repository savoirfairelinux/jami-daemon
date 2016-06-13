# RESTBED
RESTBED_VERSION := 4.0
RESTBED_URL := https://github.com/Corvusoft/restbed/archive/$(RESTBED_VERSION).tar.gz


PKGS += restbed
ifeq ($(call need_pkg,"restbed >= 4.0"),)
PKGS_FOUND += restbed
endif

$(TARBALLS)/restbed-$(RESTBED_VERSION).tar.gz:
	$(call download,$(RESTBED_URL))

RESTBED_CONF = -DBUILD_TESTS=NO \
			-DBUILD_EXAMPLES=NO \
			-DBUILD_SSL=NO \
			-DBUILD_SHARED=NO \
			-DCMAKE_C_COMPILER=gcc \
			-DCMAKE_CXX_COMPILER=g++ \
			-DCMAKE_INSTALL_PREFIX=$(PREFIX)

restbed: restbed-$(RESTBED_VERSION).tar.gz
	$(UNPACK)
	git clone https://github.com/Corvusoft/asio-dependency.git $(UNPACK_DIR)/dependency/asio/
	cd $(UNPACK_DIR)/dependency/asio/asio && patch -fp1 < ../../../../../src/restbed/conditional_sslv3.patch
	cd $(UNPACK_DIR)/ && ls && patch -p0 < ../../src/restbed/CMakeLists.patch
	$(MOVE)

.restbed: restbed
	cd $< && cmake $(RESTBED_CONF) .
	cd $< && $(MAKE) install
	touch $@
