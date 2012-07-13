SECTION="1"

TEMPLATES=sflphoned.pod

man_MANS=sflphoned.1

POD2MAN=pod2man

EXTRA_DIST=$(man_MANS) $(TEMPLATES)

all: $(MANPAGES)

SUFFIXES=.pod .1

.pod.1:
	$(POD2MAN) --section=$(SECTION) --release=@VERSION@ --center "" $< > $@

clean-local:
	rm -rf *.1
