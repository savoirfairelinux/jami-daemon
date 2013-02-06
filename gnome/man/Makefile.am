SECTION="1"

TEMPLATES=sflphone-client-gnome.pod

man_MANS = sflphone-client-gnome.1 \
           sflphone.1

POD2MAN=pod2man

EXTRA_DIST=$(man_MANS) $(TEMPLATES)

all: $(MANPAGES)

SUFFIXES=.pod .1

.pod.1:
	$(POD2MAN) --section=$(SECTION) --release=@VERSION@ --center "" $< > $@

clean-local:
	rm -rf *.1

-include $(top_srcdir)/git.mk
