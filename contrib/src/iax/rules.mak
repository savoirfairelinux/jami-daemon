#IAX

IAX_VERSION = 0e5980f1d78ce462e2d1ed6bc39ff35c8341f201
IAX_URL = https://gitlab.savoirfairelinux.com/sflphone/libiax2/repository/archive.tar.gz?ref=$(IAX_VERSION)

PKGS += iax

$(TARBALLS)/iax-git.tar.gz:
	$(call download,$(IAX_URL))

.sum-iax: iax-git.tar.gz
	$(warning $@ not implemented)
	touch $@

iax: iax-git.tar.gz .sum-iax
	$(UNPACK)
	mv libiax2-$(IAX_VERSION)-$(IAX_VERSION) $@
	touch $@


.iax: iax
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
