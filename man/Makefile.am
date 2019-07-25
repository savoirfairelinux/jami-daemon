SECTION="8"

TEMPLATES=jamid.pod

man_MANS=jamid.8

POD2MAN=pod2man

EXTRA_DIST=$(man_MANS) $(TEMPLATES)

all: $(MANPAGES)

SUFFIXES=.pod .8

.pod.8:
	$(POD2MAN) --section=$(SECTION) --release=@VERSION@ --center "" $< > $@

clean-local:
	rm -rf *.8
