# miniupnpc

MINIUPNPC_VERSION := 1.9
MINIUPNPC_URL := http://miniupnp.free.fr/files/download.php?file=miniupnpc-$(MINIUPNPC_VERSION).tar.gz

PKGS += miniupnpc
ifeq ($(call need_pkg,"miniupnpc >= 1.6"),)
PKGS_FOUND += miniupnpc
endif

$(TARBALLS)/miniupnpc-$(MINIUPNPC_VERSION).tar.gz:
	$(call download,$(MINIUPNPC_URL))

.sum-miniupnpc: miniupnpc-$(MINIUPNPC_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

miniupnpc: miniupnpc-$(MINIUPNPC_VERSION).tar.gz .sum-miniupnpc
	$(UNPACK)
	$(APPLY) $(SRC)/miniupnpc/makefile.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(MOVE)

.miniupnpc: miniupnpc
	cd $< && INSTALLPREFIX=$(PREFIX) $(MAKE) install-static
	touch $@
