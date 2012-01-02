# loction of installation directories
locale_installdir=$(DESTDIR)$(datadir)/locale
DOMAIN=sflphone-client-gnome


POFILES=\
	ar.po \
	ast.po \
	ca.po \
	da.po \
	de.po \
	el.po \
	es.po \
	fr.po \
	hu.po \
	it.po \
	ja.po \
	ko.po \
	nl.po \
	oc.po \
	pl.po \
	pt_BR.po \
	pt.po \
	ru.po \
	sv.po \
	te.po \
	tr.po \
	zh_CN.po \
	zh_HK.po \
	zh_TW.po

MOFILES=\
	ar.mo \
	ast.mo \
	ca.mo \
	da.mo \
	de.mo \
	el.mo \
	es.mo \
	fr.mo \
	hu.mo \
	it.mo \
	ja.mo \
	ko.mo \
	nl.mo \
	oc.mo \
	pl.mo \
	pt_BR.mo \
	pt.mo \
	ru.mo \
	sv.mo \
	te.mo \
	tr.mo \
	zh_CN.mo \
	zh_HK.mo \
	zh_TW.mo

MSGFMT=msgfmt

# necessary files
EXTRA_DIST= $(POFILES) $(MOFILES)

all:	$(MOFILES)	

# tells automake that toto.po makes toto.mo
SUFFIXES=.po .mo

.po.mo:
	$(MSGFMT) -o $@ $<

install-data-local: $(MOFILES)
	catalogs='$(MOFILES)'; \
	for i in $$catalogs; do \
		destdir=$(locale_installdir); \
		lang=`echo $$i | sed 's/\.mo//'`; \
		$(mkinstalldirs) $(locale_installdir)/$$lang/LC_MESSAGES; \
		$(INSTALL_DATA) $$i $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
		echo "installing $$i as $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	done	

uninstall-local:
	@catalogs='$(MOFILES)'; \
	for i in $$catalogs; do \
		destdir=$(locale_installdir); \
		lang=`echo $$i | sed 's/\.mo//'`; \
		rm -f $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
		echo "removing $$i as $(locale_installdir)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	done

clean-local:
	rm -f $(MOFILES)
