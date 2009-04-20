SECTION="1"

TEMPLATES=\
         sflphone-gtk.pod \
         sflphoned.pod

man_MANS = sflphoned.1 \
			sflphone-gtk.1 \
			sflphone.1 

POD2MAN=pod2man

EXTRA_DIST= $(man_MANS)

all:    $(MANPAGES)

SUFFIXES=.pod .1

.pod.1:
	$(POD2MAN) --section=$(SECTION) --release=@VERSION@ --center "" $< > $@ 


#install-data-local:
#	@catalogs='$(man_MANS)'; \
 #       for i in $$catalogs; do \
  #              destdir=$(data_installdir); \
   #             $(mkinstalldirs) $(data_installdir); \
    #            $(INSTALL_DATA) $$i $(data_installdir); \
     #           echo "installing $$i...done"; \
      #  done    

#uninstall-local:
#	@catalogs='$(MANPAGES)'; \
 #       for i in $$catalogs; do \
  #              destdir=$(data_installdir); \
   #             rm -f $(data_installdir)/$$i; \
    #            echo "removing $$i "; \
     #   done

clean-local:
	rm -rf *.1
