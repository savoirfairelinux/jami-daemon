data_installdir=$(datadir)/man/man1

PODILES=\
         sflphone-gtk.pod \
         sflphoned.pod

TFILES=\
	sflphone.1 \
	sflphoned.1 \
	sflphone-gtk.1

PODTOMAN=pod2man

EXTRA_DIST= $(PODFILES)

all:    $(TFILES)

SUFFIXES=.po .mo

sflphone.1:
	pod2man --section=1 --release="0.9.2" --center "" sflphone-gtk.pod > sflphone.1
sflphoned.1:
	pod2man --section=1 --release="0.9.2" --center "" sflphoned.pod > sflphoned.1
sflphone-gtk.1:
	pod2man --section=1 --release="0.9.2" --center "" sflphone-gtk.pod > sflphone-gtk.1

install-data-local:
	@catalogs='$(TFILES)'; \
        for i in $$catalogs; do \
                destdir=$(data_installdir); \
                $(mkinstalldirs) $(data_installdir); \
                $(INSTALL_DATA) $$i $(data_installdir); \
                echo "installing $$i...done"; \
        done    

uninstall-local:
	@catalogs='$(TFILES)'; \
        for i in $$catalogs; do \
                destdir=$(data_installdir); \
                rm -f $(data_installdir)/$$i; \
                echo "removing $$i "; \
        done

clean-local:
	rm -rf *.1
