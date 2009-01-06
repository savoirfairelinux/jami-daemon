data_installdir=$(datadir)/man/man1

SECTION="1"
RELEASE_VERSION="0.9.2"

TEMPLATES=\
         sflphone-gtk.pod \
         sflphoned.pod

MANPAGES=\
	sflphone.1 \
	sflphoned.1 \
	sflphone-gtk.1

POD2MAN=pod2man

EXTRA_DIST= $(MANPAGES)

all:    $(MANPAGES)

SUFFIXES=.pod .1

.pod.1:
	$(POD2MAN) --section=$(SECTION) --release=$(RELEASE_VERSION) --center "" $< > $@ 

install-data-local:
	@catalogs='$(MANPAGES)'; \
        for i in $$catalogs; do \
                destdir=$(data_installdir); \
                $(mkinstalldirs) $(data_installdir); \
                $(INSTALL_DATA) $$i $(data_installdir); \
                echo "installing $$i...done"; \
        done    

uninstall-local:
	@catalogs='$(MANPAGES)'; \
        for i in $$catalogs; do \
                destdir=$(data_installdir); \
                rm -f $(data_installdir)/$$i; \
                echo "removing $$i "; \
        done

clean-local:
	rm -rf *.1
