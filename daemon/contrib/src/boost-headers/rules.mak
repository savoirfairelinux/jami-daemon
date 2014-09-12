# Boost headers (needed for building yaml-cpp)
BOOST_HEADERS_URL = https://gitlab.savoirfairelinux.com/sfl-ports/boost-headers/repository/archive.tar.gz

$(TARBALLS)/boost-headers.tar.gz:
	$(call download,$(BOOST_HEADERS_URL))

.sum-boost-headers: boost-headers.tar.gz

boost-headers: boost-headers.tar.gz .sum-boost-headers
	$(UNPACK)
	mv boost-headers.git $@

.boost-headers: boost-headers
	mkdir -p ../$(HOST)/include
	cp -rf $< ../$(HOST)/include/boost
	touch $@
