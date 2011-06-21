 
SUBDIRS = dbus-api
if ENABLE_DOXYGEN
    SUBDIRS += doxygen
endif

.PHONY: doc
doc:
	$(MAKE) -C doxygen doc
