# CppUnit
CPPUNIT_VERSION := 1.14.0
CPPUNIT_URL := https://dev-www.libreoffice.org/src/cppunit-1.14.0.tar.gz

PKGS += cppunit
ifeq ($(call need_pkg,"cppunit"),)
PKGS_FOUND += cppunit
endif

$(TARBALLS)/cppunit-$(CPPUNIT_VERSION).tar.gz:
	$(call download,$(CPPUNIT_URL))

.sum-cppunit: cppunit-$(CPPUNIT_VERSION).tar.gz

cppunit: cppunit-$(CPPUNIT_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(SRC)/cppunit/disable-dllplugintester.patch
	$(MOVE)

.cppunit: cppunit .sum-cppunit
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) CXXFLAGS='-DCPPUNIT_NO_TESTPLUGIN' install
	touch $@
