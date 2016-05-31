# Boost

BOOST_VERSION := boost-1.58.0
BOOST_URL := https://github.com/boostorg/boost.git

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
				 --without-mpi --without-python

$(TARBALLS)/boost-modular:
	git clone --depth=1 $(BOOST_URL) $@
	cd $@ && git fetch --tags && git checkout tags/$(BOOST_VERSION) && git submodule update --init && cd ..

.sum-boost: boost-modular
	$(warning $@ not implemented)
	touch $@

boost: boost-modular .sum-boost
	cp -r $(TARBALLS)/boost-modular .
	$(MOVE)

.boost: boost
	cd $< && $(HOSTVARS) ./bootstrap.sh
	cd $< && ./b2 headers
	cd $< && $(HOSTVARS) ./b2 $(BOOST_B2_OPTS) install
	touch $@
