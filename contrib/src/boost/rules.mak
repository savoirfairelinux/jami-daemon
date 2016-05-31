# Boost
BOOST_VERSION := 1_61_0
BOOST_URL := https://downloads.sourceforge.net/project/boost/boost/1.61.0/boost_$(BOOST_VERSION).tar.bz2

PKGS += boost

BOOST_B2_OPTS := variant=release \
				 link=static \
				 --prefix="$(PREFIX)" \
				 --includedir="$(PREFIX)/include" \
				 --libdir="$(PREFIX)/lib" \
				 --build="$(BUILD)" \
				 --host="$(HOST)" \
				 --target="$(HOST)" \
				 --program-prefix="" \
				 --with-system --with-random \
				 -sNO_BZIP2=1 cxxflags=-fPIC cflags=-fPIC define="BOOST_SYSTEM_NO_DEPRECATED"

$(TARBALLS)/boost_$(BOOST_VERSION).tar.bz2:
	$(call download,$(BOOST_URL))

.sum-boost: boost_$(BOOST_VERSION).tar.bz2

boost: boost_$(BOOST_VERSION).tar.bz2 .sum-boost
	$(UNPACK)
	$(MOVE)

.boost: boost
	cd $< && $(HOSTVARS) ./bootstrap.sh
	cd $< && $(HOSTVARS) ./b2 $(BOOST_B2_OPTS) install
	touch $@
