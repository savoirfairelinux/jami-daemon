# Global variables

src=$(abs_top_srcdir)

JAMI_DIRTY_REPO ?= $(shell git diff-index --quiet HEAD 2>/dev/null || echo dirty)
JAMI_REVISION ?= $(shell git log -1 --format="%h" --abbrev=10 2>/dev/null)
JAMI_DATADIR ?= $(datadir)/jami

# Preprocessor flags
AM_CPPFLAGS += \
	-I$(src)/src \
	-I$(src)/src/config \
	-I$(src)/src/media \
	-I$(src)/test \
	-I$(src)/src/jami \
	$(SIP_CFLAGS) \
	-DPREFIX=\"$(prefix)\" \
	-DJAMI_DATADIR=\"$(JAMI_DATADIR)\" \
	-DENABLE_TRACE \
	-DJAMI_REVISION=\"$(JAMI_REVISION)\" \
	-DJAMI_DIRTY_REPO=\"$(JAMI_DIRTY_REPO)\" \
	-DPJSIP_MAX_PKT_LEN=8000 \
	-DPJ_AUTOCONF=1
