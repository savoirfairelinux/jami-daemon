#IAX

IAX_URL = https://gitlab.savoirfairelinux.com/sflphone/libiax2.git

PKGS += iax

$(TARBALLS)/iax-git.tar.xz:
	$(call download_git,$(IAX_URL))

.sum-iax: iax-git.tar.xz
	$(warning $@ not implemented)
	touch $@

iax: iax-git.tar.xz .sum-iax
	$(UNPACK)
	$(MOVE)

.iax: iax
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
