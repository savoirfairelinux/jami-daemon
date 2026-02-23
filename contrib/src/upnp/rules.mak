# UPNP
UPNP_VERSION := 1.14.25
PKG_CPE += cpe:2.3:a:pupnp_project:pupnp:$(UPNP_VERSION):*:*:*:*:*:*:*
UPNP_URL := https://github.com/pupnp/pupnp/archive/release-$(UPNP_VERSION).tar.gz

PKGS += upnp

$(TARBALLS)/pupnp-release-$(UPNP_VERSION).tar.gz:
	$(call download,$(UPNP_URL))

.sum-upnp: pupnp-release-$(UPNP_VERSION).tar.gz

upnp: pupnp-release-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
ifeq ($(OS),Windows_NT)
	$(APPLY) $(SRC)/upnp/libupnp-windows.patch
endif
	$(APPLY) $(SRC)/upnp/poll.patch
	$(MOVE)

UPNP_CONF = \
	-DBUILD_TESTING=OFF \
	-DUPNP_BUILD_SHARED=OFF \
	-DUPNP_BUILD_STATIC=ON \
	-DUPNP_BUILD_SAMPLES=OFF \
	-DUPNP_ENABLE_CLIENT_API=ON \
	-DUPNP_ENABLE_DEVICE_API=OFF \
	-DUPNP_ENABLE_WEBSERVER=OFF \
	-DUPNP_ENABLE_BLOCKING_TCP_CONNECTIONS=OFF

CMAKE_PKGS += upnp
