# Boost
BOOST_VERSION_1 := 1.61.0
BOOST_VERSION_2 := 1_61_0
BOOST_URL := $(SF)/boost/files/boost/$(BOOST_VERSION_1)/boost_$(BOOST_VERSION_2).tar.bz2/download

PKGS += boost

BOOST_B2_OPTS := variant=release \
				 link=static \
				 -d0 \
				 --prefix="$(PREFIX)" \
				 --includedir="$(PREFIX)/include" \
				 --libdir="$(PREFIX)/lib" \
				 --build="$(BUILD)" \
				 --host="$(HOST)" \
				 --target="$(HOST)" \
				 --program-prefix="" \
				 --with-system --with-random \
				 define="BOOST_SYSTEM_NO_DEPRECATED" \
				 -sNO_BZIP2=1 cxxflags=-fPIC cflags=-fPIC

ifdef HAVE_WIN32
BOOST_B2_OPTS += target-os=windows \
				 threadapi=win32 \
				 runtime-link=static \
				 binary-format=pe \
				 architecture=x86 \
				 --user-config=user-config.jam \
				 cxxflags="-std=c++11 -fPIC -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions --param=ssp-buffer-size=4"
endif
ifdef HAVE_ANDROID
BOOST_B2_OPTS += --user-config=user-config.jam
endif

$(TARBALLS)/boost_$(BOOST_VERSION_2).tar.bz2:
	$(call download,$(BOOST_URL))

.sum-boost: boost_$(BOOST_VERSION_2).tar.bz2

boost: boost_$(BOOST_VERSION_2).tar.bz2 .sum-boost
	$(UNPACK)
	$(MOVE)

.boost: boost
ifdef HAVE_WIN32
	cd $< && echo "using gcc : mingw64 : ${HOST}-g++" > user-config.jam
	cd $< && echo ":" >> user-config.jam
	cd $< && echo "<rc>${HOST}-windres" >> user-config.jam
	cd $< && echo "<archiver>${HOST}-ar" >> user-config.jam
	cd $< && echo ";" >> user-config.jam
endif
ifdef HAVE_ANDROID
	cd $< && echo "using gcc : android : clang++" > user-config.jam
	cd $< && echo ":" >> user-config.jam
	cd $< && echo "<archiver>${HOST}-ar" >> user-config.jam
	cd $< && echo "<compileflags>-DANDROID" >> user-config.jam
	cd $< && echo "<compileflags>-D__ANDROID__" >> user-config.jam
	cd $< && echo "<compileflags>-I$(ANDROID_TOOLCHAIN)/include/c++/4.9" >> user-config.jam
	cd $< && echo ";" >> user-config.jam
endif
ifdef HAVE_IOS
	$(APPLY) $(SRC)/boost/0001-Add-build-boost-for-iOS-script.patch
	cd $< && chmod +x build-boost-iOS-libc++
	cd $< && ./build-boost-iOS-libc++ \
		$(HOSTCONF) \
		$(HOSTVARS) \
		--boostVersion1=$(BOOST_VERSION_1) \
		--boostVersion2=$(BOOST_VERSION_2)
else
	cd $< && $(HOSTVARS) ./bootstrap.sh
	cd $< && $(HOSTVARS) ./b2 $(BOOST_B2_OPTS) install
endif
	touch $@
