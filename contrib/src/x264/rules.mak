# x264

X264_GITURL := git://git.videolan.org/x264.git
X264_SNAPURL := http://git.videolan.org/?p=x264.git;a=snapshot;h=HEAD;sf=tgz
X262_GITURL := git://git.videolan.org/x262.git

ifeq ($(call need_pkg,"x264 >= 0.86"),)
PKGS_FOUND += x264
endif


X264CONF = --prefix="$(PREFIX)" \
           --host="$(HOST)"     \
           --enable-static      \
           --disable-avs        \
           --disable-lavf       \
           --disable-cli        \
           --disable-ffms

ifndef HAVE_WIN32
X264CONF += --enable-pic
else
X264CONF += --enable-win32thread
endif
ifdef HAVE_CROSS_COMPILE
X264CONF += --cross-prefix="$(HOST)-"
endif

$(TARBALLS)/x264-git.tar.xz:
	$(call download_git,$(X264_GITURL))

$(TARBALLS)/x264-git.tar.gz:
	$(call download,$(X264_SNAPURL))

.sum-x264: x264-git.tar.gz
	$(warning $@ not implemented)
	touch $@

x264: x264-git.tar.gz .sum-x264
	rm -Rf $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.x264: x264
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && $(MAKE) install
	touch $@
