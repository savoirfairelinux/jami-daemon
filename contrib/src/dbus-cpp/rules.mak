# DBus-C++

DBUS_CPP_VERSION := 0.9.0
DBUS_CPP_URL := ${SF}/dbus-cplusplus/files/dbus-c%2B%2B/${DBUS_CPP_VERSION}/libdbus-c%2B%2B-${DBUS_CPP_VERSION}.tar.gz/download

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += dbus-cpp
endif
endif

ifeq ($(call need_pkg,"dbus-c++-1 >= 0.9.0"),)
PKGS_FOUND += dbus-cpp
endif

$(TARBALLS)/libdbus-c++-${DBUS_CPP_VERSION}.tar.gz:
	$(call download,$(DBUS_CPP_URL))

.sum-dbus-cpp: $(TARBALLS)/libdbus-c++-${DBUS_CPP_VERSION}.tar.gz

dbus-cpp: $(TARBALLS)/libdbus-c++-${DBUS_CPP_VERSION}.tar.gz .sum-dbus-cpp
	$(UNPACK)
	$(APPLY) $(SRC)/dbus-cpp/dbus-c++-threading.patch
	$(APPLY) $(SRC)/dbus-cpp/dbus-c++-writechar.patch
	$(APPLY) $(SRC)/dbus-cpp/dbus-c++-gcc4.7.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.dbus-cpp: dbus-cpp
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-ecore --disable-glib --disable-tests --disable-examples
	cd $< && $(MAKE) install
	touch $@
