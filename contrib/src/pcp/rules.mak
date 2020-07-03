# pcp
PCP_VERSION := c4d9bb40c4c62b28f57e209f55973a7cb09b6cc3
PCP_URL := https://github.com/libpcp/pcp/archive/$(PCP_VERSION).tar.gz

PKGS += pcp

ifeq ($(call need_pkg,'pcp'),)
PKGS_FOUND += pcp
endif

$(TARBALLS)/pcp-$(PCP_VERSION).tar.gz:
	$(call download,$(PCP_URL))

.sum-pcp: pcp-$(PCP_VERSION).tar.gz

pcp: pcp-$(PCP_VERSION).tar.gz .sum-pcp
	$(UNPACK)
	$(APPLY) $(SRC)/pcp/0001-sock_ntop.c-fix-format-truncation-error.patch
	$(MOVE)

.pcp: pcp
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure --disable-server --disable-app $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
